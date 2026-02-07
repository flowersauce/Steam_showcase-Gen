#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

// OpenCV
#include <opencv2/opencv.hpp>

// FFmpeg C API
extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace fs = std::filesystem;
using namespace cv;

// ==========================================
//                 配置参数
// ==========================================
constexpr int STEAM_SHOWCASE_WIDTH = 766;
constexpr int SLICE_WIDTH		   = 150;
constexpr int GAP_WIDTH			   = 4;
constexpr int SLICE_COUNT		   = 5;

// --- 新增功能配置 ---

// 帧跳跃：0 = 不跳帧, 1 = 每2帧取1帧, 2 = 每3帧取1帧...
// 建议：如果源视频是 60fps，建议设置为 1 或 2，将其降至 30fps 或 20fps 以减小体积。
constexpr int FRAME_SKIP = 3;

// 质量等级 (缩放算法)
// 0: Fast (Nearest Neighbor) - 速度最快，像素化，文件略小
// 1: Medium (Bilinear)       - 平衡
// 2: High (Bicubic)          - 画质较好
// 3: Best (Lanczos)          - 画质最好，计算最慢
constexpr int QUALITY_LEVEL = 1;

// ==========================================

// Steam Hex Hack 处理函数
// 来源: main.cpp_2 , PDF
// 功能: 将 GIF 文件结尾的 3B 修改为 21，防止 Steam 压缩黑边
void apply_steam_hex_hack(const std::string &filepath)
{
	std::fstream file(filepath, std::ios::binary | std::ios::in | std::ios::out);
	if (!file.is_open())
	{
		std::cerr << "[Hex Hack Error] Cannot open file: " << filepath << std::endl;
		return;
	}

	// 移动到文件末尾倒数第1个字节
	file.seekg(-1, std::ios::end);
	if (file.tellg() <= 0)
		return;

	char lastByte;
	file.read(&lastByte, 1);

	// 检查并修改
	if (static_cast<unsigned char>(lastByte) == 0x3B)
	{
		file.seekp(-1, std::ios::end);
		constexpr char newByte = 0x21;
		file.write(&newByte, 1);
		std::cout << "[Hex Hack] Applied (3B -> 21) to: " << fs::path(filepath).filename() << std::endl;
	}
	else
	{
		// 如果已经是 21 或者其他值，则不处理
		// std::cout << "[Hex Hack] Skipped (End byte is " << std::hex << (int)(unsigned char)lastByte << "): " << fs::path(filepath).filename() << std::endl;
	}

	file.close();
}

class GifEncoder
{
public:
	GifEncoder(const std::string &filename, const int width, const int height, const int fps, const int quality_mode)
		: width_(width)
		, height_(height)
	{

		// 1. 设置缩放算法 flag
		int sws_flags = SWS_BILINEAR;
		switch (quality_mode)
		{
			case 0:
				sws_flags = SWS_POINT;
				break;
			case 1:
				sws_flags = SWS_BILINEAR;
				break;
			case 2:
				sws_flags = SWS_BICUBIC;
				break;
			case 3:
				sws_flags = SWS_LANCZOS;
				break;
			default:
				sws_flags = SWS_BICUBIC;
				break;
		}

		// 2. 初始化 FFmpeg 上下文
		avformat_alloc_output_context2(&fmt_ctx, nullptr, "gif", filename.c_str());
		if (!fmt_ctx)
			return;

		const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_GIF);
		if (!codec)
			return;

		stream	  = avformat_new_stream(fmt_ctx, codec);
		codec_ctx = avcodec_alloc_context3(codec);

		codec_ctx->width	 = width;
		codec_ctx->height	 = height;
		codec_ctx->time_base = {1, fps}; // 这里的 FPS 应该是处理后的 FPS
		codec_ctx->pix_fmt	 = AV_PIX_FMT_RGB8;

