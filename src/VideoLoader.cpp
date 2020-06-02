#include "../include/torasu/mod/imgc/VideoLoader.hpp"

#include <string>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <istream>
#include <fstream>

#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/Dfile.hpp>
#include <torasu/std/Dbimg.hpp>

#include <lodepng.h>

using namespace std;
using namespace torasu;
using namespace torasu::tstd;

namespace {
	using namespace imgc;
	int ReadFunc(void* ptr, uint8_t* buf, int buf_size) {
		VideoLoader::FileReader* reader = reinterpret_cast<VideoLoader::FileReader*>(ptr);

		size_t nextPos = reader->pos+buf_size;
		size_t read;

		if (nextPos <= reader->size) {
			read = buf_size;
		} else {
			nextPos = reader->size;
			read = nextPos-reader->pos;
		}

		memcpy(buf, reader->data + reader->pos, read);
		//cout << "R " << read << " OF " << buf_size << endl;
		reader->pos+=read;
		if (read > 0) {
			return read;
		} else {
			return -1;
		}
	}
	// whence: SEEK_SET, SEEK_CUR, SEEK_END (like fseek) and AVSEEK_SIZE, which is meant to return the video's size without changing the position
	int64_t SeekFunc(void* ptr, int64_t pos, int whence) {
		VideoLoader::FileReader* reader = reinterpret_cast<VideoLoader::FileReader*>(ptr);
		
		//cout << "SEEK " << pos << " WHENCE " << whence << endl;
		switch (whence) {
		case SEEK_SET:
			reader->pos = pos;
			break;
		case SEEK_CUR:
			reader->pos += pos;
			break;
		case SEEK_END:
			reader->pos = reader->size+pos;
			break;
		case AVSEEK_SIZE:
			//cout << "SEEK SIZE" << endl;
			return reader->size;
		default:
			//cout << "UNSUPP-SEEK " << pos << " WHENCE " << whence << endl;
			break;
		}

		// Return the new position:
		return reader->pos;
	}
}

