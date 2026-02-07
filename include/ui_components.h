/**
 * @file ui_components.h
 * @brief TUI 界面组件构建函数声明
 */

#ifndef STEAM_SHOWCASE_GEN_UI_COMPONENTS_H
#define STEAM_SHOWCASE_GEN_UI_COMPONENTS_H

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
	 * @brief 创建主页面的布局组件
	 * @param state 应用状态引用
	 * @return ftxui::Component 组合后的主页面组件
	 */
	ftxui::Component make_main_layout(AppState &state);

	/**
	 * @brief 创建关于页面的布局组件
	 * @return ftxui::Component 只读渲染组件
	 */
	ftxui::Component make_about_layout();

	/**
	 * @brief 渲染底部的全局状态栏
	 * @param state 应用状态引用
	 * @param is_processing 后台处理状态
	 * @param btn_start 开始按钮组件引用
	 * @return ftxui::Element 渲染后的状态栏元素
	 */
	ftxui::Element render_status_bar(const AppState &state, bool is_processing, const ftxui::Component &btn_start);

} // namespace SteamShowcaseGen::Ui

#endif // STEAM_SHOWCASE_GEN_UI_COMPONENTS_H
