
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
        SDL_Renderer* const m_renderer;
        const SDL_Color m_blockBorderColor;
        bool m_onset;
        int m_leftPx;
        int m_rightPx;
        int m_topPx;

    public:
        PixelWideBlockDeferredRenderer(SDL_Renderer* renderer, SDL_Color blockBorderColor);
        void Reset();
        void MarkBlock(int leftPx, int rightPx, int topPx);
        void Render();
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
