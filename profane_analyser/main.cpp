
#include "pch.h"
#include "text_renderer.h"
#include "workload.h"
#include "time_scale_view.h"

std::unique_ptr<Workload> workload;
PerfLogger* perfLogger;

namespace sdl {
    class LibraryRuntime {
    public:
        LibraryRuntime()
        {
            SDL_Init(SDL_INIT_VIDEO);
            IMG_Init(IMG_INIT_PNG);
            TTF_Init();
        }

        ~LibraryRuntime()
        {
            TTF_Quit();
            IMG_Quit();
            SDL_Quit();
        }
    };
}

class GameApp
{
    bool m_quitRequested = false;
    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;
    SDL_Surface* m_screen = nullptr;
    SDL_Texture* m_texture = nullptr;
    std::unique_ptr<TextRenderer> m_textRenderer;
    std::unique_ptr<TimeScaleView> m_timeScaleView;
    TimeScaleCamera timeScaleCamera;

public:
    GameApp()
    {
        m_window = SDL_CreateWindow("Profane Analyser", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        m_screen = IMG_Load("assets/ground_tiles.png");
        m_texture = SDL_CreateTextureFromSurface(m_renderer, m_screen);

        m_textRenderer = std::make_unique<TextRenderer>(m_renderer);
        m_timeScaleView = std::make_unique<TimeScaleView>(m_renderer, *m_textRenderer, *workload);
    }

private:
    void HandleEvent(const SDL_Event& generalEvent)
    {
        PERFTRACE("Main.HandleEvent");
        m_timeScaleView->HandleEvent(generalEvent);
    }

    void OnTick()
    {
        PERFTRACE("Main.OnTick");

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            HandleEvent(event);
            if (event.type == SDL_QUIT)
                m_quitRequested = true;
        }

        PERFTRACE("Main.Draw");

        SDL_SetRenderDrawColor(m_renderer, 32, 32, 32, 255);
        SDL_RenderClear(m_renderer);

        if (workload)
        {
            m_textRenderer->OnUpdate();
            PERFTRACE("Main.TimeScaleView::Draw");
            m_timeScaleView->Draw();
        }

        PERFTRACE("Main.SDL_RenderPresent");
        SDL_RenderPresent(m_renderer);
    }

public:
    void Run()
    {
        PERFTRACE("Main.Run");

#ifdef __EMSCRIPTEN__
        emscripten_set_main_loop_arg([](void* ctx) {
            reinterpret_cast<GameApp*>(ctx)->OnTick();
        }, this, 0, 1);
#else
        while (!m_quitRequested)
        {
            OnTick();
        }
#endif
    }
};

int main(int argc, char* args[])
{
    std::ofstream outFile;
    PerfLogger perfLoggerInstance;
    perfLogger = &perfLoggerInstance;

    constexpr char fileName[] = "perflog.bin";

    perfLogger->Enable(fileName, 32 * 1024);
    PERFTRACE("Main.main");

    for (int i = 0; i < 1000; ++i)
    {
        PERFTRACE("Main.test");
    }

    {
        std::ifstream inFile{fileName, std::ifstream::binary};

        if (inFile.is_open())
        {
            profane::bin::FileContent content;

            {
                PERFTRACE("Main.profane::bin::Read");
                content = profane::bin::Read(inFile);
            }

            PERFTRACE("Main.BuildWorkload");
            workload.reset(new Workload{BuildWorkload(std::move(content))});
        }
    }

    sdl::LibraryRuntime sdl;
    GameApp gameApp;
    gameApp.Run();

    return 0;
}
