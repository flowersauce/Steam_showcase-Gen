#include "showcase_processor.h"
#include <format>
#include <fstream>
#include <iostream>
#include <opencv2/opencv.hpp>
#include <ranges>
#include "app_text.hpp"

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace SteamShowcaseGen
{
	// 日志文件路径
	static const std::string LOG_DIR  = "log";
	static const std::string LOG_FILE = LOG_DIR + "/debug.log";

	// 辅助函数：写日志到文件和控制台
	static void log_init(const std::string &msg)
	{
		std::cerr << msg << std::endl;

		if (!std::filesystem::exists(LOG_DIR))
		{
			if (!std::filesystem::create_directories(LOG_DIR))
			{
				return;
			}
		}

		if (std::ofstream log_file(LOG_FILE, std::ios::app); log_file.is_open())
		{
			log_file << msg << std::endl;
		}
	}

	ShowcaseProcessor::ShowcaseProcessor() = default;
	ShowcaseProcessor::~ShowcaseProcessor()
	{
		stop_task();
	}

	// 初始化 GIF 编码器
	bool
	ShowcaseProcessor::init_encoder(EncoderState &state, const std::string &filename, const int width, const int height, const int fps, const int quality_mode)
	{
		int			sws_flags;
		std::string sws_name;

		switch (quality_mode)
		{
			case 0:
				sws_flags = SWS_POINT;
				sws_name  = "SWS_POINT (像素化, 最快)";
				break;
			case 3:
				sws_flags = SWS_LANCZOS;
				sws_name  = "SWS_LANCZOS (高质量, 最慢)";
				break;
			case 1:
			case 2:
			default:
				sws_flags = SWS_BICUBIC;
				sws_name  = "SWS_BICUBIC (平衡)";
				break;
		}

		log_init(std::format("[Init] Video encoder - SWS flags: {}", sws_name));

		if (avformat_alloc_output_context2(&state.fmt_ctx, nullptr, "gif", filename.c_str()) < 0 || !state.fmt_ctx)
		{
			log_init("[Init] ERROR: avformat_alloc_output_context2 failed");
			return false;
		}

		const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_GIF);
		if (!codec)
		{
			log_init("[Init] ERROR: GIF codec not found");
			return false;
		}

		state.stream = avformat_new_stream(state.fmt_ctx, codec);
		if (!state.stream)
			return false;

		state.codec_ctx = avcodec_alloc_context3(codec);
		if (!state.codec_ctx)
			return false;

		state.codec_ctx->width	   = width;
		state.codec_ctx->height	   = height;
		state.codec_ctx->time_base = {1, fps};
		state.codec_ctx->pix_fmt   = AV_PIX_FMT_RGB8;

		if (quality_mode >= 2)
		{
			av_opt_set(state.codec_ctx->priv_data, "diff", "1", 0);
		}

		if (avcodec_open2(state.codec_ctx, codec, nullptr) < 0)
			return false;

		avcodec_parameters_from_context(state.stream->codecpar, state.codec_ctx);

		if (!(state.fmt_ctx->oformat->flags & AVFMT_NOFILE))
		{
			if (avio_open(&state.fmt_ctx->pb, filename.c_str(), AVIO_FLAG_WRITE) < 0)
			{
				return false;
			}
		}

		if (avformat_write_header(state.fmt_ctx, nullptr) < 0)
		{
			return false;
		}

		state.frame = av_frame_alloc();
		if (!state.frame)
		{
			return false;
		}
		state.frame->format = state.codec_ctx->pix_fmt;
		state.frame->width	= width;
		state.frame->height = height;
		if (av_frame_get_buffer(state.frame, 32) < 0)
		{
			return false;
		}

		state.sws_ctx = sws_getContext(width, height, AV_PIX_FMT_BGR24, width, height, AV_PIX_FMT_RGB8, sws_flags, nullptr, nullptr, nullptr);

		state.frame_count = 0;
		return true;
	}

	// 核心改进：修复了重复分支与性能问题的编码函数
	void ShowcaseProcessor::encode_raw_frame(const EncoderState &state, const AVFrame *raw_frame)
	{
		if (!state.codec_ctx)
		{
			return;
		}

		// 1. 发送帧
		if (const int ret = avcodec_send_frame(state.codec_ctx, raw_frame); ret < 0)
		{
			return;
		}

		// 2. 预先分配一次 Packet，用于在循环中复用
		AVPacket *pkt = av_packet_alloc();
		if (!pkt)
		{
			return;
		}

		// 3. 循环接收所有编码好的包。只要返回 0 说明有新数据
		while (avcodec_receive_packet(state.codec_ctx, pkt) == 0)
		{
			// 时间戳转换
			av_packet_rescale_ts(pkt, state.codec_ctx->time_base, state.stream->time_base);
			pkt->stream_index = state.stream->index;

			// 写入封装层
			av_interleaved_write_frame(state.fmt_ctx, pkt);

			// 重要：清除 packet 的 buffer 引用，以便下一次循环复用结构体
			av_packet_unref(pkt);
		}

		// 4. 处理完毕，彻底释放
		av_packet_free(&pkt);
	}

	void ShowcaseProcessor::push_frame(EncoderState &state, const cv::Mat &cv_frame, const int height)
	{
		if (!state.codec_ctx || !state.sws_ctx || !state.frame)
		{
			return;
		}

		const uint8_t *src_slice[]	= {cv_frame.data};
		const int	   src_stride[] = {static_cast<int>(cv_frame.step)};

		if (av_frame_make_writable(state.frame) < 0)
		{
			return;
		}

		sws_scale(state.sws_ctx, src_slice, src_stride, 0, height, state.frame->data, state.frame->linesize);

		state.frame->pts = state.frame_count++;
		encode_raw_frame(state, state.frame);
	}

	void ShowcaseProcessor::finish_encoder(EncoderState &state)
	{
		if (!state.fmt_ctx)
		{
			return;
		}

		if (state.codec_ctx)
		{
			encode_raw_frame(state, nullptr);
		}

		av_write_trailer(state.fmt_ctx);

		if (!(state.fmt_ctx->oformat->flags & AVFMT_NOFILE))
		{
			avio_closep(&state.fmt_ctx->pb);
		}

		if (state.codec_ctx)
		{
			avcodec_free_context(&state.codec_ctx);
			state.codec_ctx = nullptr;
		}
		if (state.frame)
		{
			av_frame_free(&state.frame);
			state.frame = nullptr;
		}
		if (state.sws_ctx)
		{
			sws_freeContext(state.sws_ctx);
			state.sws_ctx = nullptr;
		}

		avformat_free_context(state.fmt_ctx);
		state.fmt_ctx = nullptr;
	}

	bool ShowcaseProcessor::apply_steam_hex_hack(const std::filesystem::path &file_path)
	{
		std::fstream file(file_path, std::ios::binary | std::ios::in | std::ios::out);
		if (!file.is_open())
		{
			return false;
		}
		file.seekg(-1, std::ios::end);
		char last = 0;
		if (file.read(&last, 1) && static_cast<unsigned char>(last) == 0x3B)
		{
			file.seekp(-1, std::ios::end);
			// 修改 0x3B 为 0x21 以欺骗 Steam 长度检测
			constexpr char next = 0x21;
			file.write(&next, 1);
			file.close();
			return true;
		}
		file.close();
		return false;
	}

	void ShowcaseProcessor::start_task(
		const std::filesystem::path &source_path, const std::filesystem::path &output_dir, int sampling_rate, int quality_mode, const UpdateCallback &on_update)
	{
		stop_task();
		worker_thread_ = std::jthread([this, source_path, output_dir, sampling_rate, quality_mode, on_update](const std::stop_token &st)
									  { this->run_internal(st, source_path, output_dir, sampling_rate, quality_mode, on_update); });
	}

	void ShowcaseProcessor::stop_task()
	{
		if (worker_thread_.joinable())
		{
			worker_thread_.request_stop();
			worker_thread_.join();
		}
	}

	void ShowcaseProcessor::run_internal(const std::stop_token		 &st,
										 const std::filesystem::path &source_path,
										 const std::filesystem::path &output_dir,
										 const int					  sampling_rate,
										 const int					  quality_mode,
										 const UpdateCallback		 &on_update)
	{
		is_processing_.store(true);
		namespace text = SteamShowcaseGen::AppText;

		if (!std::filesystem::exists(output_dir))
		{
			std::filesystem::create_directories(output_dir);
		}

		std::ofstream log_clear(LOG_FILE, std::ios::trunc);
		log_clear << "=== Steam Showcase Gen Debug Log ===" << std::endl;
		log_clear.close();

		if (on_update)
		{
			on_update(text::LOG_STARTING);
		}

		std::string ext = source_path.extension().string();
		std::ranges::transform(ext, ext.begin(), ::tolower);

		// 处理图片
		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".webp")
		{
			cv::Mat img = cv::imread(source_path.string(), cv::IMREAD_COLOR);
			if (img.empty())
			{
				if (on_update)
				{
					on_update(text::ERR_OPEN_FAILED);
				}
				is_processing_.store(false);
				return;
			}

			double aspect_ratio = static_cast<double>(img.rows) / img.cols;
			int	   target_h		= static_cast<int>(STEAM_SHOWCASE_WIDTH * aspect_ratio);
			int	   inter_flag	= (quality_mode >= 2) ? cv::INTER_AREA : cv::INTER_LINEAR;

			cv::Mat resized;
			cv::resize(img, resized, cv::Size(STEAM_SHOWCASE_WIDTH, target_h), 0, 0, inter_flag);

			for (int i = 0; i < SLICE_COUNT; ++i)
			{
				int x = i * (SLICE_WIDTH + GAP_WIDTH);
				if (x + SLICE_WIDTH > resized.cols)
				{
					break;
				}
				cv::Rect roi(x, 0, SLICE_WIDTH, target_h);
				auto	 p = output_dir / std::format("slice_{}.gif", i + 1);
				cv::imwrite(p.string(), resized(roi));
				apply_steam_hex_hack(p);
			}
			if (on_update)
			{
				on_update(std::format("{}{}", text::LOG_FINISHED, output_dir.string()));
			}
			is_processing_.store(false);
			return;
		}

		// 处理视频
		cv::VideoCapture cap(source_path.string());
		if (!cap.isOpened())
		{
			if (on_update)
			{
				on_update(text::ERR_OPEN_FAILED);
			}
			is_processing_.store(false);
			return;
		}

		const double fps		= cap.get(cv::CAP_PROP_FPS);
		const int	 divisor	= 11 - sampling_rate;
		const int	 target_fps = std::max(1, static_cast<int>((fps > 0 ? fps : 30) / divisor));
		const int	 target_h	= static_cast<int>(STEAM_SHOWCASE_WIDTH * (cap.get(cv::CAP_PROP_FRAME_HEIGHT) / cap.get(cv::CAP_PROP_FRAME_WIDTH)));

		std::vector<EncoderState>		   encoders(SLICE_COUNT);
		std::vector<std::filesystem::path> out_paths;

		for (int i = 0; i < SLICE_COUNT; ++i)
		{
			auto p = output_dir / std::format("slice_{}.gif", i + 1);
			out_paths.push_back(p);
			if (!init_encoder(encoders[i], p.string(), SLICE_WIDTH, target_h, target_fps, quality_mode))
			{
				for (int j = 0; j <= i; ++j)
				{
					finish_encoder(encoders[j]);
				}
				is_processing_.store(false);
				return;
			}
		}

		cv::Mat frame, resized;
		int		frame_idx = 0, processed_cnt = 0;
		int		inter_flag = (quality_mode >= 2) ? cv::INTER_AREA : cv::INTER_LINEAR;

		while (cap.read(frame))
		{
			if (st.stop_requested())
			{
				break;
			}
			if (frame.empty() || frame_idx++ % divisor != 0)
			{
				continue;
			}

			cv::resize(frame, resized, cv::Size(STEAM_SHOWCASE_WIDTH, target_h), 0, 0, inter_flag);
			for (int i = 0; i < SLICE_COUNT; ++i)
			{
				if (int x = i * (SLICE_WIDTH + GAP_WIDTH); x + SLICE_WIDTH <= resized.cols)
				{
					push_frame(encoders[i], resized(cv::Rect(x, 0, SLICE_WIDTH, target_h)).clone(), target_h);
				}
			}
			if (++processed_cnt % 10 == 0 && on_update)
				on_update(std::format("{}{}", text::LOG_ENCODING, processed_cnt));
		}

		for (auto &e: encoders)
		{
			finish_encoder(e);
		}
		if (!st.stop_requested() && processed_cnt > 0)
		{
			for (const auto &p: out_paths)
			{
				apply_steam_hex_hack(p);
			}
			if (on_update)
			{
				on_update(std::format("{}{}", text::LOG_FINISHED, output_dir.string()));
			}
		}
		is_processing_.store(false);
	}
} // namespace SteamShowcaseGen