namespace imgc {


VideoLoader::VideoLoader(torasu::Renderable* source) : tools::SimpleRenderable("Rvideo_file", false, true) {
	this->source = source;
	current_fp.loaded = false;

	av_format_ctx = avformat_alloc_context();

	if (!av_format_ctx) {
		throw runtime_error("Failed allocating av_format_ctx!");
	}


}

VideoLoader::~VideoLoader() {

	if (sws_scaler_ctx != NULL) {
		sws_freeContext(sws_scaler_ctx);
	}
	if (av_frame != NULL) {
		av_frame_free(&av_frame);
	}
	if (av_packet != NULL) {
		av_packet_free(&av_packet);
	}
	if (av_codec_ctx != NULL) {
		avcodec_free_context(&av_codec_ctx);
	}

	if (av_codec_fp_buf != NULL) {
		delete[] av_codec_fp_buf;
	}

	if (input_laoded) {
		avformat_close_input(&av_format_ctx);
	}

	avformat_free_context(av_format_ctx);

	if (sourceFetchResult != NULL) {
		delete sourceFetchResult;
	}

}

ResultSegment* VideoLoader::renderSegment(ResultSegmentSettings* resSettings, RenderInstruction* ri) {
	std::string pName = resSettings->getPipeline();
	if (pName == TORASU_STD_PL_VIS) {
		ExecutionInterface* ei = ri->getExecutionInterface();
		if (!loaded) {
			load(ei);
			loaded = true;
		}

		ResultFormatSettings* format = resSettings->getResultFormatSettings();

		if (format == NULL || format->getFormat().compare("STD::DBIMG") == 0) {
			
			int32_t rWidth, rHeight;
			if (format == NULL) {
				rWidth = -1;
				rHeight = -1;
				cout << "Rvideo_file RENDER autoSize" << endl;
			} else {
				Dbimg_FORMAT* bimgFormat;
				if (!(bimgFormat = dynamic_cast<Dbimg_FORMAT*>(format->getData()))) {
					return new ResultSegment(ResultSegmentStatus_INVALID_FORMAT);
				}
				rWidth = bimgFormat->getWidth();
				rHeight = bimgFormat->getHeight();
				cout << "Rvideo_file RENDER " << rWidth << "x" << rHeight << endl;
			}
			

			Dbimg* resImg = getFrame(0, rWidth, rHeight);

			return new ResultSegment(ResultSegmentStatus::ResultSegmentStatus_OK, resImg, true);

		} else {
			return new ResultSegment(ResultSegmentStatus::ResultSegmentStatus_INVALID_FORMAT);
		}

		return new ResultSegment(ResultSegmentStatus::ResultSegmentStatus_INTERNAL_ERROR);
	} else {
		return new ResultSegment(ResultSegmentStatus::ResultSegmentStatus_INVALID_SEGMENT);
	}
}

void VideoLoader::load(torasu::ExecutionInterface* ei) {

	if (sourceFetchResult != NULL) {
		delete sourceFetchResult;
	}

	tools::RenderInstructionBuilder rib;

	auto handle = rib.addSegmentWithHandle<Dfile>(TORASU_STD_PL_FILE, NULL);

	sourceFetchResult = rib.runRender(source, NULL, ei);

	auto castedResultSeg = handle.getFrom(sourceFetchResult);

	if (castedResultSeg.getResult() == NULL) {
		throw runtime_error("Sub-render of file failed");
	}

	auto data = castedResultSeg.getResult()->getFileData();
	
	in_stream.data = data->data();
	in_stream.size = data->size();
	in_stream.pos = 0;

	// Open file
    /*if (avformat_open_input(&av_format_ctx, "test-res/in.mp4", NULL, NULL) != 0) {
		throw runtime_error("Failed to open input file");
    }*/
	
	uint8_t* alloc_buf = (uint8_t*) av_malloc(alloc_buf_len);

	av_format_ctx->pb = avio_alloc_context(alloc_buf, alloc_buf_len, false, &in_stream, ReadFunc, NULL, SeekFunc);

	if(!av_format_ctx->pb) {
		av_free(alloc_buf);
		throw runtime_error("Failed allocating avio_context!");
	}
	av_format_ctx->flags = AVFMT_FLAG_CUSTOM_IO;
	av_format_ctx->probesize = 1200000;

	// Need to probe buffer for input format unless you already know it
	AVProbeData probeData;
	probeData.buf = alloc_buf;
	probeData.buf_size = alloc_buf_len;
	probeData.filename = "";

	AVInputFormat* av_input_format;// = av_probe_input_format(&probeData, 1);

	//if(!pAVInputFormat)
	av_input_format = av_probe_input_format(&probeData, 1);

	if(!av_input_format) {
		throw runtime_error("Failed to create input-format!");
	}

	av_input_format->flags |= AVFMT_NOFILE;
	
	if (avformat_open_input(&av_format_ctx, "", NULL, NULL) != 0) {
		throw runtime_error("Failed to open input stream");
    }

    // Get infromation about streams
    if (avformat_find_stream_info(av_format_ctx, NULL) < 0) {
		throw runtime_error("Failed to find stream info");
    }

	for (unsigned int i = 0; i < av_format_ctx->nb_streams; i++) {
		AVStream* stream = av_format_ctx->streams[i];
		if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			av_codec_params = stream->codecpar;
			av_codec = avcodec_find_decoder(av_codec_params->codec_id);
			av_codec_video_delay = av_codec_params->video_delay;
			av_codec_fp_buf_len = av_codec_video_delay+1;
			av_codec_fp_buf = new FrameProperties[av_codec_fp_buf_len];
			for (int i = 0; i < av_codec_fp_buf_len; i++) {
				av_codec_fp_buf[i].loaded = false;
			}
			av_codec_fp_buf_pos = 0;

			// Frames per second of the video
			video_framees_per_second = av_q2d(stream->r_frame_rate);
			// Base for video-timestamples
			video_base_time = av_q2d(stream->time_base);

			if (!av_codec) {
				continue;
			}
			video_stream_index = i;

			break;
		}
	}

	if (video_stream_index < 0) {
		throw runtime_error("Failed to find suitable video stream!");
	}

	av_codec_ctx = avcodec_alloc_context3(av_codec);
	if (!av_codec_ctx) {
		throw runtime_error("Failed to allocate av_codec_ctx!");
	}

	if (avcodec_parameters_to_context(av_codec_ctx, av_codec_params) < 0) {
		throw runtime_error("Failed to link parameters to context!");
	}

