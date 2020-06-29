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
#include <torasu/mod/imgc/Rvideo_file.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <thread>
#include "torasu/mod/imgc/VideoFileDeserializer.hpp"
#include <fstream>

using namespace std;
using namespace torasu;
using namespace torasu::tstd;
using namespace imgc;

// variable declarations
struct audio_state {
	Uint8* audio_pos; // global pointer to the audio buffer to be played
	Uint32 audio_len; // remaining length of the sample we have to play

};
void my_audio_callback(void* userdata, Uint8* stream, int len);

uint8_t *mixChannels(uint8_t* l, uint8_t* r, size_t nbSamples, size_t sampleSize) {
    uint8_t * out =new uint8_t[nbSamples * sampleSize * 2];
    for (int i = 0; i < nbSamples; ++i) {
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
	len = (len > state->audio_len ? state->audio_len : len);
	SDL_memcpy(stream, state->audio_pos, len);                    // simply copy from one buffer into the other
	// SDL_MixAudio(stream, state->audio_pos, len, SDL_MIX_MAXVOLUME);// mix from one buffer into another
	state->audio_pos += len;
	state->audio_len -= len;
}

void avTest() {
	audio_state* state = new audio_state();
	state->audio_len = 0;
	state->audio_pos = nullptr;
	ofstream stream("test.pcm");
	// VideoFileDeserializer des("/Users/liz3/Desktop/110038564_What_You_Want_Ilkay_Sencan.mp4");
	 VideoFileDeserializer des("/home/liz3/test-videos/143386147_Superstar W.mp4");
	//VideoFileDeserializer des("/home/cedric/Downloads/143386147_Superstar W.mp4");
	std::vector<torasu::tstd::Dbimg*>* firstFrameSeekVidBuffer; // Dummy buffer for now
	torasu::tstd::Dbimg_FORMAT vidFormat(-1, -1);
	auto firstFrameSeek = des.getSegment((SegmentRequest) {
		.start = 0,
		.end = 0.04,
		.videoBuffer = &firstFrameSeekVidBuffer,
		.videoFormat = &vidFormat
	});
	int w = firstFrameSeek->frameWidth;
	int h = firstFrameSeek->frameHeight;
	int frameRate = 25;
	std::vector<std::pair<uint8_t*, Daudio_buffer*>> frames;
	std::vector<uint8_t> audio;
	bool decodingDone = false;
	delete [] firstFrameSeek->vidFrames[0].data;
	delete firstFrameSeek;
	int totalFrames = (des.streams[0]->duration * av_q2d(des.streams[0]->base_time)) * 25;
	auto* rendererThread = new std::thread([&frames, &des, &decodingDone, &totalFrames, &audio, &stream]() {
		auto totalLength = des.streams[0]->duration * av_q2d(des.streams[0]->base_time);
		double i = 0;
		for (int j = 0; j < totalFrames; ++j) {
			std::vector<torasu::tstd::Dbimg*>* vidBuffer; // Dummy buffer for now
			torasu::tstd::Dbimg_FORMAT vidFormat(-1, -1);
			torasu::tstd::Daudio_buffer* audBuffer = NULL;
			torasu::tstd::Daudio_buffer_FORMAT audFormat(44100, torasu::tstd::Daudio_buffer_CHFMT::FLOAT32);
			DecodingState* currentFrame = des.getSegment((SegmentRequest) {
				.start = i+0,
				.end = i+0.04,
				.videoBuffer = &vidBuffer,
				.videoFormat = &vidFormat,
				.audioBuffer = &audBuffer,
				.audioFormat = &audFormat
			});

			frames.push_back(
				std::pair<uint8_t*, Daudio_buffer*>(currentFrame->vidFrames[0].data, NULL));
			// audio-buffer is NULL because we currently have a different audio handling

			auto l = audBuffer->getChannels()[0];
            auto r = audBuffer->getChannels()[1];

			uint8_t *mixed = mixChannels(l.data,r.data, l.dataSize / 4, 4);

			auto* p = &(audio);
            p->insert(p->end(), mixed,  mixed+(l.dataSize * 2));

			delete audBuffer;

			//  delete result;
			i += 0.04;
			delete currentFrame;
		}

		decodingDone = true;
	});
	rendererThread->detach();

	while (!decodingDone)
		std::this_thread::sleep_for(std::chrono::milliseconds(40));
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

	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
	SDL_CreateWindowAndRenderer(w, h, SDL_RENDERER_ACCELERATED, &window, &renderer);
	SDL_SetWindowTitle(window, "Playback test");
	TTF_Font* Sans = TTF_OpenFont("Roboto-Regular.ttf",
								  16);
	SDL_Texture* texture = SDL_CreateTexture(renderer,
						   SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
	SDL_Color White = {255, 255,
					   255
					  };
	SDL_Surface* surfaceMessage = TTF_RenderText_Solid(Sans, "Decoding",
								  White);
	SDL_Texture* Message = SDL_CreateTextureFromSurface(renderer,
						   surfaceMessage);

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point begin2 = std::chrono::steady_clock::now();
	int audioPos = 0;
	int j = 0;
	int t = 0;
	if(j == 0) {
		SDL_AudioSpec spec;
		spec.channels = 2;
		spec.format = AUDIO_F32;
		spec.samples = (audio.size() / 4) - (44100 * 2);
		spec.freq = 44100;
		spec.callback = my_audio_callback;
		state->audio_len = audio.size();
		state->audio_pos = &audio[0];
		spec.userdata = state;
		if (SDL_OpenAudio(&spec, NULL) < 0) return;
		SDL_PauseAudio(0);

	} else {
		std::cout << "lol\n";
	}

	if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
		return;
	while (true) {
		if (j >= totalFrames && decodingDone) {
			SDL_PauseAudio(1);
			break;
		}

		begin = std::chrono::steady_clock::now();

		auto part = frames[j];

		auto p = part.first;
		SDL_UpdateTexture(texture, NULL, &p[0], w * 4);
		SDL_RenderCopy(renderer, texture, NULL, NULL);
		if (!decodingDone) {
			SDL_Rect Message_rect;
			Message_rect.x = 0;
			Message_rect.y = 0;
			Message_rect.w = 200;
			Message_rect.h = 100;
			SDL_RenderCopy(renderer, Message, NULL,
						   &Message_rect);

		}
		SDL_RenderPresent(renderer);
		j++;
		delete [] part.first;
		std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
		t = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
		//   if (t < 40)
		//   SDL_Delay(39 - t);
		std::this_thread::sleep_for(std::chrono::milliseconds(39 - t));
		// SDL_PauseAudio(1);

	}
	audio.clear();
	surfaceMessage = TTF_RenderText_Solid(Sans, "Done",
										  White);
	SDL_DestroyTexture(Message);
	Message = SDL_CreateTextureFromSurface(renderer, surfaceMessage);
	SDL_Rect Message_rect;
	Message_rect.x = 0;
	Message_rect.y = 0;
	Message_rect.w = 200;
	Message_rect.h = 100;
	SDL_RenderCopy(renderer, Message, NULL,
				   &Message_rect);
	SDL_RenderPresent(renderer);
	stream.close();
	while (true)
		if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
			break;

	delete  state;
	SDL_DestroyTexture(Message);
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}

int main() {
	avTest();


	//AUDIO SORTING DEBUG TEST;
	/*    EIcore_runner *runner = new EIcore_runner();
	    ExecutionInterface *ei = runner->createInterface();
	//    Rnet_file file(
	//            "https://cdn.discordapp.com/attachments/609022154213949461/717606443586682889/143386147_Superstar_W.mp4");
	    auto file = Rlocal_file("/Users/liz3/Desktop/Just Pedro - Belong-ZhPxnB9hJE8.mkv");
	    imgc::VideoLoader tree(&file);

	    tree.load(ei);
	    tree.debugPackets();*/

	return 0;
}