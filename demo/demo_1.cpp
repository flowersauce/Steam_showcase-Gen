#include <format>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/screen.hpp>
#include <opencv2/opencv.hpp>

int main()
{
	using namespace ftxui;

	// 1. 获取 OpenCV 版本信息 (如果 OpenCV 没链接好，这里会报错)
	const std::string cv_version = cv::getVersionString();

	// 2. 获取构建信息
	const std::string &build_info	= cv::getBuildInformation();
	const bool		   is_optimized = !build_info.empty();

	// 3. 构建一个简单的 FTXUI 界面
	auto document = window(text(" 环境验证 (Environment Check) ") | bold | color(Color::Green),
						   vbox({
							   hbox({text("OpenCV Version: ") | bold, text(cv_version) | color(Color::Cyan)}),
							   hbox({text("Build Status:   ") | bold, text(is_optimized ? "Linked Successfully" : "Error") | color(Color::Yellow)}),
							   separator(),
							   text("如果你看到了这个带框的界面，说明：") | color(Color::GrayDark),
							   text("1. MSVC C++23 编译器工作正常"),
							   text("2. OpenCV 静态库链接成功"),
							   text("3. FTXUI 静态库渲染正常"),
						   }));

	// 4. 渲染到终端
	auto screen = Screen::Create(Dimension::Full(), Dimension::Fit(document));
	Render(screen, document);
	screen.Print();

	system("pause");
	return 0;
}
