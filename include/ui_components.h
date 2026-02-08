#ifndef STEAM_SHOWCASE_GEN_UI_COMPONENTS_H
#define STEAM_SHOWCASE_GEN_UI_COMPONENTS_H

#include <functional>
#include <string>
#include <vector>
#include "ftxui/component/component.hpp"

namespace SteamShowcaseGen::Ui
{
	/**
	 * @struct AppState
	 * @brief 聚合 UI 运行时的所有状态变量
	 */
	struct AppState
	{
		std::string				 src_dir = "target_resource";
		std::string				 out_dir = "output";
		std::vector<std::string> file_list;
		int						 selected_file_idx = 0;
		int						 sampling_rate	   = 10;
		int						 quality_idx	   = 2;
		int						 tab_idx		   = 0;
		std::string				 current_log;
		int						 spinner_index = 0;
	};

	/**
	 * @brief 构建完整的应用程序 UI 界面
	 * @param state 应用状态引用
	 * @param on_start 点击"开始生成"按钮的回调
	 * @param is_busy 当前是否正在处理任务 (用于控制 UI 禁用/加载状态)
	 * @return 封装好的根组件
	 */
	ftxui::Component BuildMainInterface(AppState &state, const std::function<void()> &on_start, const std::function<bool()> &is_busy);

} // namespace SteamShowcaseGen::Ui

#endif // STEAM_SHOWCASE_GEN_UI_COMPONENTS_H
