#include "ui_components.h"
#include <algorithm>
#include <filesystem>
#include <format>
#include <ranges>
#include "app_text.hpp"
#include "ftxui/dom/elements.hpp"
#include "platform_utils.h"

namespace SteamShowcaseGen::Ui
{
	using namespace ftxui;
	namespace txt = AppText;
	namespace fs  = std::filesystem;

	// --- 内部辅助函数声明 ---
	static Component MakeHomeTab(AppState &state);
	static Component MakeAboutTab();
	static Component MakeStartButton(const std::function<void()> &on_start, const std::function<bool()> &is_busy);
	static Element	 RenderHeader(const Component &tab_toggle);
	static Element	 RenderStatusBar(const AppState &state, bool is_busy, const Component &btn_start);

	// --- 核心入口 ---
	Component BuildMainInterface(AppState &state, const std::function<void()> &on_start, const std::function<bool()> &is_busy)
	{
		// 1. 构建各子页面
		auto home_page	= MakeHomeTab(state);
		auto about_page = MakeAboutTab();
		auto btn_start	= MakeStartButton(on_start, is_busy);

		// 2. 构建 Tab 切换栏
		static std::vector<std::string> tab_labels = {std::string(txt::TAB_MAIN), std::string(txt::TAB_ABOUT)};

		MenuOption tab_opt				 = MenuOption::Horizontal();
		tab_opt.elements_infix			 = [] { return text(""); };
		tab_opt.entries_option.transform = [](const EntryState &s)
		{
			auto element = text(s.label);
			if (s.active)
			{
				const Color active_color = (s.index == 0) ? Color::Cyan : Color::Blue;
				element |= bold | color(active_color);
			}
			auto item_content = s.focused ? hbox({text("[") | dim, element, text("]") | dim}) : element;
			item_content	  = item_content | center | size(WIDTH, EQUAL, 10);
			if (s.index > 0)
			{
				return hbox({separator(), item_content});
			}
			return item_content;
		};
		auto tab_toggle = Menu(&tab_labels, &state.tab_idx, tab_opt);
		auto tab_view	= Container::Tab({home_page, about_page}, &state.tab_idx);

		// 3. 组合根容器
		const auto root_container = Container::Vertical({tab_toggle, tab_view, btn_start});

		// 4. 添加全局事件捕获
		const auto event_handler = root_container
			| CatchEvent(
									   [is_busy](const Event & /*unused*/)
									   {
										   return is_busy(); // 如果忙碌，吞掉所有事件
									   });

		// 5. 定义主渲染器
		return Renderer(event_handler,
						[=, &state]
						{
							const bool busy = is_busy();

							// 渲染头部
							auto header = RenderHeader(tab_toggle);

							// 根据 Tab 页面决定布局逻辑
							if (state.tab_idx == 1)
							{
								// 关于页：全屏显示内容，不渲染底部状态栏
								return vbox({header, about_page->Render() | flex});
							}
							// 主页：显示内容 + 底部状态栏
							auto footer = RenderStatusBar(state, busy, btn_start);

							return vbox({header, home_page->Render() | flex, footer});
						});
	}

	// --- 内部实现细节 ---