	if (avcodec_open2(av_codec_ctx, av_codec, NULL) < 0) {
		throw runtime_error("Failed to initialize av_codec_ctx with av_codec!");
	}
	
	width   = av_codec_ctx->width;
	height  = av_codec_ctx->height;

	cout << "GOT VIDEO:" << endl
			<< "FPS	" << video_framees_per_second << endl
			<< "TIME-BASE	" << video_base_time << endl
			<< "RES " << width << "x" << height << endl;
	

	av_frame = av_frame_alloc();
	if (!av_frame) {
		throw runtime_error("Failed to allocate av_frame");
	}

	av_packet = av_packet_alloc();
	if (!av_packet) {
		throw runtime_error("Failed to allocate av_packet");
	}
}


std::map<std::string, Element*> VideoLoader::getElements() {
	throw runtime_error("getElements() not implemented yet!");
}

void VideoLoader::setElement(std::string key, Element* elem) {
	throw runtime_error("setElement(...) not implemented yet!");
}

void VideoLoader::flushBuffers() {

	for (int i = 0; i < av_codec_fp_buf_len; i++) {
		av_codec_fp_buf[i].loaded = false;
	}

	lastReadDts = INT64_MIN;

	avcodec_flush_buffers(av_codec_ctx);
	draining = false;

}

