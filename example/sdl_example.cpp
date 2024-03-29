#include <iostream>
#include <lodepng.h>
#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/EIcore_runner.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Rnet_file.hpp>
#include <torasu/std/Rlocal_file.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <thread>
#include "torasu/mod/imgc/MediaDecoder.hpp"
#include <fstream>

using namespace std;
using namespace torasu;
using namespace torasu::tstd;
using namespace imgc;
using namespace std::chrono;
namespace imgc::example_sdl {

// variable declarations
struct audio_state {
	Uint8* audio_pos; // global pointer to the audio buffer to be played
	Uint32 audio_len; // remaining length of the sample we have to play

};
void my_audio_callback(void* userdata, Uint8* stream, int len);

uint8_t* mixChannels(uint8_t* l, uint8_t* r, size_t nbSamples, size_t sampleSize) {
	uint8_t* out =new uint8_t[nbSamples * sampleSize * 2];
	for (size_t i = 0; i < nbSamples; ++i) {
		std::copy(&l[i *sampleSize], &l[(i * sampleSize) + sampleSize], &out[i * (2*sampleSize)]);
		std::copy(&r[i *sampleSize], &r[(i * sampleSize) + sampleSize], &out[(i * (2*sampleSize)) + sampleSize]);
	}
	return out;
}

void my_audio_callback(void* userdata, Uint8* stream, int len) {
	audio_state* state = static_cast<audio_state*>(userdata);

	if (state->audio_len == 0) {
		return;// simply copy from one buffer into the other
	}
	len = ( (Uint32) len > state->audio_len ? state->audio_len : len);
	SDL_memcpy(stream, state->audio_pos, len);                    // simply copy from one buffer into the other
	// SDL_MixAudio(stream, state->audio_pos, len, SDL_MIX_MAXVOLUME);// mix from one buffer into another
	state->audio_pos += len;
	state->audio_len -= len;
}

void avTest(char* file) {
	audio_state* state = new audio_state();
	state->audio_len = 0;

	state->audio_pos = nullptr;
	MediaDecoder des(file);

	torasu::tstd::Dbimg_sequence* firstFrameSeekVidBuffer;
	torasu::tstd::Daudio_buffer* audioTestSample = NULL;
	size_t currentFrameCount = 0;

	des.getSegment((SegmentRequest) {
		.start = 0,
		.end = 0.01,
		.videoBuffer = &firstFrameSeekVidBuffer,
		.audioBuffer = &audioTestSample
	});
	auto firstFrame = firstFrameSeekVidBuffer->getFrames().begin()->second;
	int w = firstFrame->getWidth();
	int h = firstFrame->getHeight();
	delete firstFrameSeekVidBuffer;
	std::vector<Dbimg*> frames;
	size_t audioLen = 0;
	double frameDuration = (double) des.streams[0]->vid_fps.den / des.streams[0]->vid_fps.num;

	// dont ask.....
//	if(frameDuration == 0.033333333333333333) {
//	    frameDuration = 0.03;
//	}

	size_t totalFrames = (des.streams[0]->duration * av_q2d(des.streams[0]->base_time)) * des.streams[0]->vid_fps.num;
	size_t totalAudioLen = ((des.streams[1]->duration * av_q2d(des.streams[1]->base_time))) * audioTestSample->getChannels()[0].sampleRate *  audioTestSample->getChannels()[0].sampleSize * 2;
	bool decodingDone = false;
	uint8_t* audio = new uint8_t[totalAudioLen];
	bool forceStop = false;
	auto* rendererThread = new std::thread([&frames, &des, &decodingDone, &totalFrames, audio, &audioLen, &currentFrameCount, &frameDuration, &totalAudioLen, &forceStop]() {
		double i = 0;
		for (size_t j = 0; j < totalFrames; ++j) {
			if(forceStop)
				break;
			while(frames.size() -currentFrameCount > 64 && currentFrameCount > 32) {
				if(forceStop)
					break;

			}
			torasu::tstd::Dbimg_sequence* vidBuffer;
			torasu::tstd::Daudio_buffer* audBuffer = NULL;
			des.getSegment((SegmentRequest) {
				.start = i+0,
				.end = (i+frameDuration * 32) >(des.streams[0]->duration * av_q2d(des.streams[0]->base_time)) ? totalFrames * frameDuration : (i+frameDuration * 32),
				.videoBuffer = &vidBuffer,
				.audioBuffer = &audBuffer
			});

			for(auto& frame : vidBuffer->getFrames()) {
				frames.push_back(frame.second);
			}
			// audio-buffer is NULL because we currently have a different audio handling

			auto l = audBuffer->getChannels()[0];
			auto r = audBuffer->getChannels()[1];
			uint8_t* mixed = mixChannels(l.data,r.data, l.dataSize /  audBuffer->getChannels()[0].sampleSize, audBuffer->getChannels()[0].sampleSize);
			std::copy(mixed, mixed+ (audioLen + (l.dataSize*2) > totalAudioLen ? totalAudioLen-audioLen:  (l.dataSize*2)), &audio[audioLen]);
			audioLen += l.dataSize * 2;
			delete[] mixed;
			delete audBuffer;
			if(frames.size() >= totalFrames) {
				break;
			}
			i += frameDuration * 32;
		}

		decodingDone = true;
	});

	SDL_Event event;
	SDL_Renderer* renderer = NULL;
	SDL_Window* window = NULL;

	int ec;
	if ((ec = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO)) < 0) {
		printf("Error: couldn't initialize SDL2. EC %d\n", ec);
		throw runtime_error(string("SDL Error: ") + SDL_GetError());
	}

