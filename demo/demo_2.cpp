#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

// OpenCV
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/opencv.hpp>

// 命名空间简化
namespace fs = std::filesystem;
using namespace cv;

// 配置参数
constexpr int STEAM_SHOWCASE_WIDTH = 766;
constexpr int SLICE_WIDTH		   = 150;
constexpr int GAP_WIDTH			   = 4;
constexpr int SLICE_COUNT		   = 5;

// Steam Hex Hack 处理函数
// 功能：将 GIF 文件的结尾字节从 3B 修改为 21
// 原因：这是 Steam 创意工坊展柜的常见技巧，用于欺骗上传机制，防止黑边或压缩
void apply_steam_hex_hack(const std::string &filepath)
{
	std::fstream file(filepath, std::ios::binary | std::ios::in | std::ios::out);
	if (!file.is_open())
	{
		std::cerr << "[Error] Cannot open file for Hex Hack: " << filepath << std::endl;
		return;
	}

	// 移动到文件末尾
	file.seekg(-1, std::ios::end);

	if (const std::streampos fileSize = file.tellg(); fileSize == 0)
		return;

	char lastByte;
	file.read(&lastByte, 1);

	//  "最后，不要忘记将结尾的 3B 改成 21"
	if (static_cast<unsigned char>(lastByte) == 0x3B)
	{
		file.seekp(-1, std::ios::end);
		constexpr char newByte = 0x21;
		file.write(&newByte, 1);
		std::cout << "[Hex Hack] Applied (3B -> 21) to: " << fs::path(filepath).filename() << std::endl;
	}
	else
	{
		std::cout << "[Hex Hack] Skipped (End byte is not 3B): " << fs::path(filepath).filename() << std::endl;
	}

	file.close();
}

int main()
{
	// 1. 定义路径
	std::string input_dir_str  = "target_resource";
	std::string output_dir_str = "output";
	fs::path	input_dir(input_dir_str);
	fs::path	output_dir(output_dir_str);

	// 检查输入目录
	if (!fs::exists(input_dir))
	{
		std::cerr << "[Error] Input directory not found: " << input_dir << std::endl;
		return 1;
	}

	// 创建输出目录
	if (!fs::exists(output_dir))
	{
		fs::create_directories(output_dir);
	}

	// 2. 寻找第一张 PNG 图片
	fs::path source_path;
	bool	 found = false;
	for (const auto &entry: fs::directory_iterator(input_dir))
	{
		if (entry.path().extension() == ".png")
		{
			source_path = entry.path();
			found		= true;
			break; // 仅处理第一张作为 Demo
		}
	}

	if (!found)
	{
		std::cerr << "[Error] No .png files found in " << input_dir << std::endl;
		return 1;
	}

	std::cout << "[Processing] Source: " << source_path << std::endl;

	// 3. 加载并缩放图片
	Mat src = imread(source_path.string(), IMREAD_COLOR);
	if (src.empty())
	{
		std::cerr << "[Error] Failed to load image." << std::endl;
		return 1;
	}

	// 计算缩放后的高度，保持纵横比
	// width: 766
	double aspect_ratio	 = static_cast<double>(src.rows) / src.cols;
	int	   target_height = static_cast<int>(STEAM_SHOWCASE_WIDTH * aspect_ratio);

	Mat resized_img;
	resize(src, resized_img, Size(STEAM_SHOWCASE_WIDTH, target_height), 0, 0, INTER_LINEAR);

	std::cout << "[Resize] " << src.cols << "x" << src.rows << " -> " << STEAM_SHOWCASE_WIDTH << "x" << target_height << std::endl;

	// 4. 切片并保存
	for (int i = 0; i < SLICE_COUNT; ++i)
	{
		// 计算 ROI (Region of Interest)
		// x = i * (150 + 4)
		int x = i * (SLICE_WIDTH + GAP_WIDTH);

		// 边界检查
		if (x + SLICE_WIDTH > resized_img.cols)
		{
			std::cerr << "[Warning] Slice " << i << " exceeds image width." << std::endl;
			break;
		}

		Rect roi(x, 0, SLICE_WIDTH, target_height);
		Mat	 slice = resized_img(roi);

		// 构建输出文件名
		std::string filename = "slice_" + std::to_string(i + 1) + ".gif";
		fs::path	out_path = output_dir / filename;

		// 保存切片
		// 注意：OpenCV 的 imwrite 对 gif 支持取决于系统安装的编解码器。
		// 如果失败，建议先输出 .png，后续集成 ffmpeg 编码。
		// 但为了配合教程的 "Hex Hack"，我们需要它是 GIF 格式。
		// 这里假设您的 OpenCV 能够写入简单的静态 GIF，或者您可以改为 .png 观察效果。
		Mat success = imread(out_path.string()); // Dummy check
		try
		{
			// 尝试保存为 gif
			if (!imwrite(out_path.string(), slice))
			{
				// 如果 gif 保存失败（常见于标准 OpenCV 构建），回退到 png
				// 注意：如果回退到 png，Hex Hack 将不适用，需要手动用工具转换
				std::cout << "[Warning] GIF write failed (missing backend?), saving as PNG: " << filename << std::endl;
				filename = "slice_" + std::to_string(i + 1) + ".png";
				out_path = output_dir / filename;
				imwrite(out_path.string(), slice);
			}
		}
		catch (...)
		{
			std::cerr << "[Error] Exception during saving." << std::endl;
		}

		// 5. 应用 Steam Hex Hack
		// 只有当文件是 GIF 时才应用此 Hack
		if (out_path.extension() == ".gif")
		{
			apply_steam_hex_hack(out_path.string());
		}
	}

	std::cout << "[Done] Slices generated in " << output_dir << std::endl;
	return 0;
}