Dbimg* VideoLoader::getFrame(double targetPos, int32_t width, int32_t height) {
	
	
	if (!(current_fp.loaded && current_fp.start <= targetPos && current_fp.start+current_fp.duration > targetPos)) {
		cout << "::NOT SAME FRAME MEM: L" << current_fp.loaded << " S" << current_fp.start << " T" << targetPos << "E" << (current_fp.start+current_fp.duration) << endl;
		bool searchBeginLoc = true;


		
		for (FrameProperties* fp = av_codec_fp_buf; fp < av_codec_fp_buf+av_codec_fp_buf_len;fp++) {
			if (fp->loaded && fp->start <= targetPos && fp->start+fp->duration > targetPos) {
				cout << " ## IN PIPE " << targetPos <<"{" << fp->start << " - " << fp->start+fp->duration<< "}" << endl;
				searchBeginLoc = false;
				break;
			}
		}
		
		
		if (searchBeginLoc) {

			if (current_fp.start <= targetPos) {

				if (draining) {
					
					// Timeline Example
					//
					// WWW                            DDD
					//    K   K   K    K       K        E
					//                         L       P
					//                                 AA
					//
					// A: The closest way to the desired frame is to just continue draining

					cout << " ## CONT DRAIN " << targetPos << endl;
					searchBeginLoc = false; // Signalize that the begin-location has been found

				} else {

					// Timeline Example
					//
					// WWW                            DDD
					//    K   K   K    K       K        E
					//                 L  P
					//                 AAAAABBBBBBBBBBB
					//
					// Check if the keyframe of the requested frame and the keyframe of the playhead are the same (K)
					// A: if yes, skip back to the current playhead (P) and iterate from there
					// B: if not, skip to the according keyframe that was found and iterate from there

					int seekRet = av_seek_frame(av_format_ctx, -1, targetPos*AV_TIME_BASE, AVSEEK_FLAG_BACKWARD); // Skip to the keyframe the requested frame is based on (Which will be probed later)
					cout << " ## PEEK " << targetPos << ">> " << seekRet << " @" << av_format_ctx->pb->pos << endl;
					searchBeginLoc = true; // Signalize that the begin-location still has to be found
					
					if (draining) {
						flushBuffers();
					}

				}

			} else {

				// Timeline Example
				//
				// WWW                            DDD
				//    K   K   K    K       K        E
				//                 L  P
				//    AAAAAAAAAAAAAAAA
				//
				// A: Move playhead to the keyframe of the reuqested frame, since the playhead is already after the requested frame

				int seekRet = av_seek_frame(av_format_ctx, -1, targetPos*AV_TIME_BASE, AVSEEK_FLAG_BACKWARD); // Skip to the keyframe the requested frame is based on
				cout << " ## JUMP " << targetPos << ">> " << seekRet << " @" << av_format_ctx->pb->pos << endl;
				searchBeginLoc = false; // Signalize that the begin-location has been found
				
				if (draining) {
					flushBuffers();
				}

			}

		}

		bool skipNextPacket = false;

		while (true) {
				
			if (!draining) {
				int nextFrameStat = av_read_frame(av_format_ctx, av_packet);
				if (nextFrameStat == AVERROR_EOF) {
					cout << "ENTER DRAIN!" << endl;
					draining = true;
					nextFrameStat = avcodec_send_packet(av_codec_ctx, NULL);
				}
				if (nextFrameStat < 0) {
					char errStr[100];
					av_strerror(nextFrameStat, errStr, 100);
					std::string msgExcept = draining ? "Failed to enter draining: " : "Failed to decode packet: ";
					msgExcept += errStr;
					//cout << "NEXT FRAME STAT E" << nextFrameStat << " " << msgExcept << endl;
					
					throw runtime_error(msgExcept);

				}
			}

			int response;
			
			if (!draining) {

				if (av_packet->stream_index != video_stream_index) {
					// cout << "SKIP PACKET FROM OTHER STREAM " << av_packet->stream_index << " != " << video_stream_index << endl;
					av_packet_unref(av_packet);
					continue;
				}

				if (searchBeginLoc) {

					searchBeginLoc = false;

					double newKeyFrameDts = av_packet->dts;
					if (lastReadDts >= newKeyFrameDts) {
						if (lastReadDts == INT64_MIN) {
							cout << " ## STAY INIT @" << av_format_ctx->pb->pos << endl;
						} else {

							int dtsOffset = av_codec_video_delay/video_base_time/video_framees_per_second;
							cout << "DTS-OFF " << dtsOffset << endl;
							int64_t seekPos = lastReadDts+dtsOffset;
							int seekRet = av_seek_frame(av_format_ctx, video_stream_index, seekPos, AVSEEK_FLAG_ANY);
							cout << " ## BACK " << seekPos << " >> " << seekRet << " [CAUSE: LAST " << lastReadDts << ">=" << newKeyFrameDts << " KEY]" << endl;
							skipNextPacket = true;
							continue;
						}
					}
					
					cout << " ## SKIP TO KEY " << lastReadDts << "=>" << newKeyFrameDts << "@" << av_format_ctx->pb->pos << endl;

				}

				if (skipNextPacket) {
					skipNextPacket = false;
					cout << " ## SKIP PACK" << av_packet->pts << endl;
					continue;
				}

				cout << "PTS " << av_packet->pts << " DTS " << av_packet->dts << " DUR " << av_packet->duration << " POS " << av_packet->pos << endl;
				
				response = avcodec_send_packet(av_codec_ctx, av_packet);

				lastReadDts = av_packet->dts;
				
				// Save packet-metadata into buffer
				FrameProperties* bufObj = NULL;
				for (int i = 0; i < av_codec_fp_buf_len; i++) {
					if ( !(av_codec_fp_buf[i].loaded) ) {
						bufObj = av_codec_fp_buf+i;
						break;
					}
				}

				if (bufObj == NULL) {
					throw runtime_error("Video buffer stuck!");
				}

				bufObj->loaded = true;
				bufObj->start = av_packet->pts*video_base_time;
				// bufObj->duration = av_packet->duration*video_base_time;
				// if (bufObj->duration != 0) {
					// bufObj->duration = 0.4;
					bufObj->duration = 1.0/video_framees_per_second;// FIXME Really calculate duration by time difference between two packets
				
				// 	cerr << "WARN: av_packet->duration unset! (falling back to fps-calculated solution: " << bufObj->duration << ")" << endl;
				// }

				cout << "PACK " << bufObj->start << " DUR " << bufObj->duration << endl;
				//double dur = bufObj->duration;

				cout << ":: ADD TO PIPE[" << av_codec_fp_buf_len << "] ";
				for (FrameProperties* fp = av_codec_fp_buf; fp < av_codec_fp_buf+av_codec_fp_buf_len;fp++) {
					if (fp->loaded) {
						cout << " " <<  fp->start;
					} else {
						cout << " --";
					}
				}
				cout << endl;

				if (response < 0) {
					char errStr[100];
					av_strerror(response, errStr, 100);
					std::string msgExcept = "Failed to decode packet: ";
					msgExcept += errStr;
					throw runtime_error(msgExcept);
				}
			}

			av_codec_fp_buf_pos++;

			if (av_codec_fp_buf_pos >= av_codec_fp_buf_len) {
				av_codec_fp_buf_pos = 0;
			}

			av_packet_unref(av_packet);


			response = avcodec_receive_frame(av_codec_ctx, av_frame);


			if (response == AVERROR(EAGAIN)) {
				cout << "== EAGAIN" << endl;
				//av_seek_frame(av_format_ctx, -1, 0, 0); // Appearently adds two still frames to the start
				continue;
			} else if (response == AVERROR_EOF) {
				//cout << "EOF REC-FRAME" << endl;
				//animate = false;
				throw runtime_error("Requested frame is out of bounds!");
			} else if (response < 0) {
				char errStr[100];
				av_strerror(response, errStr, 100);
				std::string msgExcept = "Failed to recieve frame: ";
				msgExcept += errStr;
				throw runtime_error(msgExcept);
			}

			double packetStart = av_frame->pts*video_base_time;

			FrameProperties* currFP = NULL;
			for (int i = 0; i < av_codec_fp_buf_len; i++) {
				if ( (av_codec_fp_buf[i].loaded) && av_codec_fp_buf[i].start == packetStart) {
					currFP = av_codec_fp_buf+i;
					break;
				}
			}

			if (currFP == NULL) {
				throw runtime_error("Couldnt find injected FrameProperties from buffer!");
			}

			currFP->loaded = false;

			if (currFP->start > targetPos) {
				cout << "  - SKIP TOO LATE FRAME " << currFP->start << endl;
				continue;
			}

			if (currFP->start+currFP->duration < targetPos) {
				cout << "  - SKIP TOO EARLY FRAME " << currFP->start << endl;
				continue;
			}

			cout << "pkt_duration " << av_frame->pkt_duration << " pkt_pos " << av_frame->pkt_pos << " pkt_dts " << av_frame->pkt_dts << endl;
			
			cout << "ld " << currFP->loaded << " start " << currFP->start << " dur " << currFP->duration << endl;
			
			current_fp = *currFP;

			current_fp.loaded = true;

			break;
		}

	} else {
		cout << "::SAME FRAME MEM: L" << current_fp.loaded << " S" << current_fp.start << " T" << targetPos << "E" << (current_fp.start+current_fp.duration) << endl;
	}

	int32_t rWidth, rHeight;
	if (width < 0) {
		if (height < 0) {
			rWidth = av_frame->width;
			rHeight = av_frame->height;
		} else {
			rWidth = height*( static_cast<double>(av_frame->width)/av_frame->height );
			rHeight = height;
		}
	} else {
		if (height < 0) {
			rWidth = width;
			rHeight = width*( static_cast<double>(av_frame->height)/av_frame->width );
		} else {
			rWidth = width;
			rHeight = height;
		}
	}

	sws_scaler_ctx = sws_getContext(av_frame->width, av_frame->height, av_codec_ctx->pix_fmt,
									rWidth, rHeight, AV_PIX_FMT_RGB0,
									SWS_FAST_BILINEAR, NULL, NULL, NULL);
	if (!sws_scaler_ctx) {
		throw runtime_error("Failed to create sws_scaler_ctx!");
	}

	Dbimg* dbimg = new Dbimg(rWidth, rHeight);
	uint8_t* dst[4] = {dbimg->getImageData()->data(), NULL, NULL, NULL};
	int dest_linesize[4] = { rWidth*4, 0, 0, 0 };
	sws_scale(sws_scaler_ctx, av_frame->data, av_frame->linesize, 0, av_frame->height, dst, dest_linesize);

	return dbimg;
}

