#include "ui_components.h"
#include <algorithm>
#include <filesystem>
#include <format>
#include <ranges>
#include "app_text.hpp"
#include "ftxui/dom/elements.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace SteamShowcaseGen::Ui
{
	using namespace ftxui;
	namespace txt = AppText;

	// 辅助函数：复制到剪贴板
	void copy_to_clipboard(const std::string &text)
	{
#ifdef _WIN32
		if (!OpenClipboard(nullptr))
		{
			return;
		}
		EmptyClipboard();
		const HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
		if (!hg)
		{
			CloseClipboard();
			return;
		}
		memcpy(GlobalLock(hg), text.c_str(), text.size() + 1);
		GlobalUnlock(hg);
		SetClipboardData(CF_TEXT, hg);
		CloseClipboard();
		GlobalFree(hg);
#endif
	}

	// 辅助函数：打开文件夹
	void open_directory(const std::string &path)
	{
#ifdef _WIN32
		// 使用 ShellExecute 打开资源管理器
		ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOW);
#endif
	}

	Component make_main_layout(AppState &state)
	{
		auto scan_action = [&state]
		{
			state.file_list.clear();
			state.selected_file_idx = 0;
			state.current_log		= std::string(txt::LOG_SCANNING);
			try
			{
				if (const std::filesystem::path dir_path(state.src_dir); std::filesystem::exists(dir_path) && std::filesystem::is_directory(dir_path))
				{
					static constexpr std::array supported = {".mp4", ".avi", ".mov", ".mkv", ".png", ".jpg", ".jpeg", ".webp"};
					for (const auto &entry: std::filesystem::directory_iterator(dir_path))
					{
						if (entry.is_regular_file())
						{
							std::string ext = entry.path().extension().string();
							std::ranges::transform(ext, ext.begin(), ::tolower);
							if (std::ranges::any_of(supported, [&](auto s) { return s == ext; }))
							{
								state.file_list.push_back(entry.path().filename().string());
							}
						}
					}
					if (state.file_list.empty())
					{
						state.file_list.emplace_back(txt::TAG_NO_FILE);
						state.current_log = std::string(txt::ERR_NO_FILE);
					}
					else
					{
						const size_t count = state.file_list.size();
						state.current_log  = std::vformat(txt::LOG_SCAN_DONE, std::make_format_args(count));
					}
				}
				else
				{
					state.file_list.emplace_back(txt::TAG_INVALID_DIR);
					state.current_log = std::string(txt::ERR_DIR_INVALID);
				}
			}
			catch (const std::exception &e)
			{
				state.current_log = e.what();
			}
		};

		static std::vector q_labels = {
			std::string(txt::QUALITY_FAST), std::string(txt::QUALITY_MEDIUM), std::string(txt::QUALITY_HIGH), std::string(txt::QUALITY_BEST)};

		MenuOption quality_opt;
		quality_opt.entries_option.transform = [](const EntryState &s)
		{
			Color c;
			switch (s.index)
			{
				case 0:
					c = Color::RedLight;
					break;
				case 1:
					c = Color::Yellow;
					break;
				case 2:
					c = Color::Green;
					break;
				case 3:
					c = Color::Blue;
					break;
				default:
					c = Color::White;
					break;
			}
			auto prefix = text(s.active ? "◉ " : "○ ") | color(Color::GrayLight);
			auto label	= text(s.label) | color(c);
			if (s.active)
			{
				label |= bold;
			}
			auto res = hbox({prefix, label});
			if (s.focused)
			{
				res |= inverted;
			}
			return res;
		};

		InputOption input_opt;
		input_opt.multiline = false;

		// --- 组件定义 ---
		auto input_src = Input(&state.src_dir, std::string(txt::PLACEHOLDER_SRC), input_opt);
		auto btn_scan  = Button(std::string(txt::BTN_SCAN), scan_action, ButtonOption::Ascii());
		// [新增] 打开源目录按钮
		auto btn_open_src = Button(std::string(txt::BTN_OPEN), [&] { open_directory(state.src_dir); }, ButtonOption::Ascii());

		auto menu_file = Menu(&state.file_list, &state.selected_file_idx);
		auto input_out = Input(&state.out_dir, std::string(txt::PLACEHOLDER_OUT), input_opt);
		// [新增] 打开输出目录按钮
		auto btn_open_out = Button(std::string(txt::BTN_OPEN), [&] { open_directory(state.out_dir); }, ButtonOption::Ascii());

		auto slider_samp  = Slider("", &state.sampling_rate, 1, 10, 1);
		auto menu_quality = Menu(&q_labels, &state.quality_idx, quality_opt);

		// 定义标准长度为 10
		constexpr int STD_W = 10;

		// [修改] 更新容器包含关系，加入新按钮以便处理焦点
		auto	   left_col	 = Container::Vertical({input_src, btn_scan, btn_open_src, menu_file});
		auto	   right_col = Container::Vertical({input_out, btn_open_out, slider_samp, menu_quality});
		const auto container = Container::Horizontal({left_col, right_col});

		return Renderer(container,
						[=, &state]
						{
							int				  div		  = 11 - state.sampling_rate;
							const std::string display_str = (state.sampling_rate == 10) ? "N/A" : std::format("1/{}", div);

							// [修改] 资源视图 (左侧卡片)
							const auto resource_view = vbox({hbox({text(std::string(txt::LABEL_DIR_SRC)) | vcenter | size(WIDTH, EQUAL, STD_W),
																   separator(),
																   input_src->Render() | size(WIDTH, EQUAL, 24),
																   filler(),
																   separator(),
																   btn_scan->Render() | center | size(WIDTH, EQUAL, STD_W),
																   // [新增] 扫描按钮右侧的分割线和打开按钮
																   separator(),
																   btn_open_src->Render() | center | size(WIDTH, EQUAL, STD_W)})
																 | size(HEIGHT, EQUAL, 1),
															 separator(),
															 vbox({text(std::string((txt::LABEL_FILE_LIST))) | bold,
																   separator(),
																   hbox({text(" "), menu_file->Render() | vscroll_indicator | frame | size(WIDTH, EQUAL, 56)})})
																 | flex})
								| border | flex;

							// [修改] 设置视图 (右侧卡片)
							const auto config_view = vbox({hbox({text(std::string((txt::LABEL_DIR_OUT))) | vcenter | size(WIDTH, EQUAL, STD_W),
																 separator(),
																 input_out->Render() | size(WIDTH, EQUAL, 24),
																 filler(),
																 // 这一部分内容的作用是模拟扫描按钮的占用空间
																 text(" ") | size(WIDTH, EQUAL, STD_W + 1),
																 separator(),
																 btn_open_out->Render() | center | size(WIDTH, EQUAL, STD_W)})
															   | size(HEIGHT, EQUAL, 1),
														   separator(),
														   hbox({text(std::string((txt::LABEL_SAMPLING))) | vcenter | size(WIDTH, EQUAL, STD_W),
																 separator(),
																 slider_samp->Render() | flex,
																 separator(),
																 text(display_str) | dim | center | size(WIDTH, EQUAL, STD_W)})
															   | size(HEIGHT, EQUAL, 1),
														   separator(),
														   text(std::string((txt::LABEL_QUALITY))) | bold,
														   separator(),
														   hbox({text(" "), menu_quality->Render() | flex}) | flex})
								| border | flex;

							return hbox({resource_view, text(" "), config_view});
						});
	}

	Component make_about_layout()
	{
		// 1. 准备代码字符串 (用于复制)
		static const std::string full_code_str = std::string(txt::GUIDE_CODE_1) + std::string(txt::GUIDE_CODE_2) + std::string(txt::GUIDE_CODE_3);

		auto copied_state = std::make_shared<bool>(false);

		auto btn_option		 = ButtonOption::Ascii();
		btn_option.transform = [copied_state](const EntryState &s)
		{
			const auto label	 = *copied_state ? txt::BTN_COPIED : txt::BTN_COPY;
			const auto color_val = *copied_state ? Color::Green : Color::Blue;

			auto content = text(std::string(label)) | bold | color(color_val);
			if (s.focused)
			{
				content = content | inverted;
			}

			return hbox({text("[") | dim, content, text("]") | dim});
		};

		const auto btn_copy = Button(
			std::string(txt::BTN_COPY),
			[=]
			{
				copy_to_clipboard(full_code_str);
				*copied_state = true;
			},
			btn_option);

		return Renderer(
			btn_copy,
			[=]
			{
				// ================= 上半部分：系统信息 =================
				const auto info_content =
					vbox({hbox(
						{hbox({text(std::string(txt::LABEL_VERSION)) | dim, text(std::string(txt::VAL_VERSION)) | bold | color(Color::Blue)}) | center | flex,
						 separator() | color(Color::GrayDark),
						 hbox({text(std::string(txt::VAL_COPYRIGHT)) | color(Color::Yellow)}) | center | flex,
						 separator() | color(Color::GrayDark),
						 hbox(
							 {text(std::string(txt::LABEL_REPO)) | dim,
							  text(std::string(txt::VAL_REPO_NAME)) | bold | color(Color::GreenLight) | hyperlink("https://" + std::string(txt::VAL_REPO_URL))})
							 | center | flex})})
					| borderEmpty;

				// ================= 下半部分：教程卡片 =================
				const auto code_block = vbox({text(std::string(txt::GUIDE_CODE_1)) | color(Color::Yellow),
											  text(std::string(txt::GUIDE_CODE_2)) | color(Color::Yellow),
											  text(std::string(txt::GUIDE_CODE_3)) | color(Color::Yellow),
											  filler(),
											  hbox({filler(), btn_copy->Render()})})
					| borderEmpty | bgcolor(Color::GrayDark);

				const auto guide_content =
					vbox({
						// 步骤 1: 软件内操作
						hbox({text("1. ") | bold | color(Color::Blue), text(std::string(txt::GUIDE_STEP_1_TEXT))}),
						text(" "),

						// 步骤 2: 访问网页
						hbox({text("2. ") | bold | color(Color::Blue),
							  text(std::string(txt::GUIDE_STEP_2_TEXT)),
							  text(std::string(txt::GUIDE_URL)) | color(Color::BlueLight) | underlined | hyperlink("https://" + std::string(txt::GUIDE_URL))}),
						text(" "),

						// 步骤 3: 控制台 (纯文本说明)
						hbox({text("3. ") | bold | color(Color::Blue),
							  text(std::string(txt::GUIDE_STEP_3_TEXT_PRE)),
							  text(std::string(txt::GUIDE_STEP_3_KEY)) | bold | color(Color::White) | bgcolor(Color::Red),
							  text(std::string(txt::GUIDE_STEP_3_TEXT_MID)),
							  text(std::string(txt::GUIDE_STEP_3_ACTION)) | bold | color(Color::RedLight)}),
						text(" "),

						// 代码块
						code_block | flex,
						text(" "),

						// 步骤 4: 保存
						hbox({text("4. ") | bold | color(Color::Blue), text(std::string(txt::GUIDE_STEP_4_TEXT))}),
					})
					| borderEmpty;

				return vbox({window(text(std::string(txt::ABOUT_HEADER_SYSTEM)) | center | bold, info_content),
							 window(text(std::string(txt::ABOUT_HEADER_GUIDE)) | center | bold, guide_content) | flex});
			});
	}

	Element render_status_bar(const AppState &state, const bool is_processing, const Component &btn_start)
	{
		const auto icon = is_processing ? spinner(12, state.spinner_index) | color(Color::Yellow) | bold : text("●") | color(Color::Green);

		// [开始按钮] 这里保持原样 (size|center)，因为 btn_start 内部 render 已经处理了内容居中，这里是对整个按钮组件居中
		return hbox({icon | center | size(WIDTH, EQUAL, 3),
					 text(state.current_log) | vcenter | flex,
					 separator(),
					 btn_start->Render() | size(WIDTH, EQUAL, 20) | center})
			| border | size(HEIGHT, EQUAL, 3);
	}
} // namespace SteamShowcaseGen::Ui
