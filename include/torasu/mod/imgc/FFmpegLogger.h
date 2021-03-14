#ifndef INCLUDE_TORASU_MOD_IMGC_FFMPEGLOGGER_H_
#define INCLUDE_TORASU_MOD_IMGC_FFMPEGLOGGER_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
#include <libavutil/log.h>

int imgc_ffmpeg_add_log_callback(void* avcl, void(*callback)(void*, int, const char*, va_list));
int imgc_ffmpeg_remove_log_callback(void* avcl);

#ifdef __cplusplus
} // extern "C"

#include <string>
#include <functional>

namespace imgc {

typedef std::function<void(void*, int, const char*, va_list)> FFmpegCallbackFunc;

int ffmpeg_add_log_callback_func(void* avcl, FFmpegCallbackFunc callback);

class FFmpegLogger {
private:
	bool attached = false;
	void* avcl = nullptr;

public:
	FFmpegLogger();
	FFmpegLogger(void* avcl, FFmpegCallbackFunc callback);
	FFmpegLogger& operator=(FFmpegLogger&) = delete;
	FFmpegLogger(FFmpegLogger&) = delete;
	~FFmpegLogger();
	void attach(void* avcl, FFmpegCallbackFunc callback);
	void detach();

	static std::string formatLine(void* avcl, int level, const char* fmt, va_list vl, size_t strMaxize);

#ifdef TORASU_REG_LOGGING
	static torasu::LogLevel getLogLevel(int ffmpegLevel);
	static void logMessageChecked(torasu::LogInstruction li, void* avcl, int ffmpegLevel, const char* fmt, va_list vl, size_t strMaxize);
#endif
};

} // namespace imgc

#endif

#endif // INCLUDE_TORASU_MOD_IMGC_FFMPEGLOGGER_H_