void VideoLoader::debugPackets() {
	int i = 0;
	int i_v = 0;
	int64_t lastPts = 0;
	int64_t lastDts = 0;
	int64_t lastPackPos = 0;
	
	double lastPos = 0;
	int toSkipNextFrames = 0;
		
	// if (av_format_ctx->iformat->flags & AVFMT_NO_BYTE_SEEK) {
	// 	throw runtime_error("NO BYTE SEEK!"); // yes we appearently cant seek by bytes here
	// }

	int dtsOffset = av_codec_video_delay/video_base_time/video_framees_per_second;
	cout << "DTS-OFF " << dtsOffset << endl;
	while (true) {

		int nextFrameStat = av_read_frame(av_format_ctx, av_packet);
		if (nextFrameStat == AVERROR_EOF) {
			cout << "ENTER DRAIN!" << endl;
			draining = true;
			nextFrameStat = avcodec_send_packet(av_codec_ctx, NULL);
		}
		if (nextFrameStat < 0) {
			char errStr[100];
			av_strerror(nextFrameStat, errStr, 100);
			std::string msgExcept = draining ? "Failed to enter draining: " : "Failed to decode packet: ";
			msgExcept += errStr;
			//cout << "NEXT FRAME STAT E" << nextFrameStat << " " << msgExcept << endl;
			
			throw runtime_error(msgExcept);
		}

		if (av_packet->stream_index == video_stream_index) {
			lastPts = av_packet->pts;
			lastPackPos = av_packet->pos;
			lastDts = av_packet->dts;
			lastPos = lastPts*video_base_time;
			if (toSkipNextFrames > 0) {
				cout << "	( P " << lastPos << " )" << endl;
				toSkipNextFrames--;
				continue;
			}

			cout << i_v << "	P " << lastPos << " [" << lastPts << " @ " << lastDts << " POS " << lastPackPos << "]" << endl;

			// int seekRet = av_seek_frame(av_format_ctx, video_stream_index, lastPts, AVSEEK_FLAG_ANY);
			// cout << " ## SEEK " << lastPts << " >> " << seekRet << endl;
			int64_t seekPos = lastDts+dtsOffset;
			int seekRet = av_seek_frame(av_format_ctx, video_stream_index, seekPos, AVSEEK_FLAG_ANY);
			cout << " ## SEEK " << seekPos << " >> " << seekRet << endl;

			// int seekRet = av_seek_frame(av_format_ctx, -1, i_v*AV_TIME_BASE, AVSEEK_FLAG_FRAME | AVSEEK_FLAG_ANY);
			// cout << " ## SEEK " << i_v << " >> " << seekRet << endl;
			// int seekRet = avformat_seek_file
			// 				(av_format_ctx, video_stream_index, INT64_MIN, lastPackPos, INT64_MAX, AVSEEK_FLAG_BYTE | AVSEEK_FLAG_ANY);
			// cout << " ## SEEK " << lastPackPos << " >> " << seekRet << endl;
			
			toSkipNextFrames = 1;


			// if (lastPts > av_packet->pts) {
			// 	cerr << i_v << "	SMOLLER PTS " << lastPts*video_base_time << ">" << av_packet->pts*video_base_time << endl;
			// }
			//cout << "" << i << "	" << av_packet->pts << endl;
			i_v++;
		}
		i++;
	}
}

