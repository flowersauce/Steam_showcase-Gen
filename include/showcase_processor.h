/**
 * @file showcase_processor.h
 * @brief 异步图像处理类，整合 Steam 特化切片与字节优化逻辑
 */

#ifndef STEAM_SHOWCASE_GEN_SHOWCASE_PROCESSOR_H
#define STEAM_SHOWCASE_GEN_SHOWCASE_PROCESSOR_H

#include <atomic>
#include <filesystem>
#include <functional>
#include <opencv2/core/mat.hpp>
#include <string_view>
#include <thread>

struct AVFormatContext;
struct AVCodecContext;
struct AVStream;
struct AVFrame;
struct SwsContext;

namespace SteamShowcaseGen
{
	/**
	 * @struct EncoderState
	 * @brief 用于管理单个切片编码状态的轻量化数据结构
	 */
	struct EncoderState
	{
		AVFormatContext *fmt_ctx	 = nullptr;
		AVCodecContext	*codec_ctx	 = nullptr;
		AVStream		*stream		 = nullptr;
		AVFrame			*frame		 = nullptr;
		SwsContext		*sws_ctx	 = nullptr;
		int				 frame_count = 0;
	};

	/**
	 * @class ShowcaseProcessor
	 * @brief 负责异步生成 Steam 展柜切片的核心处理器
	 */
	class ShowcaseProcessor
	{
	public:
		using UpdateCallback = std::function<void(std::string_view)>;

		ShowcaseProcessor();
		~ShowcaseProcessor();

		ShowcaseProcessor(const ShowcaseProcessor &)			= delete;
		ShowcaseProcessor &operator=(const ShowcaseProcessor &) = delete;

		void start_task(const std::filesystem::path &source_path,
						const std::filesystem::path &output_dir,
						int							 sampling_rate,
						int							 quality_mode,
						const UpdateCallback		&on_update);
		void stop_task();

		[[nodiscard]] bool is_active() const
		{
			return is_processing_.load();
		}

		/** @brief 静态方法：应用 Steam Hex Hack */
		static bool apply_steam_hex_hack(const std::filesystem::path &file_path);

	private:
		/** @brief 内部执行主循环 */
		void run_internal(const std::stop_token		  &st,
						  const std::filesystem::path &source_path,
						  const std::filesystem::path &output_dir,
						  int						   sampling_rate,
						  int						   quality_mode,
						  const UpdateCallback		  &on_update);

		// FFmpeg 静态辅助方法
		static bool init_encoder(EncoderState &state, const std::string &filename, int width, int height, int fps, int quality_mode);
		static void push_frame(EncoderState &state, const cv::Mat &cv_frame, int height);
		static void encode_raw_frame(const EncoderState &state, const AVFrame *raw_frame);
		static void finish_encoder(EncoderState &state);

		// 常量定义
		static constexpr int STEAM_SHOWCASE_WIDTH = 766;
		static constexpr int SLICE_WIDTH		  = 150;
		static constexpr int GAP_WIDTH			  = 4;
		static constexpr int SLICE_COUNT		  = 5;

		std::jthread	  worker_thread_;
		std::atomic<bool> is_processing_{false};
	};
} // namespace SteamShowcaseGen

#endif // STEAM_SHOWCASE_GEN_SHOWCASE_PROCESSOR_H
