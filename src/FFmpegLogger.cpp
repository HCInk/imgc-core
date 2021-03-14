#include <torasu/torasu.hpp>

#include "../include/torasu/mod/imgc/FFmpegLogger.h"

#include <iostream>
#include <mutex>
#include <map>
#include <exception>


#define SANITY_CHECK true

namespace imgc {

std::string FFmpegLogger::formatLine(void* avcl, int level, const char* fmt, va_list vl, size_t strMaxize) {
	size_t bufSize = strMaxize + 1;
	char* line = new char[bufSize];
	std::fill_n(line, bufSize, 0x00);
	int print_prefix = 1;
	av_log_format_line(avcl, level, fmt, vl, line, strMaxize, &print_prefix);
	std::string formatted(line);
	delete[] line;

	// Remove newline at end
	if (formatted.c_str()[formatted.length()-1] == '\n') {
		formatted.erase(formatted.length()-1);
	}

	return formatted;
}

torasu::LogLevel FFmpegLogger::getLogLevel(int ffmpegLevel) {
	if (ffmpegLevel <= AV_LOG_FATAL) {
		return torasu::SERVERE_ERROR;
	} else if (ffmpegLevel <= AV_LOG_ERROR) {
		return torasu::ERROR;
	} else if (ffmpegLevel <= AV_LOG_WARNING) {
		return torasu::WARN;
	} else if (ffmpegLevel <= AV_LOG_INFO) {
		return torasu::INFO;
	} else if (ffmpegLevel <= AV_LOG_VERBOSE) {
		return torasu::DEBUG;
	} else {
		return torasu::TRACE;
	}
}

void FFmpegLogger::logMessageChecked(torasu::LogInstruction li, void* avcl, int ffmpegLevel, const char* fmt, va_list vl, size_t strMaxize) {
	auto torasuLevel = getLogLevel(ffmpegLevel);
	if (li.level <= torasuLevel) 
		li.logger->log(torasuLevel, FFmpegLogger::formatLine(avcl, ffmpegLevel, fmt, vl, strMaxize));
}

class FFmpegLogManager {
private:
	std::mutex callbackLock;
	std::map<void*, FFmpegCallbackFunc> callbacks;
public:
	FFmpegLogManager();

	int add(void* avcl, FFmpegCallbackFunc callback) {
		std::unique_lock lck(callbackLock);
#if	SANITY_CHECK
		if (callbacks.contains(avcl)) return -3;
#endif
		callbacks[avcl] = callback;
		return 0;
	};

	int remove(void* avcl) {
		std::unique_lock lck(callbackLock);
#if	SANITY_CHECK
		if (!callbacks.contains(avcl)) return -4;
#endif
		callbacks.erase(avcl);
		return 0;
	};

	void process(void* avcl, int level, const char* fmt, va_list vl) {
		std::unique_lock lck(callbackLock);

		auto found = callbacks.find(avcl);

		if (found != callbacks.end()) {
			found->second(avcl, level, fmt, vl);
		} else {
			if (avcl == nullptr && level <= AV_LOG_VERBOSE) return;
			std::cout << "FFmpeg: " << FFmpegLogger::formatLine(avcl, level, fmt, vl, 500) << std::endl;
		}
	}
};

static FFmpegLogManager logger;

namespace {
void processCallback(void* avcl, int level, const char* fmt, va_list vl) {
	logger.process(avcl, level, fmt, vl);
}
} // namespace

FFmpegLogManager::FFmpegLogManager() {
	av_log_set_callback(processCallback);
}

FFmpegLogger::FFmpegLogger() {}

FFmpegLogger::FFmpegLogger(void* avcl, FFmpegCallbackFunc callback) {
	attach(avcl, callback);
}

void FFmpegLogger::attach(void* avcl, FFmpegCallbackFunc callback) {
	if (attached) detach();
	this->avcl = avcl;
	int ret = ffmpeg_add_log_callback_func(avcl, callback);
	if (ret >= 0) {
		attached = true;
	} else {
		throw std::runtime_error("Failed to attach ffmpeg-logger: " + ret);
	}
}

void FFmpegLogger::detach() {
	int ret = imgc_ffmpeg_remove_log_callback(avcl);
	if (ret >= 0)  {
		attached = false;
	} else {
		throw std::runtime_error("Failed to remove ffmpeg-logger: " + ret);
	}
}

FFmpegLogger::~FFmpegLogger() {
	if (attached) detach();
}

int ffmpeg_add_log_callback_func(void* avcl, FFmpegCallbackFunc callback) {
	return logger.add(avcl, callback);
}

} // namespace imgc

int imgc_ffmpeg_add_log_callback(void* avcl, void(*callback)(void*, int, const char*, va_list)) {
	return imgc::logger.add(avcl, [callback] (void* avcl, int level, const char* message, va_list list) {
		callback(avcl, level, message, list);
	});
}

int imgc_ffmpeg_remove_log_callback(void* avcl) {
	return imgc::logger.remove(avcl);
}
