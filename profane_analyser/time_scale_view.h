
#pragma once

#include "pch.h"
#include "text_renderer.h"
#include "workload.h"

class TimeScaleView
{
    struct Camera
    {
        int64_t leftNs = 0;
        int64_t widthNs = 10000000000;
        int topPx = 0;
        int rendererWidth = 800;

        int64_t NsToPx(int64_t ns);
        int64_t PxToNs(int x);
        void ResetToViewAllWorkload(const Workload& workload);
    };

    class PixelWideBlockDeferredRenderer
    {
        struct Level
        {
            bool onset;
            int leftPx;
            int rightPx;
        };

        SDL_Renderer* const m_renderer;
        const SDL_Color m_blockBorderColor;

        int m_topPx = 0;
        std::vector<Level> m_levels;

    public:
        PixelWideBlockDeferredRenderer(SDL_Renderer* renderer, SDL_Color blockBorderColor);
        void Reset(int topPx, int levelCount);
        void MarkBlock(int leftPx, int rightPx, int levelIdx);
        void Render(int levelIdx);
        void RenderAll();
    };

    SDL_Renderer* m_renderer;
    TextRenderer& m_textRenderer;
    Workload* m_workload;
    Camera m_camera;
    PixelWideBlockDeferredRenderer m_pixelWideBlockDeferredRenderer;

public:
    TimeScaleView(SDL_Renderer* renderer, TextRenderer& textRenderer, Workload& workload);
    void HandleEvent(const SDL_Event& generalEvent);
    void Draw();

private:
    struct TimeScaleRuler
    {
        int64_t firstLabelNs;
        int64_t spacingNs;
    };

    TimeScaleRuler FitTimeScaleRuler();
};
