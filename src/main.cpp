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
	// 1. 系统初始化
	cv::utils::logging::setLogLevel(cv::utils::logging::LOG_LEVEL_SILENT);
	av_log_set_level(AV_LOG_QUIET);

	ssg::Ui::AppState	   app_state;
	ssg::ShowcaseProcessor processor;
	auto				   screen = ScreenInteractive::Fullscreen();

	// 2. 目录初始化
	try
	{
		namespace fs = std::filesystem;
		if (!fs::exists(app_state.src_dir))
		{
			fs::create_directories(app_state.src_dir);
		}
		if (!fs::exists(app_state.out_dir))
		{
			fs::create_directories(app_state.out_dir);
		}
		if (!fs::exists("log"))
		{
			fs::create_directories("log");
		}
	}
	catch (...)
	{
	}

	// 3. 定义核心业务回调
	auto start_task_callback = [&]
	{
		if (app_state.file_list.empty() || app_state.file_list[0].starts_with('<'))
		{
			app_state.current_log = "错误: 请先扫描目录选择有效文件";
			screen.Post(Event::Custom); // 刷新 UI 显示错误信息
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
								 screen.Post(Event::Custom); // 触发 UI 刷新
							 });
	};

	auto is_busy_callback = [&] { return processor.is_active(); };

	// 4. 构建统一 UI
	const auto main_interface = ssg::Ui::BuildMainInterface(app_state, start_task_callback, is_busy_callback);

	// 5. 启动后台动画线程
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

	// 6. 运行主循环
	app_state.current_log = std::string(ssg::AppText::LOG_READY);
	screen.Loop(main_interface);

	return 0;
}
