/**
 * @file app_text.hpp
 * @brief UI 文本翻译中心
 */

#ifndef STEAM_SHOWCASE_GEN_APP_TEXT_HPP
#define STEAM_SHOWCASE_GEN_APP_TEXT_HPP

#include <string_view>

namespace SteamShowcaseGen::AppText
{
	// 动态版权信息
	inline constexpr std::string_view VAL_COPYRIGHT = "Copyright (c) " APP_BUILD_YEAR " " APP_AUTHOR ". " APP_LICENSE ".";

#ifdef LANG_ZH_CN
	inline constexpr std::string_view TITLE		= " STEAM 创意工坊展柜图像生成器 ";
	inline constexpr std::string_view TAB_MAIN	= "主页";
	inline constexpr std::string_view TAB_ABOUT = "关于";

	// 主页资源区
	inline constexpr std::string_view LABEL_DIR_SRC	  = " 目录";
	inline constexpr std::string_view PLACEHOLDER_SRC = "源文件目录路径";
	inline constexpr std::string_view BTN_SCAN		  = "扫描";
	inline constexpr std::string_view LABEL_FILE_LIST = " 文件列表";

	// 主页输出区
	inline constexpr std::string_view LABEL_DIR_OUT	  = " 输出路径";
	inline constexpr std::string_view PLACEHOLDER_OUT = "输出保存路径";
	inline constexpr std::string_view LABEL_SAMPLING  = " 帧采样率";
	inline constexpr std::string_view LABEL_QUALITY	  = " 缩放质量";

	inline constexpr std::string_view BTN_OPEN = "打开";

	inline constexpr std::string_view QUALITY_FAST	 = "快速   (最近邻)";
	inline constexpr std::string_view QUALITY_MEDIUM = "平衡   (双线性)";
	inline constexpr std::string_view QUALITY_HIGH	 = "高质量 (双三次)";
	inline constexpr std::string_view QUALITY_BEST	 = "极高   (Lanczos)";

	// 运行状态与日志
	inline constexpr std::string_view BTN_START		 = "开始生成";
	inline constexpr std::string_view BTN_PROCESSING = "生成中";

	inline constexpr std::string_view LOG_READY		= "就绪";
	inline constexpr std::string_view LOG_SCANNING	= "正在扫描...";
	inline constexpr std::string_view LOG_SCAN_DONE = "扫描完成，发现 {} 个文件";
	inline constexpr std::string_view LOG_STARTING	= "启动处理任务...";
	inline constexpr std::string_view LOG_ENCODING	= "正在编码... 已处理帧数: ";
	inline constexpr std::string_view LOG_FINISHED	= "任务完成! 输出目录: ";
	inline constexpr std::string_view LOG_HEX_HACK	= "应用 Hex Hack...";

	inline constexpr std::string_view ERR_NO_FILE	  = "错误: 请先选择有效文件";
	inline constexpr std::string_view ERR_DIR_INVALID = "目录不存在";
	inline constexpr std::string_view ERR_OPEN_FAILED = "错误: 无法打开文件";

	inline constexpr std::string_view TAG_NO_FILE	  = "<无文件>";
	inline constexpr std::string_view TAG_INVALID_DIR = "<无效目录>";

	// 关于页
	inline constexpr std::string_view LABEL_VERSION = " Version : ";
	inline constexpr std::string_view LABEL_REPO	= " Repo : ";

	inline constexpr std::string_view ABOUT_HEADER_SYSTEM = " 系统信息 ";
	inline constexpr std::string_view ABOUT_HEADER_GUIDE  = " 使用教程 ";

	// 教程分段文本
	// 步骤 1: 软件操作
	inline constexpr std::string_view GUIDE_STEP_1_TEXT = "选择目标图片或视频 -> 点击 [开始生成] -> 得到切片文件";

	// 步骤 2: 访问网页
	inline constexpr std::string_view GUIDE_STEP_2_TEXT = "访问 STEAM 艺术作品上传页: ";
	inline constexpr std::string_view GUIDE_URL			= "steamcommunity.com/sharedfiles/edititem/767/3/";

	// 步骤 3: 控制台代码 (拆分以支持高亮)
	inline constexpr std::string_view GUIDE_STEP_3_TEXT_PRE = "上传切片 -> 按 ";
	inline constexpr std::string_view GUIDE_STEP_3_KEY		= "F12";
	inline constexpr std::string_view GUIDE_STEP_3_TEXT_MID = " 打开控制台 -> 粘贴代码 -> ";
	inline constexpr std::string_view GUIDE_STEP_3_ACTION	= "按回车运行";

	// 步骤 4: 保存
	inline constexpr std::string_view GUIDE_STEP_4_TEXT = "回到网页 -> 点击 [保存并继续]，重复此操作直到全部上传完成";

	// 注入 JS 代码
	inline constexpr std::string_view GUIDE_CODE_1 = "$J('#ConsumerAppID').val(480),";
	inline constexpr std::string_view GUIDE_CODE_2 = "$J('[name=file_type]').val(0),";
	inline constexpr std::string_view GUIDE_CODE_3 = "$J('[name=visibility]').val(0);";

	inline constexpr std::string_view BTN_COPY	 = " 复制代码 ";
	inline constexpr std::string_view BTN_COPIED = " √ 已复制 ";

	// 元数据
	inline constexpr std::string_view VAL_REPO_NAME = "Code REPO";
	inline constexpr std::string_view VAL_REPO_URL	= APP_REPO_URL;
	inline constexpr std::string_view VAL_VERSION	= APP_VERSION;

#else
	// 其他语言定义

#endif
} // namespace SteamShowcaseGen::AppText

// 设置默认值放置 IDE 报错
#ifndef APP_VERSION
#define APP_VERSION "Unknown"
#endif
#ifndef APP_REPO_URL
#define APP_REPO_URL "Unknown"
#endif
#ifndef APP_BUILD_DATE
#define APP_BUILD_DATE "Unknown"
#endif
#ifndef APP_BUILD_YEAR
#define APP_BUILD_YEAR "Unknown"
#endif
#ifndef APP_AUTHOR
#define APP_AUTHOR "Flowersauce"
#endif
#ifndef APP_LICENSE
#define APP_LICENSE "MIT License"
#endif

#endif // STEAM_SHOWCASE_GEN_APP_TEXT_HPP
