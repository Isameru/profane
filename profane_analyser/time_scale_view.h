
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

        int64_t NsToPx(int64_t ns)
        {
            return (ns - leftNs) * rendererWidth / widthNs;
        }

        int64_t PxToNs(int x)
        {
            return leftNs + x * widthNs / rendererWidth;
        }
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
        PixelWideBlockDeferredRenderer(SDL_Renderer* renderer, SDL_Color blockBorderColor) :
            m_renderer{renderer},
            m_blockBorderColor{blockBorderColor}
        {}

        void Reset() {
            m_onset = false;
        }

        void MarkBlock(int leftPx, int rightPx, int topPx)
        {
            assert(rightPx >= leftPx);
            assert(rightPx - leftPx <= 1);

            if (!m_onset)
            {
                m_leftPx = leftPx;
                m_rightPx = rightPx;
                m_topPx = topPx;
                m_onset = true;
            }
            else
            {
                if (topPx == m_topPx && leftPx - m_rightPx <= 1)
                {
                    assert(leftPx >= m_rightPx);
                    m_rightPx = rightPx;
                }
                else
                {
                    Render();
                    MarkBlock(leftPx, rightPx, topPx);
                }
            }
        }

        void Render()
        {
            if (!m_onset) return;

            SDL_SetRenderDrawColor(m_renderer, m_blockBorderColor.r, m_blockBorderColor.g, m_blockBorderColor.b, m_blockBorderColor.a);

            SDL_Rect rect { m_leftPx, m_topPx, std::max(m_rightPx - m_leftPx + 1, 1), 41 };
            SDL_RenderFillRect(m_renderer, &rect);

            m_onset = false;
        }
    };

    SDL_Renderer* m_renderer;
    TextRenderer& m_textRenderer;
    Workload* m_workload;
    Camera m_camera;
    PixelWideBlockDeferredRenderer m_pixelWideBlockDeferredRenderer;