	if (TTF_Init() < 0) {
		throw runtime_error("Error initializing sdl-TTF");
	}
#ifdef __linux__
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
#endif
	SDL_CreateWindowAndRenderer(w, h, SDL_RENDERER_ACCELERATED, &window, &renderer);
	SDL_SetWindowTitle(window, "Playback test");
	TTF_Font* Sans = TTF_OpenFont("Roboto-Regular.ttf",
								  16);
	SDL_Color White = {255, 255,
					   255
					  };
	SDL_Surface* surfaceMessage = TTF_RenderText_Solid(Sans, "Decoding",
								  White);
	SDL_Texture* texture = SDL_CreateTexture(renderer,
						   SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
	SDL_RenderCopy(renderer, texture, NULL, NULL);

	SDL_Texture* message = SDL_CreateTextureFromSurface(renderer,
						   surfaceMessage);

	SDL_RenderPresent(renderer);
	SDL_PollEvent(&event);
	while (frames.size() < 32)
		std::this_thread::sleep_for(std::chrono::milliseconds(40));

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	int t = 0;
	SDL_AudioDeviceID devid;
	if(currentFrameCount == 0) {
		SDL_AudioSpec spec;
		spec.channels = 2;
		spec.format = AUDIO_F32;
		spec.samples = 2048;
		spec.freq = audioTestSample->getChannels()[0].sampleRate;
		spec.callback = my_audio_callback;
		state->audio_len = totalAudioLen;
		state->audio_pos = audio;
		spec.userdata = state;
		devid = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, SDL_AUDIO_ALLOW_ANY_CHANGE);
		SDL_PauseAudioDevice(devid, 0); /* start audio playing. */


	} else {
		std::cout << "lol\n";
	}

	std::chrono::high_resolution_clock::time_point startTime = high_resolution_clock::now();



	while (true) {
		if (currentFrameCount >= totalFrames && decodingDone) {
			state->audio_pos = &audio[0];
			state->audio_len = totalAudioLen;
			currentFrameCount = 0;
			startTime = high_resolution_clock::now();
		}


		begin = std::chrono::steady_clock::now();

		auto part = frames[currentFrameCount];

		uint8_t* imageData = part->getImageData();
		SDL_UpdateTexture(texture, NULL, imageData, w * 4);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		if (!decodingDone) {
			SDL_Rect Message_rect;
			Message_rect.x = 0;
			Message_rect.y = 0;
			Message_rect.w = 200;
			Message_rect.h = 100;
			SDL_RenderCopy(renderer, message, NULL,
						   &Message_rect);

		}
		SDL_RenderPresent(renderer);
		currentFrameCount++;
//		delete part;
		if (SDL_PollEvent(&event) && event.type == SDL_QUIT) {
			forceStop = true;
			break;
		}

		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		duration<double> abs_elapsed = duration_cast<duration<double>>(t2 - startTime);
		double play_head = frameDuration * currentFrameCount;
		double delay = play_head - abs_elapsed.count();
		if(delay > 0)
			std::this_thread::sleep_for(std::chrono::microseconds((int) (delay * 1000000)));





	}
	SDL_PauseAudio(1);
	SDL_CloseAudioDevice(devid);
	delete[] audio;
	delete state;
	SDL_DestroyTexture(message);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	rendererThread->join();
}

int main(int argc, char** argv) {
	example_sdl::avTest(argv[1]);
	return 0;
}

} // namespace imgc::example_sdl
