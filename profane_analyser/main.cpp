
#include "pch.h"
#include "text_renderer.h"
#include "workload.h"
#include "time_scale_view.h"

#include <functional>   // Temporarily here, unless really needed.

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
    std::unique_ptr<TextRenderer> m_textRenderer;
    std::unique_ptr<TimeScaleView> m_timeScaleView;

public:
    GameApp()
    {
        m_window = SDL_CreateWindow("Profane Analyser", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1024, 768, SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

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
            {   PERFTRACE("Main.TextRenderer::OnUpdate");
                m_textRenderer->OnUpdate();
            }

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

    perfLogger->Enable(fileName, 128 * 1024);
    PERFTRACE("Main.main");

    {
        PERFTRACE("Main.test-1");

        constexpr const char* workerRoutineNames[] = {
            "z-1.1", "z-1.2", "z-1.3", "z-1.4", "z-1.5", "z-1.6",
            "z-2.1", "z-2.2", "z-2.3", "z-2.4", "z-2.5", "z-2.6",
            "z-3.1", "z-3.2", "z-3.3", "z-3.4", "z-3.5", "z-3.6",
            "z-4.1", "z-4.2", "z-4.3", "z-4.4", "z-4.5", "z-4.6",
            "z-5.1", "z-5.2", "z-5.3", "z-5.4", "z-5.5", "z-5.6",
            "z-6.1", "z-6.2", "z-6.3", "z-6.4", "z-6.5", "z-6.6",
        };

        std::function<void(int, int)> depthTest;

        depthTest = [&](int level, int phase) {
            PERFTRACE(workerRoutineNames[6 * level + phase]);

            if (level > 0)
                depthTest(level - 1, phase);

            if (phase > 0)
                depthTest(level, phase - 1);
        };

        depthTest(5, 5);
    }

    {
        PERFTRACE("Main.test-2");

        for (int i = 0; i < 1000; ++i)
        {
            PERFTRACE("Main.test");
        }
    }

    {
        std::ifstream inFile{fileName, std::ifstream::binary};

        if (inFile.is_open())
        {
            profane::bin::FileContent content;

            {   PERFTRACE("Main.profane::bin::Read");
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
