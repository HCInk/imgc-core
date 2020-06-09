#include <iostream>
#include <../thirdparty/lodepng.h>
#include <torasu/torasu.hpp>
#include <torasu/render_tools.hpp>
#include <torasu/std/pipeline_names.hpp>
#include <torasu/std/context_names.hpp>
#include <torasu/std/EIcore_runner.hpp>
#include <torasu/std/Dbimg.hpp>
#include <torasu/std/Dnum.hpp>
#include <torasu/std/Rnet_file.hpp>
#include <torasu/mod/imgc/Rvideo_file.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <thread>

using namespace std;
using namespace torasu;
using namespace torasu::tstd;
using namespace imgc;

constexpr int w = 1600;
constexpr int h = 900;
constexpr int frameRate = 25;

void avTest() {
    std::vector<uint8_t *> frames;
    bool decodingDone = false;
    int totalFrames = 0;
    auto *rendererThread = new std::thread([&frames, &decodingDone, &totalFrames]() {
        EIcore_runner *runner = new EIcore_runner();
        ExecutionInterface *ei = runner->createInterface();
        Rnet_file file(
                "https://cdn.discordapp.com/attachments/598323767202152458/719988881700945920/110038564_What_You_Want_Ilkay_Sencan.mp4");
        imgc::VideoLoader tree(&file);

        tools::RenderInstructionBuilder rib;

        Dbimg_FORMAT format(w, h);

        auto rf = format.asFormat();

        auto handle = rib.addSegmentWithHandle<Dbimg>(TORASU_STD_PL_VIS, &rf);
        int i = 0;
        while (true) {
            double start = 0.00;
            cout << "RENDER BEGIN" << endl;
            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            start = ((double) i / frameRate) + 0.02;
            Dnum timeBuf(start);
            RenderContext rctx;
            rctx[TORASU_STD_CTX_TIME] = &timeBuf;
            auto result = rib.runRender(&tree, &rctx, ei);
            auto castedRes = handle.getFrom(result);
            ResultSegmentStatus rss = castedRes.getStatus();
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            std::cout << "Time difference = "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "[ms]"
                      << std::endl;
            cout << "RENDER FIN" << endl;
            cout << "STATUS " << rss << endl;
            if (rss >= ResultSegmentStatus::ResultSegmentStatus_OK) {
                Dbimg *bimg = castedRes.getResult();
                uint8_t *d = bimg->getImageData();

                frames.push_back(d);
            }else {
                break;
            }

            //  delete result;
            i++;
        }
        delete ei;
        delete runner;
        totalFrames = i;
        decodingDone = true;
    });
    rendererThread->detach();

    while (frames.size() < 40)
        std::this_thread::sleep_for(std::chrono::milliseconds(40));


    SDL_Event event;
    SDL_Renderer *renderer;
    SDL_Window *window;
    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();
    SDL_CreateWindowAndRenderer(w, h, SDL_RENDERER_ACCELERATED, &window, &renderer);
    SDL_SetWindowTitle(window, "Playback test");
    TTF_Font *Sans = TTF_OpenFont("../assets/Roboto-Regular.ttf",
                                  16);
    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h);
    SDL_Color White = {255, 255,
                       255};

    SDL_Surface *surfaceMessage = TTF_RenderText_Solid(Sans, "Decoding",
                                                       White);

    SDL_Texture *Message = SDL_CreateTextureFromSurface(renderer,
                                                        surfaceMessage);

    std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
    std::chrono::steady_clock::time_point begin2 = std::chrono::steady_clock::now();
    int j = 0;
    while (true) {
        if (j >= totalFrames && decodingDone) break;
        begin = std::chrono::steady_clock::now();
        if (j % frameRate == 0) {
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
            std::cout << "PLAYBACK AFTER 25 FRAMES = "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin2).count() << "[ms]"
                      << std::endl;
            begin2 = std::chrono::steady_clock::now();
            if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
                break;
        }
        auto p = frames[j];
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
        std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
        auto t = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
        std::this_thread::sleep_for(std::chrono::milliseconds(39 - t));

    }
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

    while (true)
        if (SDL_PollEvent(&event) && event.type == SDL_QUIT)
            break;

    SDL_DestroyTexture(Message);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
}

int main() {
    avTest();

    return 0;
}