// Random access test
void VideoLoader::video_decode_example() {

	int frameNum = 0;
	cout << "FNUM " << frameNum << endl;
	//av_seek_frame(av_format_ctx, video_stream_index, 0, 0);


	double total_duration = ((double)av_format_ctx->streams[video_stream_index]->duration)*video_base_time;

	//av_seek_frame(av_format_ctx, -1, 0, 0);

    // auto stream = av_format_ctx->streams[video_stream_index];
    // avio_seek(av_format_ctx->pb, 0, SEEK_SET);
    // avformat_seek_file(av_format_ctx, video_stream_index, 0, 0, stream->duration, 0);

	int totalFrames = total_duration*50;
	for (int i = 0; i < 100; i++) {

		frameNum = rand() % totalFrames;
		cout << "==== FRAME " << frameNum << " ====" << endl;

		double targetPos = ((double)frameNum/50)+( 1.0 / 100);
		//double targetPos = ((double)frameNum/1)+( 1.0 / 2);
		//double targetPos = total_duration-((double)(frameNum+1)/25)+( 1.0 / 50);
		cout << "TargetPos " << frameNum << " -> " << targetPos << endl;
		if (targetPos >= total_duration || targetPos < 0) {
			cout << "END" << endl;
			break;
		}

		stringstream out_name;
		out_name << "test-res/out";
		out_name << std::setfill('0') << std::setw(5) << frameNum;
		out_name << ".png";

		Dbimg* nextFrame = getFrame(targetPos, -1, -1);

		unsigned error = lodepng::encode(out_name.str(), *(nextFrame->getImageData()), nextFrame->getWidth(), nextFrame->getHeight());

		if (error) {
			cerr << "Failed encoding png: " << lodepng_error_text(error) << endl;
		} else {
			cout << "Saved " << out_name.str() << endl;
		}

		delete nextFrame;

	}
}

} // namespace imgc