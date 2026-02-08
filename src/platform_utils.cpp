#include "platform_utils.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace SteamShowcaseGen::Platform
{
	void OpenDirectory(const std::string &path)
	{
#ifdef _WIN32
		// SW_SHOW 确保窗口可见
		ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOW);
#else
		// Linux/Mac implementation placeholders
		// system(("xdg-open " + path).c_str());
#endif
	}

	void CopyToClipboard(const std::string &text)
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
} // namespace SteamShowcaseGen::Platform
