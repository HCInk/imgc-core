#include "../include/torasu/mod/imgc/MediaEncoder.hpp"

namespace imgc {



torasu::tstd::Dfile* MediaEncoder::encode(EncodeRequest request) {

    //
    // Create Context
    //

    uint8_t* alloc_buf = (uint8_t*) av_malloc(32 * 1024);
	AVFormatContext* av_format_ctx = avformat_alloc_context();
	if (!av_format_ctx) {
		throw std::runtime_error("Failed allocating av_format_ctx!");
	}

    void* opaque = NULL;
	av_format_ctx->pb = avio_alloc_context(alloc_buf, 32 * 1024, true, opaque, ReadFunc, NULL, SeekFunc);

	if (!av_format_ctx->pb) {
		av_free(alloc_buf);
		throw std::runtime_error("Failed allocating avio_context!");
	}
	av_format_ctx->flags = AVFMT_FLAG_CUSTOM_IO;

    //
    // Cleanup
    //
    
    av_free(alloc_buf);
}

} // namespace imgc
