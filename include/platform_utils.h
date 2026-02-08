#ifndef STEAM_SHOWCASE_GEN_PLATFORM_UTILS_H
#define STEAM_SHOWCASE_GEN_PLATFORM_UTILS_H

#include <string>

namespace SteamShowcaseGen::Platform
{
	/**
	 * @brief 打开本地目录 (调用系统资源管理器)
	 * @param path 目录路径
	 */
	void OpenDirectory(const std::string &path);

	/**
	 * @brief 将文本复制到系统剪贴板
	 * @param text 待复制的文本
	 */
	void CopyToClipboard(const std::string &text);

} // namespace SteamShowcaseGen::Platform

#endif // STEAM_SHOWCASE_GEN_PLATFORM_UTILS_H