public:
    TimeScaleView(SDL_Renderer* renderer, TextRenderer& textRenderer, Workload& workload) :
        m_renderer{renderer},
        m_textRenderer{textRenderer},
        m_workload{&workload},
        m_pixelWideBlockDeferredRenderer{renderer, SDL_Color{18, 8, 8, 255}}
    {}

    void HandleEvent(const SDL_Event& generalEvent)
    {
        switch (generalEvent.type)
        {
            case SDL_MOUSEMOTION:
            {
                const auto& event = reinterpret_cast<const SDL_MouseMotionEvent&>(generalEvent);

                if (event.state & SDL_BUTTON_RMASK)
                {
                    const auto t1 = m_camera.PxToNs(event.x - event.xrel);
                    const auto t2 = m_camera.PxToNs(event.x);
                    m_camera.leftNs += t1 - t2;
                }

                break;
            }

            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP:
            {
                const auto& event = reinterpret_cast<const SDL_MouseButtonEvent&>(generalEvent);

                break;
            }

            case SDL_MOUSEWHEEL:
            {
                const auto& event = reinterpret_cast<const SDL_MouseWheelEvent&>(generalEvent);

                int rendererWidth, rendererHeight;
                SDL_GetRendererOutputSize(m_renderer, &rendererWidth, &rendererHeight);

                int mouseX, mouseY;
                SDL_GetMouseState(&mouseX, &mouseY);

                int64_t pointedTimeX = m_camera.PxToNs(mouseX);
                double pointedToLeftRatio = (double)mouseX / (double)rendererWidth;
                double visibleTimeRadius = (double)m_camera.widthNs;

                visibleTimeRadius = ((double)(visibleTimeRadius) * std::pow(0.75, event.y));
                visibleTimeRadius = std::max(visibleTimeRadius, 1000.0);
                m_camera.leftNs = pointedTimeX - int64_t(pointedToLeftRatio * visibleTimeRadius);
                m_camera.widthNs = (int64_t)(visibleTimeRadius);

                break;
            }
        }
    }

    void Draw()
    {
        int rendererWidth, rendererHeight;
        SDL_GetRendererOutputSize(m_renderer, &rendererWidth, &rendererHeight);

        m_camera.rendererWidth = rendererWidth;


        SDL_Rect r0 { 0, 0, rendererWidth, 20 };
        SDL_SetRenderDrawColor(m_renderer, 43, 43, 47, 255);
        SDL_RenderFillRect(m_renderer, &r0);

        r0.y = rendererHeight - 20;
        SDL_RenderFillRect(m_renderer, &r0);


        for (int i = 10; i < rendererWidth; i += 200)
        {
            auto tns = m_camera.PxToNs(i);
            m_textRenderer.RenderText(i + 4, 0, FormatDuration(tns, 99).c_str(), SDL_Color{180, 240, 210, 255});

            SDL_SetRenderDrawColor(m_renderer, 180, 240, 210, 255);
            SDL_RenderDrawLine(m_renderer, i, 12, i, 20);
        }


        int mouseX, mouseY;
        SDL_GetMouseState(&mouseX, &mouseY);

        Workload::WorkItem* selectedWorkItem = nullptr;

        auto st = m_workload->workers["Main"].workItems[0].startTimeNs;

        m_pixelWideBlockDeferredRenderer.Reset();

        for (auto& wi : m_workload->workers["Main"].workItems)
        {
            auto startTime = wi.startTimeNs - st;
            auto stopTime = wi.stopTimeNs - st;

            auto leftPx = m_camera.NsToPx(startTime);
            auto rightPx = m_camera.NsToPx(stopTime);

            if (rightPx < 0 || leftPx >= rendererWidth)
                continue;

            leftPx = std::max(leftPx, -1LL);
            rightPx = std::min(rightPx, (int64_t)rendererWidth + 1LL);

            assert(rightPx >= leftPx);

            int blockRect_y = 38 + 40*(int)wi.stackLevel;

            if (rightPx - leftPx <= 1)
            {
                m_pixelWideBlockDeferredRenderer.MarkBlock(static_cast<int>(leftPx), static_cast<int>(rightPx), blockRect_y);
                continue;
            }
            else
            {
                m_pixelWideBlockDeferredRenderer.Render();
            }

            SDL_Rect blockRect { static_cast<int>(leftPx), blockRect_y, static_cast<int>(std::max(rightPx - leftPx + 1, 1LL)), 41 };

            if (selectedWorkItem == nullptr && mouseX >= blockRect.x && mouseY >= blockRect.y && mouseX < blockRect.x + blockRect.w && mouseY < blockRect.y + blockRect.h)
            {
                selectedWorkItem = &wi;
                SDL_SetRenderDrawColor(m_renderer, 143, 81, 75, 255);
            }
            else
            {
                SDL_SetRenderDrawColor(m_renderer, 103, 51, 45, 255);
            }

            if (blockRect.w > 1)
                SDL_RenderFillRects(m_renderer, &blockRect, 1);

            SDL_SetRenderDrawColor(m_renderer, 18, 8, 8, 255);
            SDL_RenderDrawRects(m_renderer, &blockRect, 1);

            if (rightPx - leftPx > 32) {
                m_textRenderer.RenderText(blockRect.x + 4, blockRect.y + 2, wi.routineName, SDL_Color{180, 240, 210, 255});
                m_textRenderer.RenderText(blockRect.x + 4, blockRect.y + 20, FormatDuration(wi.stopTimeNs - wi.startTimeNs, 4).c_str(), SDL_Color{130, 240, 175, 255});
            }
        }

        m_pixelWideBlockDeferredRenderer.Render();

        SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(m_renderer, 180, 240, 210, 135);
        SDL_RenderDrawLine(m_renderer, mouseX, 20, mouseX, rendererHeight - 20);
        m_textRenderer.RenderText(mouseX + 3, 20, FormatDuration(m_camera.PxToNs(mouseX), 0).c_str(), SDL_Color{180, 240, 210, 135});
    }
};
