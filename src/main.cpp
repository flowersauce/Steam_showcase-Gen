#include <filesystem>
#include <opencv2/core/utils/logger.hpp>
#include "app_text.hpp"
#include "ftxui/component/screen_interactive.hpp"
#include "showcase_processor.h"
#include "ui_components.h"

extern "C"
{
#include <libavutil/log.h>
}

using namespace ftxui;
namespace ssg = SteamShowcaseGen;

int main()
{
	// 禁用日志
	cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);
	av_log_set_level(AV_LOG_QUIET);

	ssg::Ui::AppState	   app_state;
	ssg::ShowcaseProcessor processor;
	auto				   screen = ScreenInteractive::Fullscreen();

	try
	{
		namespace fs = std::filesystem;

		// 1. 创建默认源文件目录
		if (!fs::exists(app_state.src_dir))
		{
			fs::create_directories(app_state.src_dir);
		}

		// 2. 创建默认输出目录
		if (!fs::exists(app_state.out_dir))
		{
			fs::create_directories(app_state.out_dir);
		}

		// 3. 创建运行时 log 输出目录
		if (!fs::exists("log"))
		{
			fs::create_directories("log");
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Init Warning: " << e.what() << std::endl;
	}

	auto start_task = [&]
	{
		if (app_state.file_list.empty() || app_state.file_list[0].starts_with('<'))
		{
			return;
		}
		const auto src_path = std::filesystem::path(app_state.src_dir) / app_state.file_list[app_state.selected_file_idx];
		processor.start_task(src_path,
							 app_state.out_dir,
							 app_state.sampling_rate,
							 app_state.quality_idx,
							 [&](const std::string_view log)
							 {
								 app_state.current_log = std::string(log);
								 screen.Post(Event::Custom);
							 });
	};

	ButtonOption start_opt = ButtonOption::Ascii();
	start_opt.transform	   = [&](const EntryState &s)
	{
		const bool is_busy = processor.is_active();
		const auto label   = is_busy ? ssg::AppText::BTN_PROCESSING : ssg::AppText::BTN_START;
		const auto col	   = is_busy ? Color::Red : (s.focused ? Color::GreenLight : Color::Green);
		auto	   content = text(std::string(label)) | bold | color(col);

		if (s.focused && !is_busy)
		{
			return hbox({text("[") | color(Color::GrayLight), content, text("]") | color(Color::GrayLight)}) | center;
		}
		return hbox({text(" "), content, text(" ")}) | center;
	};
	auto btn_start = Button("", start_task, start_opt);

	auto main_page	= ssg::Ui::make_main_layout(app_state);
	auto about_page = ssg::Ui::make_about_layout();

	std::vector tab_labels = {std::string(ssg::AppText::TAB_MAIN), std::string(ssg::AppText::TAB_ABOUT)};

	MenuOption tab_opt = MenuOption::Horizontal();

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

		item_content = item_content | center | size(WIDTH, EQUAL, 10);

		if (s.index > 0)
		{
			return hbox({separator(), item_content});
		}

		return item_content;
	};

	auto tab_toggle = Menu(&tab_labels, &app_state.tab_idx, tab_opt);
	auto tab_view	= Container::Tab({main_page, about_page}, &app_state.tab_idx);

	auto root_container = Container::Vertical({tab_toggle, tab_view, btn_start}) | CatchEvent([&](const Event &) { return processor.is_active(); });

	auto root_renderer = Renderer(root_container,
								  [&]
								  {
									  auto header =
										  hbox({text(std::string(ssg::AppText::TITLE)) | bold | color(Color::Blue), filler(), text(" "), tab_toggle->Render()})
										  | borderRounded | color(Color::Default) | size(HEIGHT, EQUAL, 3);

									  if (app_state.tab_idx == 1)
									  {
										  return vbox({header, about_page->Render() | flex});
									  }
									  return vbox({
										  header,
										  tab_view->Render() | flex,
										  ssg::Ui::render_status_bar(app_state, processor.is_active(), btn_start),
									  });
								  });

	std::jthread anim_worker(
		[&](const std::stop_token &st)
		{
			while (!st.stop_requested())
			{
				if (processor.is_active())
				{
					app_state.spinner_index++;
					screen.Post(Event::Custom);
					std::this_thread::sleep_for(std::chrono::milliseconds(80));
				}
				else
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(200));
				}
			}
		});

	app_state.current_log = std::string(ssg::AppText::LOG_READY);
	screen.Loop(root_renderer);
	return 0;
}