		if (avcodec_open2(codec_ctx, codec, nullptr) < 0)
			return;
		avcodec_parameters_from_context(stream->codecpar, codec_ctx);

		if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
		{
			avio_open(&fmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE);
		}

		avformat_write_header(fmt_ctx, nullptr);

		frame		  = av_frame_alloc();
		frame->format = codec_ctx->pix_fmt;
		frame->width  = width;
		frame->height = height;
		av_frame_get_buffer(frame, 32);

		// 初始化 SWS Context (BGR -> RGB8)
		sws_ctx = sws_getContext(width, height, AV_PIX_FMT_BGR24, width, height, AV_PIX_FMT_RGB8, sws_flags | SWS_DITHER_BAYER, nullptr, nullptr, nullptr);
		// SWS_DITHER_BAYER 有助于减少 GIF 的色带问题
	}

	~GifEncoder()
	{
		finish();
	}

	void push_frame(const cv::Mat &cv_frame)
	{
		if (!codec_ctx || !sws_ctx)
			return;

		// 确保输入 Mat 为 BGR 格式
		const uint8_t *srcSlice[]  = {cv_frame.data};
		const int	   srcStride[] = {static_cast<int>(cv_frame.step)};

		av_frame_make_writable(frame);

		// 转换并缩放
		sws_scale(sws_ctx, srcSlice, srcStride, 0, height_, frame->data, frame->linesize);

		frame->pts = frame_count++;
		encode(frame);
	}

	void finish()
	{
		if (finished)
			return;
		encode(nullptr); // Flush
		if (fmt_ctx)
		{
			av_write_trailer(fmt_ctx);
			if (!(fmt_ctx->oformat->flags & AVFMT_NOFILE))
				avio_closep(&fmt_ctx->pb);
			avformat_free_context(fmt_ctx);
		}
		if (codec_ctx)
			avcodec_free_context(&codec_ctx);
		if (frame)
			av_frame_free(&frame);
		if (sws_ctx)
			sws_freeContext(sws_ctx);
		finished = true;
	}

private:
	void encode(AVFrame *frame) const
	{
		int ret = avcodec_send_frame(codec_ctx, frame);
		if (ret < 0)
			return;
		while (ret >= 0)
		{
			AVPacket *pkt = av_packet_alloc();
			ret			  = avcodec_receive_packet(codec_ctx, pkt);
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
			{
				av_packet_free(&pkt);
				break;
			}
			else if (ret < 0)
			{
				av_packet_free(&pkt);
				return;
			}
			av_packet_rescale_ts(pkt, codec_ctx->time_base, stream->time_base);
			pkt->stream_index = stream->index;
			av_interleaved_write_frame(fmt_ctx, pkt);
			av_packet_free(&pkt);
		}
	}

	AVFormatContext *fmt_ctx   = nullptr;
	AVCodecContext	*codec_ctx = nullptr;
	AVStream		*stream	   = nullptr;
	AVFrame			*frame	   = nullptr;
	SwsContext		*sws_ctx   = nullptr;
	int				 width_, height_;
	int				 frame_count = 0;
	bool			 finished	 = false;
};