	static Component MakeHomeTab(AppState &state)
	{
		// 扫描动作
		auto scan_action = [&state]
		{
			state.file_list.clear();
			state.selected_file_idx = 0;
			state.current_log		= std::string(txt::LOG_SCANNING);
			try
			{
				if (const fs::path dir_path(state.src_dir); fs::exists(dir_path) && fs::is_directory(dir_path))
				{
					static constexpr std::array supported = {".mp4", ".avi", ".mov", ".mkv", ".png", ".jpg", ".jpeg", ".bmp", ".webp", ".tif", ".tiff"};
					for (const auto &entry: fs::directory_iterator(dir_path))
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

		// 组件定义
		InputOption input_opt;
		input_opt.multiline = false;

		auto input_src	  = Input(&state.src_dir, std::string(txt::PLACEHOLDER_SRC), input_opt);
		auto btn_scan	  = Button(std::string(txt::BTN_SCAN), scan_action, ButtonOption::Ascii());
		auto btn_open_src = Button(std::string(txt::BTN_OPEN), [&] { Platform::OpenDirectory(state.src_dir); }, ButtonOption::Ascii());

		auto menu_file	  = Menu(&state.file_list, &state.selected_file_idx);
		auto input_out	  = Input(&state.out_dir, std::string(txt::PLACEHOLDER_OUT), input_opt);
		auto btn_open_out = Button(std::string(txt::BTN_OPEN), [&] { Platform::OpenDirectory(state.out_dir); }, ButtonOption::Ascii());

		auto slider_samp = Slider("", &state.sampling_rate, 1, 10, 1);

		static std::vector q_labels = {
			std::string(txt::QUALITY_FAST), std::string(txt::QUALITY_MEDIUM), std::string(txt::QUALITY_HIGH), std::string(txt::QUALITY_BEST)};
		MenuOption quality_opt;
		quality_opt.entries_option.transform = [](const EntryState &s)
		{
			const Color c	   = (s.index == 0) ? Color::RedLight : (s.index == 1) ? Color::Yellow : (s.index == 2) ? Color::Green : Color::Blue;
			auto		prefix = text(s.active ? "◉ " : "○ ") | color(Color::GrayLight);
			auto		label  = text(s.label) | color(c);
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
		auto menu_quality = Menu(&q_labels, &state.quality_idx, quality_opt);

		// 布局容器
		auto	   left_col	 = Container::Vertical({input_src, btn_scan, btn_open_src, menu_file});
		auto	   right_col = Container::Vertical({input_out, btn_open_out, slider_samp, menu_quality});
		const auto container = Container::Horizontal({left_col, right_col});

		// 渲染逻辑
		return Renderer(container,
						[=, &state]
						{
							constexpr int	  STD_W		  = 10;
							int				  div		  = 11 - state.sampling_rate;
							const std::string display_str = (state.sampling_rate == 10) ? "N/A" : std::format("1/{}", div);

							auto resource_view = vbox({hbox({text(std::string(txt::LABEL_DIR_SRC)) | vcenter | size(WIDTH, EQUAL, STD_W),
															 separator(),
															 input_src->Render() | size(WIDTH, EQUAL, 24),
															 filler(),
															 separator(),
															 btn_scan->Render() | center | size(WIDTH, EQUAL, STD_W),
															 separator(),
															 btn_open_src->Render() | center | size(WIDTH, EQUAL, STD_W)})
														   | size(HEIGHT, EQUAL, 1),
													   separator(),
													   vbox({text(std::string(txt::LABEL_FILE_LIST)) | bold,
															 separator(),
															 hbox({text(" "), menu_file->Render() | vscroll_indicator | frame | size(WIDTH, EQUAL, 50)})})
														   | flex})
								| border | flex;

							auto config_view = vbox({hbox({text(std::string(txt::LABEL_DIR_OUT)) | vcenter | size(WIDTH, EQUAL, STD_W),
														   separator(),
														   input_out->Render() | size(WIDTH, EQUAL, 24),
														   filler(),
														   text(" ") | size(WIDTH, EQUAL, STD_W + 1),
														   separator(),
														   btn_open_out->Render() | center | size(WIDTH, EQUAL, STD_W)})
														 | size(HEIGHT, EQUAL, 1),
													 separator(),
													 hbox({text(std::string(txt::LABEL_SAMPLING)) | vcenter | size(WIDTH, EQUAL, STD_W),
														   separator(),
														   slider_samp->Render() | flex,
														   separator(),
														   text(display_str) | dim | center | size(WIDTH, EQUAL, STD_W)})
														 | size(HEIGHT, EQUAL, 1),
													 separator(),
													 text(std::string(txt::LABEL_QUALITY)) | bold,
													 separator(),
													 hbox({text(" "), menu_quality->Render() | flex}) | flex})
								| border | flex;

							return hbox({resource_view, text(" "), config_view});
						});
	}

	static Component MakeAboutTab()
	{
		static const std::string full_code_str = std::string(txt::GUIDE_CODE_1) + std::string(txt::GUIDE_CODE_2) + std::string(txt::GUIDE_CODE_3);
		auto					 copied_state  = std::make_shared<bool>(false);

		auto btn_option		 = ButtonOption::Ascii();
		btn_option.transform = [copied_state](const EntryState &s)
		{
			const auto label	 = *copied_state ? txt::BTN_COPIED : txt::BTN_COPY;
			const auto color_val = *copied_state ? Color::Green : Color::Blue;
			auto	   content	 = text(std::string(label)) | bold | color(color_val);
			if (s.focused)
			{
				content |= inverted;
			}
			return hbox({text("[") | dim, content, text("]") | dim});
		};

		const auto btn_copy = Button(
			std::string(txt::BTN_COPY),
			[=]
			{
				Platform::CopyToClipboard(full_code_str);
				*copied_state = true;
			},
			btn_option);

		return Renderer(
			btn_copy,
			[=]
			{
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

				const auto code_block = vbox({text(std::string(txt::GUIDE_CODE_1)) | color(Color::Yellow),
											  text(std::string(txt::GUIDE_CODE_2)) | color(Color::Yellow),
											  text(std::string(txt::GUIDE_CODE_3)) | color(Color::Yellow),
											  filler(),
											  hbox({filler(), btn_copy->Render()})})
					| borderEmpty | bgcolor(Color::GrayDark);

				const auto guide_content =
					vbox({
						hbox({text("1. ") | bold | color(Color::Blue), text(std::string(txt::GUIDE_STEP_1_TEXT))}),
						text(" "),
						hbox({text("2. ") | bold | color(Color::Blue),
							  text(std::string(txt::GUIDE_STEP_2_TEXT)),
							  text(std::string(txt::GUIDE_URL)) | color(Color::BlueLight) | underlined | hyperlink("https://" + std::string(txt::GUIDE_URL))}),
						text(" "),
						hbox({text("3. ") | bold | color(Color::Blue),
							  text(std::string(txt::GUIDE_STEP_3_TEXT_PRE)),
							  text(std::string(txt::GUIDE_STEP_3_KEY)) | bold | color(Color::White) | bgcolor(Color::Red),
							  text(std::string(txt::GUIDE_STEP_3_TEXT_MID)),
							  text(std::string(txt::GUIDE_STEP_3_ACTION)) | bold | color(Color::RedLight)}),
						text(" "),
						code_block | flex,
						text(" "),
						hbox({text("4. ") | bold | color(Color::Blue), text(std::string(txt::GUIDE_STEP_4_TEXT))}),
					})
					| borderEmpty;

				return vbox({window(text(std::string(txt::ABOUT_HEADER_APP)) | center | bold, info_content),
							 window(text(std::string(txt::ABOUT_HEADER_GUIDE)) | center | bold, guide_content) | flex});
			});
	}

	static Component MakeStartButton(const std::function<void()> &on_start, const std::function<bool()> &is_busy)
	{
		ButtonOption opt = ButtonOption::Ascii();
		opt.transform	 = [is_busy](const EntryState &s)
		{
			const bool busy	   = is_busy();
			const auto label   = busy ? txt::BTN_PROCESSING : txt::BTN_START;
			const auto col	   = busy ? Color::Red : (s.focused ? Color::GreenLight : Color::Green);
			auto	   content = text(std::string(label)) | bold | color(col);
			if (s.focused && !busy)
			{
				return hbox({text("[") | color(Color::GrayLight), content, text("]") | color(Color::GrayLight)}) | center;
			}
			return hbox({text(" "), content, text(" ")}) | center;
		};
		return Button("", on_start, opt);
	}

	static Element RenderHeader(const Component &tab_toggle)
	{
		return hbox({text(std::string(txt::TITLE)) | bold | color(Color::Blue), filler(), text(" "), tab_toggle->Render()}) | borderRounded
			| color(Color::Default) | size(HEIGHT, EQUAL, 3);
	}

	static Element RenderStatusBar(const AppState &state, const bool is_busy, const Component &btn_start)
	{
		Element spinner_elem;
		if (is_busy)
		{
			spinner_elem = spinner(12, state.spinner_index) | bold | color(Color::Yellow);
		}
		else
		{
			const bool is_error = state.current_log.find("错误") != std::string::npos || state.current_log.find("Error") != std::string::npos
				|| state.current_log.find("失败") != std::string::npos;

			const Color dot_color = is_error ? Color::Red : Color::Green;

			spinner_elem = text("●") | color(dot_color);
		}

		return hbox({spinner_elem | center | size(WIDTH, EQUAL, 3),
					 text(state.current_log) | vcenter | flex,
					 separator(),
					 btn_start->Render() | size(WIDTH, EQUAL, 20) | center})
			| border | size(HEIGHT, EQUAL, 3);
	}

} // namespace SteamShowcaseGen::Ui
