#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

// OpenCV
#include <opencv2/core/utils/logger.hpp>
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

// FTXUI
#include "ftxui/component/component.hpp"
#include "ftxui/component/component_options.hpp"
#include "ftxui/component/event.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "ftxui/dom/elements.hpp"

namespace fs = std::filesystem;
using namespace ftxui;
using namespace cv;

// ==========================================
//           全局静音类 (Silencer)
// ==========================================
class ScopedStderrSilencer
{
public:
	ScopedStderrSilencer()
	{
		old_buffer = std::cerr.rdbuf();
		std::cerr.rdbuf(null_buffer.rdbuf());
	}
	~ScopedStderrSilencer()
	{
		std::cerr.rdbuf(old_buffer);
	}

private:
	std::stringstream null_buffer;
	std::streambuf	 *old_buffer;
};

// ==========================================
//              核心逻辑区 (Backend)
// ==========================================

constexpr int STEAM_SHOWCASE_WIDTH = 766;
constexpr int SLICE_WIDTH		   = 150;
constexpr int GAP_WIDTH			   = 4;
constexpr int SLICE_COUNT		   = 5;

void apply_steam_hex_hack(const std::string &filepath)
{
	std::fstream file(filepath, std::ios::binary | std::ios::in | std::ios::out);
	if (!file.is_open())
		return;

	file.seekg(-1, std::ios::end);
	if (file.tellg() <= 0)
		return;

	char lastByte;
	file.read(&lastByte, 1);

	if (static_cast<unsigned char>(lastByte) == 0x3B)
	{
		file.seekp(-1, std::ios::end);
		constexpr char newByte = 0x21;
		file.write(&newByte, 1);
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

		av_log_set_level(AV_LOG_QUIET);

		int sws_flags;
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

		avformat_alloc_output_context2(&fmt_ctx, nullptr, "gif", filename.c_str());
		if (!fmt_ctx)
			return;

		const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_GIF);
		if (!codec)
			return;

		stream				 = avformat_new_stream(fmt_ctx, codec);
		codec_ctx			 = avcodec_alloc_context3(codec);
		codec_ctx->width	 = width;
		codec_ctx->height	 = height;
		codec_ctx->time_base = {1, fps};
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

		sws_ctx = sws_getContext(width, height, AV_PIX_FMT_BGR24, width, height, AV_PIX_FMT_RGB8, sws_flags | SWS_DITHER_BAYER, nullptr, nullptr, nullptr);
	}

	~GifEncoder()
	{
		finish();
	}

	void push_frame(const cv::Mat &cv_frame)
	{
		if (!codec_ctx || !sws_ctx)
			return;
		const uint8_t *srcSlice[]  = {cv_frame.data};
		const int	   srcStride[] = {static_cast<int>(cv_frame.step)};
		av_frame_make_writable(frame);
		sws_scale(sws_ctx, srcSlice, srcStride, 0, height_, frame->data, frame->linesize);
		frame->pts = frame_count++;
		encode(frame);
	}

	void finish()
	{
		if (finished)
			return;
		encode(nullptr);
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
	void encode(const AVFrame *enc_frame) const
	{
		int ret = avcodec_send_frame(codec_ctx, enc_frame);
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

bool is_supported_file(const fs::path &path)
{
	static const std::vector<std::string> extensions = {".mp4", ".avi", ".mov", ".mkv", ".png", ".jpg", ".jpeg"};
	std::string							  ext		 = path.extension().string();
	std::ranges::transform(ext, ext.begin(), ::tolower);
	for (const auto &e: extensions)
	{
		if (ext == e)
			return true;
	}
	return false;
}

// ==========================================
//               UI 界面区 (Frontend)
// ==========================================

int main()
{
	ScopedStderrSilencer global_stderr_silencer;
	cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);

	auto screen = ScreenInteractive::Fullscreen();

	// --- 状态变量 ---
	std::string				 src_dir_str = "target_resource";
	std::string				 out_dir_str = "output";
	std::vector<std::string> file_list_entries;
	int						 selected_file_index = 0;
	int						 sampling_slider	 = 10;
	int						 quality_selected	 = 2;

	int						 tab_index	 = 0;
	std::vector<std::string> tab_entries = {" [主页] ", " [关于] "};

	std::string		  current_log	= "就绪";
	std::atomic<bool> is_processing = false;
	int				  spinner_index = 0;

	// --- 组件构建 ---
	Component tab_toggle	= Toggle(&tab_entries, &tab_index);
	Component input_src_dir = Input(&src_dir_str, "源文件目录路径");
	Component input_out_dir = Input(&out_dir_str, "输出保存路径");

	MenuOption menu_option;
	menu_option.on_enter = [&]
	{
		if (selected_file_index >= 0 && selected_file_index < file_list_entries.size())
			current_log = "选中: " + file_list_entries[selected_file_index];
	};
	Component file_menu = Menu(&file_list_entries, &selected_file_index, menu_option);

	auto scan_action = [&]
	{
		current_log = "正在扫描...";
		file_list_entries.clear();
		selected_file_index = 0;
		try
		{
			if (const fs::path dir_path(src_dir_str); fs::exists(dir_path) && fs::is_directory(dir_path))
			{
				for (const auto &entry: fs::directory_iterator(dir_path))
				{
					if (entry.is_regular_file() && is_supported_file(entry.path()))
					{
						file_list_entries.push_back(entry.path().filename().string());
					}
				}
				if (file_list_entries.empty())
				{
					current_log = "未找到支持的文件";
					file_list_entries.push_back("<无文件>");
				}
				else
				{
					current_log = "扫描完成，发现 " + std::to_string(file_list_entries.size()) + " 个文件";
				}
			}
			else
			{
				current_log = "目录不存在";
				file_list_entries.push_back("<无效目录>");
			}
		}
		catch (const std::exception &e)
		{
			current_log = "错误: " + std::string(e.what());
		}
	};
	Component btn_scan = Button("扫描", scan_action, ButtonOption::Ascii());

	Component slider_skip = Slider("", &sampling_slider, 1, 10, 1);

	// ================= 质量选项卡设置 =================

	// 1. 字符串优化：增加空格以实现视觉对齐
	std::vector<std::string> quality_labels = {
		"快速   (最近邻)", // 补齐空格
		"平衡   (双线性)", // 补齐空格
		"高质量 (双三次)",
		"极高   (Lanczos)" // 补齐空格
	};

	// 2. 自定义 MenuOption 实现炫酷颜色
	MenuOption option_quality;
	option_quality.entries_option.transform = [](const EntryState &s)
	{
		// 根据 index 决定颜色：红 -> 黄 -> 绿 -> 青
		Color c = Color::White;
		switch (s.index)
		{
			case 0:
				c = Color::RedLight;
				break; // 快速 (质量低)
			case 1:
				c = Color::Yellow;
				break; // 平衡
			case 2:
				c = Color::Green;
				break; // 高质量
			case 3:
				c = Color::Cyan;
				break; // 极高
		}

		// 构建前缀图标 (选中是实心圆，未选中是空心圆)
		std::string prefix_icon = s.active ? "◉ " : "○ ";

		// 构建文本元素
		auto text_elem = text(s.label) | color(c);
		if (s.active)
			text_elem |= bold; // 选中时加粗

		// 组合成一行
		auto res = hbox({text(prefix_icon) | color(Color::GrayLight), // 图标用淡灰色
						 text_elem});

		// 如果鼠标悬停/聚焦，加一个反色背景，看起来像个高亮条
		if (s.focused)
			res |= inverted;

		return res;
	};

	Component quality_menu = Menu(&quality_labels, &quality_selected, option_quality);

	// ================= 按钮逻辑区 =================

	auto start_action = [&]
	{
		if (file_list_entries.empty() || file_list_entries[0] == "<无文件>" || file_list_entries[0] == "<无效目录>")
		{
			current_log = "错误: 请先选择有效文件";
			return;
		}

		std::string filename = file_list_entries[selected_file_index];
		fs::path	src_file = fs::path(src_dir_str) / filename;
		fs::path	out_dir(out_dir_str);

		int divisor		 = 11 - sampling_slider;
		int quality_mode = quality_selected;

		is_processing = true;
		current_log	  = "启动处理任务...";

		std::thread(
			[=, &is_processing, &current_log, &screen]() mutable
			{
				ScopedStderrSilencer thread_silencer;
				try
				{
					if (!fs::exists(out_dir))
						fs::create_directories(out_dir);

					VideoCapture cap(src_file.string());
					if (!cap.isOpened())
					{
						current_log	  = "错误: 无法打开视频文件";
						is_processing = false;
						screen.Post(Event::Custom);
						return;
					}

					double fps = cap.get(CAP_PROP_FPS);
					if (fps <= 0)
						fps = 30;
					int target_fps = std::max(1, static_cast<int>(fps / divisor));

					double width		 = cap.get(CAP_PROP_FRAME_WIDTH);
					double height		 = cap.get(CAP_PROP_FRAME_HEIGHT);
					double aspect_ratio	 = height / width;
					int	   target_height = static_cast<int>(STEAM_SHOWCASE_WIDTH * aspect_ratio);

					std::vector<std::unique_ptr<GifEncoder>> encoders;
					std::vector<std::string>				 out_paths;

					for (int i = 0; i < SLICE_COUNT; ++i)
					{
						std::string fname = "slice_" + std::to_string(i + 1) + ".gif";
						fs::path	p	  = out_dir / fname;
						out_paths.push_back(p.string());
						encoders.push_back(std::make_unique<GifEncoder>(p.string(), SLICE_WIDTH, target_height, target_fps, quality_mode));
					}

					Mat frame, resized_frame;
					int frame_idx	  = 0;
					int processed_cnt = 0;
					int cv_inter_flag = (quality_mode >= 2) ? INTER_AREA : INTER_LINEAR;

					while (cap.read(frame))
					{
						if (frame.empty())
							break;
						if (frame_idx++ % divisor != 0)
							continue;

						resize(frame, resized_frame, Size(STEAM_SHOWCASE_WIDTH, target_height), 0, 0, cv_inter_flag);

						for (int i = 0; i < SLICE_COUNT; ++i)
						{
							int x = i * (SLICE_WIDTH + GAP_WIDTH);
							if (x + SLICE_WIDTH > resized_frame.cols)
								continue;
							Rect roi(x, 0, SLICE_WIDTH, target_height);
							encoders[i]->push_frame(resized_frame(roi));
						}
						processed_cnt++;
						if (processed_cnt % 10 == 0)
						{
							current_log = "正在编码... 已处理帧数: " + std::to_string(processed_cnt);
						}
					}

					for (auto &enc: encoders)
						enc->finish();

					current_log = "应用 Hex Hack...";
					for (const auto &p: out_paths)
						apply_steam_hex_hack(p);

					current_log = "任务完成! 输出目录: " + out_dir_str;
				}
				catch (const std::exception &e)
				{
					current_log = "异常: " + std::string(e.what());
				}
				is_processing = false;
				screen.Post(Event::Custom);
			})
			.detach();
	};

	// ================= 开始按钮自定义样式 =================
	ButtonOption start_btn_option;
	start_btn_option.transform = [&](const EntryState &s)
	{
		Element e;
		if (is_processing)
		{
			// 状态：正在生成中 (红色文字)
			e = text("[生成中]") | bold | color(Color::Red);
		}
		else
		{
			// 状态：空闲/就绪 (绿色文字)
			e = text("[开始生成]") | bold | color(Color::Green);

			// 仅在空闲状态下，且被选中(Focus)时显示反色背景
			if (s.focused)
				e |= inverted;
		}
		return e | center;
	};
	Component btn_start = Button("", start_action, start_btn_option);

	// --- 页面布局定义 ---
	auto resource_container = Container::Vertical({input_src_dir, btn_scan, file_menu});

	auto output_container = Container::Vertical({
		input_out_dir,
		slider_skip,
		quality_menu // Menu 组件
	});

	auto main_page_content = Container::Vertical({Container::Horizontal({resource_container, output_container}), Container::Vertical({btn_start})});

	auto about_container = Container::Vertical({});
	auto tab_container	 = Container::Tab({main_page_content, about_container}, &tab_index);

	auto main_layout = Container::Vertical({tab_toggle, tab_container})
		| CatchEvent(
						   [&](Event /*event*/)
						   {
							   if (is_processing)
								   return true;
							   return false;
						   });

	// --- 渲染器 ---
	auto renderer = Renderer(
		main_layout,
		[&]
		{
			auto title_bar = hbox({text(" STEAM 创意工坊展柜图像生成器 ") | bold | color(Color::CyanLight), filler(), tab_toggle->Render() | align_right})
				| borderRounded | color(Color::Default) | size(HEIGHT, EQUAL, 3);

			Element spinner_elem;
			if (is_processing)
			{
				spinner_elem = spinner(12, spinner_index) | bold | color(Color::Yellow) | size(WIDTH, EQUAL, 1);
			}
			else
			{
				spinner_elem = text("●") | color(Color::Green) | size(WIDTH, EQUAL, 1);
			}

			auto status_bar = hbox({spinner_elem | center | size(WIDTH, EQUAL, 3),
									text(current_log) | vcenter | flex,
									separator(),
									btn_start->Render() | size(WIDTH, EQUAL, 16) | center})
				| border | size(HEIGHT, EQUAL, 3);

			auto render_main_page = [&]
			{
				int			divisor		= 11 - sampling_slider;
				std::string suffix_text = (divisor == 1) ? "(N/A)" : ("(1/" + std::to_string(divisor) + ")");

				auto file_list_display =
					vbox({text(" 文件列表") | bold, separator(), hbox({text(" "), file_menu->Render() | vscroll_indicator | frame | flex}) | flex}) | flex;

				auto card_resource =
					vbox({text("资源选择") | bold | center,
						  separator(),
						  hbox({text(" 目录: "), input_src_dir->Render() | size(WIDTH, EQUAL, 30), filler(), btn_scan->Render()}) | size(HEIGHT, EQUAL, 1),
						  separator(),
						  file_list_display | flex})
					| border | flex;

				// 右侧布局
				auto card_output =
					vbox({text("输出设置") | bold | center,
						  separator(),
						  // 行 1
						  hbox(text(" 路径: "), input_out_dir->Render() | size(WIDTH, EQUAL, 30)) | size(HEIGHT, EQUAL, 1),
						  separator(),
						  // 行 2
						  hbox({text(" 采样率: "), slider_skip->Render() | flex, text(suffix_text) | size(WIDTH, EQUAL, 6)}) | size(HEIGHT, EQUAL, 1),
						  separator(),
						  // 行 3：缩放质量 (Menu)
						  text(" 缩放质量:") | bold,
						  separator(),
						  // 菜单渲染
						  hbox({text(" "), quality_menu->Render() | flex}) | flex})
					| border | flex;

				return vbox({hbox({card_resource, card_output}) | flex, status_bar});
			};

			auto render_about_page = [&]
			{
				return vbox({filler(),
							 text("关于本软件") | bold | hcenter | size(HEIGHT, EQUAL, 3),
							 separator(),
							 vbox({text("作者: Your Name / ID") | hcenter,
								   text("版本: v1.0.0") | hcenter,
								   text("") | hcenter,
								   text("技术栈致谢:") | hcenter | bold,
								   text("• OpenCV (图像处理/计算机视觉)") | hcenter,
								   text("• FFmpeg (视频解码/GIF编码)") | hcenter,
								   text("• FTXUI (终端用户界面)") | hcenter,
								   text("") | hcenter,
								   text("本工具用于生成 Steam 创意工坊长展柜所需的 GIF 切片，") | hcenter,
								   text("并自动应用 Hex Hack 以防止黑边压缩问题。") | hcenter})
								 | border | flex,
							 filler()});
			};

			Element content;
			if (tab_index == 0)
			{
				content = render_main_page();
			}
			else
			{
				content = render_about_page();
			}

			return vbox({title_bar, content | flex});
		});

	std::thread animation_thread(
		[&]
		{
			while (true)
			{
				if (is_processing)
				{
					spinner_index++;
					screen.Post(Event::Custom);
					std::this_thread::sleep_for(std::chrono::milliseconds(80));
				}
				else
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
			}
		});
	animation_thread.detach();

	scan_action();
	screen.Loop(renderer);
	return 0;
}