int main()
{
	// 1. 定义路径
	std::string input_dir_str  = "target_resource";
	std::string output_dir_str = "output";
	fs::path	input_dir(input_dir_str);
	fs::path	output_dir(output_dir_str);

	if (!fs::exists(output_dir))
		fs::create_directories(output_dir);

	// 2. 寻找视频文件
	fs::path source_path;
	bool	 found = false;
	for (const auto &entry: fs::directory_iterator(input_dir))
	{
		std::string ext = entry.path().extension().string();
		// 简单转小写检查
		std::ranges::transform(ext, ext.begin(), ::tolower);
		if (ext == ".mp4" || ext == ".avi" || ext == ".mov" || ext == ".mkv")
		{
			source_path = entry.path();
			found		= true;
			break;
		}
	}

	if (!found)
	{
		std::cerr << "[Error] No video files found in " << input_dir << std::endl;
		return 1;
	}

	std::cout << "[Processing] Source: " << source_path << std::endl;

	// 3. 打开视频流
	VideoCapture cap(source_path.string());
	if (!cap.isOpened())
	{
		std::cerr << "[Error] Failed to open video." << std::endl;
		return 1;
	}

	double original_fps = cap.get(CAP_PROP_FPS);
	if (original_fps <= 0)
		original_fps = 30;

	// 计算新的 FPS (根据跳帧)
	// 比如 60fps, FRAME_SKIP=1 (每2帧取1帧), 实际输出就是 30fps
	int target_fps = static_cast<int>(original_fps / (FRAME_SKIP + 1));
	if (target_fps < 1)
		target_fps = 1;

	double width  = cap.get(CAP_PROP_FRAME_WIDTH);
	double height = cap.get(CAP_PROP_FRAME_HEIGHT);

	// 计算目标高度
	double aspect_ratio	 = height / width;
	int	   target_height = static_cast<int>(STEAM_SHOWCASE_WIDTH * aspect_ratio);

	std::cout << "[Info] Original: " << width << "x" << height << " @ " << original_fps << " fps" << std::endl;
	std::cout << "[Info] Target:   " << STEAM_SHOWCASE_WIDTH << "x" << target_height << " @ " << target_fps << " fps (Skip: " << FRAME_SKIP
			  << ", Quality: " << QUALITY_LEVEL << ")" << std::endl;

	// 4. 初始化编码器
	std::vector<std::unique_ptr<GifEncoder>> encoders;
	std::vector<std::string>				 output_files;

	for (int i = 0; i < SLICE_COUNT; ++i)
	{
		std::string filename = "slice_" + std::to_string(i + 1) + ".gif";
		fs::path	out_path = output_dir / filename;
		output_files.push_back(out_path.string());

		encoders.push_back(std::make_unique<GifEncoder>(out_path.string(), SLICE_WIDTH, target_height, target_fps, QUALITY_LEVEL));
	}

	// 5. 逐帧处理
	Mat frame, resized_frame;
	int frame_index		= 0;
	int processed_count = 0;

	// 选择 OpenCV 的插值算法
	// 如果想要高质量缩小，INTER_AREA 最好；如果追求速度，INTER_LINEAR
	int cv_inter_flag = (QUALITY_LEVEL >= 2) ? INTER_AREA : INTER_LINEAR;

	while (true)
	{
		if (bool ret = cap.read(frame); !ret || frame.empty())
			break;

		// ================= 帧跳跃逻辑 =================
		if (frame_index % (FRAME_SKIP + 1) != 0)
		{
			frame_index++;
			continue; // 跳过此帧
		}
		// ============================================

		// 缩放
		resize(frame, resized_frame, Size(STEAM_SHOWCASE_WIDTH, target_height), 0, 0, cv_inter_flag);

		// 切片并写入
		for (int i = 0; i < SLICE_COUNT; ++i)
		{
			int x = i * (SLICE_WIDTH + GAP_WIDTH);
			if (x + SLICE_WIDTH > resized_frame.cols)
				continue;

			Rect roi(x, 0, SLICE_WIDTH, target_height);
			Mat	 slice = resized_frame(roi);

			encoders[i]->push_frame(slice);
		}

		processed_count++;
		frame_index++;

		if (processed_count % 10 == 0)
			std::cout << "\r[Encoding] Encoded Frames: " << processed_count << std::flush;
	}

	std::cout << std::endl << "[Done] Encoding finished." << std::endl;

	// 6. 收尾 & Hex Hack
	for (auto &enc: encoders)
		enc->finish();

	std::cout << "[Hex Hack] Applying Steam Hex Hack (3B -> 21)..." << std::endl;
	for (const auto &file: output_files)
	{
		apply_steam_hex_hack(file);
	}

	return 0;
}
