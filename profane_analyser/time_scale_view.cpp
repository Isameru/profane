
#include "pch.h"
#include "time_scale_view.h"

int64_t TimeScaleView::Camera::NsToPx(int64_t ns)
{
    return (ns - leftNs) * rendererWidth / widthNs;
}

int64_t TimeScaleView::Camera::PxToNs(int x)
{
    return leftNs + x * widthNs / rendererWidth;
}

void TimeScaleView::Camera::ResetToViewAllWorkload(const Workload& workload)
{
    leftNs = 0;
    widthNs = 0;
    for (const auto& workerKV : workload.workers)
    {
        for (const auto& workItem : workerKV.second.workItems)
        {
            widthNs = std::max(widthNs, static_cast<int64_t>(workItem.stopTimeNs - workload.startTimeNs));
        }
    }
}


TimeScaleView::PixelWideBlockDeferredRenderer::PixelWideBlockDeferredRenderer(SDL_Renderer* renderer, SDL_Color blockBorderColor) :
    m_renderer{renderer},
    m_blockBorderColor{blockBorderColor}
{}

void TimeScaleView::PixelWideBlockDeferredRenderer::Reset() {
    m_onset = false;
}

void TimeScaleView::PixelWideBlockDeferredRenderer::MarkBlock(int leftPx, int rightPx, int topPx)
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

void TimeScaleView::PixelWideBlockDeferredRenderer::Render()
{
    if (!m_onset) return;

    SDL_SetRenderDrawColor(m_renderer, m_blockBorderColor.r, m_blockBorderColor.g, m_blockBorderColor.b, m_blockBorderColor.a);

    SDL_Rect rect { m_leftPx, m_topPx, std::max(m_rightPx - m_leftPx + 1, 1), 39 };
    SDL_RenderFillRect(m_renderer, &rect);

    m_onset = false;
}


TimeScaleView::TimeScaleView(SDL_Renderer* renderer, TextRenderer& textRenderer, Workload& workload) :
    m_renderer{renderer},
    m_textRenderer{textRenderer},
    m_workload{&workload},
    m_pixelWideBlockDeferredRenderer{renderer, SDL_Color{18, 8, 8, 255}}
{
    m_camera.ResetToViewAllWorkload(workload);
}

void TimeScaleView::HandleEvent(const SDL_Event& generalEvent)
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

            m_camera.topPx -= event.yrel;
            m_camera.topPx = std::max(m_camera.topPx, 0);
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
        visibleTimeRadius = std::max(visibleTimeRadius, static_cast<double>(MinCameraWidthNs));
        m_camera.leftNs = pointedTimeX - int64_t(pointedToLeftRatio * visibleTimeRadius);
        m_camera.widthNs = (int64_t)(visibleTimeRadius);

        break;
    }
    }
}

void TimeScaleView::Draw()
{
    int rendererWidth, rendererHeight;
    SDL_GetRendererOutputSize(m_renderer, &rendererWidth, &rendererHeight);

    m_camera.rendererWidth = rendererWidth;

    int mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    int workerOffsetY = -m_camera.topPx + 22;

    for (const auto& workerKV : m_workload->workers)
    {
        const auto& worker = workerKV.second;

        m_pixelWideBlockDeferredRenderer.Reset();
        const Workload::WorkItem* selectedWorkItem = nullptr;

        // Draw the worker banner.
        SDL_SetRenderDrawColor(m_renderer, 128, 28, 28, 64);
        SDL_Rect workerBannerRect { 0, workerOffsetY, rendererWidth, 19};
        SDL_RenderFillRect(m_renderer, &workerBannerRect);
        m_textRenderer.RenderText(3, workerOffsetY + 1, worker.name, SDL_Color{175, 125, 125, 255});
        workerOffsetY += 20;

        for (auto& wi : worker.workItems)
        {
            auto startTime = wi.startTimeNs - m_workload->startTimeNs;
            auto stopTime = wi.stopTimeNs - m_workload->startTimeNs;

            auto leftPx = m_camera.NsToPx(startTime);
            auto rightPx = m_camera.NsToPx(stopTime);

            if (rightPx < 0)
                continue;

            if (leftPx >= rendererWidth)
                break;

            leftPx = std::max(leftPx, -1LL);
            rightPx = std::min(rightPx, (int64_t)rendererWidth + 1LL);

            assert(rightPx >= leftPx);

            int blockRect_y = workerOffsetY + 40 * (int)wi.stackLevel;

            if (rightPx - leftPx <= 1)
            {
                m_pixelWideBlockDeferredRenderer.MarkBlock(static_cast<int>(leftPx), static_cast<int>(rightPx), blockRect_y);
                continue;
            }
            else
            {
                m_pixelWideBlockDeferredRenderer.Render();
            }

            SDL_Rect blockRect { static_cast<int>(leftPx), blockRect_y, static_cast<int>(std::max(rightPx - leftPx + 1, 1LL)), 39 };

            if (selectedWorkItem == nullptr && mouseX >= blockRect.x && mouseY >= blockRect.y && mouseX < blockRect.x + blockRect.w && mouseY < blockRect.y + blockRect.h)
            {
                selectedWorkItem = &wi;
                SDL_SetRenderDrawColor(m_renderer, 143, 81, 75, 255);
            }
            else
            {
                SDL_SetRenderDrawColor(m_renderer, 103, 51, 45, 255);
            }

            PERFTRACE("TimeScaleView.Draw WorkItem");

            if (blockRect.w > 1)
                SDL_RenderFillRects(m_renderer, &blockRect, 1);

            SDL_SetRenderDrawColor(m_renderer, 18, 8, 8, 255);
            SDL_RenderDrawRects(m_renderer, &blockRect, 1);

            if (rightPx - leftPx > 32) {
                m_textRenderer.RenderText(blockRect.x + 4, blockRect.y + 2, wi.routineName, SDL_Color{180, 240, 210, 255});
                m_textRenderer.RenderText(blockRect.x + 4, blockRect.y + 20, FormatDuration(wi.stopTimeNs - wi.startTimeNs, 4).c_str(), SDL_Color{130, 240, 175, 255});
            }
        }

        workerOffsetY += 1 + 40 * worker.stackLevels;
    }

    m_pixelWideBlockDeferredRenderer.Render();

    // Draw the time scale top ruler.
    //
    {
        PERFTRACE("TimeScaleView.Draw Ruler");

        // top bg
        SDL_Rect r0 { 0, 0, rendererWidth, 20 };
        SDL_SetRenderDrawColor(m_renderer, 43, 43, 47, 255);
        SDL_RenderFillRect(m_renderer, &r0);

        // bottom bg
        r0.y = rendererHeight - 20;
        SDL_RenderFillRect(m_renderer, &r0);

        // top line
        r0.y = 19;
        r0.h = 1;
        SDL_SetRenderDrawColor(m_renderer, 180, 240, 210, 128);
        SDL_RenderFillRect(m_renderer, &r0);


        const auto ruler = FitTimeScaleRuler();
        auto labelNs = ruler.firstLabelNs;

        while (true)
        {
            const auto labelPx = static_cast<int>(m_camera.NsToPx(labelNs));

            if (labelPx >= rendererWidth)
                break;

            if (labelNs >= 0)
            {
                m_textRenderer.RenderText(labelPx + 4, 3, FormatTimePoint(labelNs).c_str(), SDL_Color{180, 240, 210, 255});

                SDL_SetRenderDrawColor(m_renderer, 180, 240, 210, 255);
                SDL_RenderDrawLine(m_renderer, labelPx, 12, labelPx, 19);
            }

            labelNs += ruler.spacingNs;
        }
    }

    SDL_SetRenderDrawBlendMode(m_renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(m_renderer, 180, 240, 210, 135);
    SDL_RenderDrawLine(m_renderer, mouseX, 20, mouseX, rendererHeight - 20);
    m_textRenderer.RenderText(mouseX + 1, 22, FormatTimePoint(m_camera.PxToNs(mouseX)).c_str(), SDL_Color{180, 240, 210, 135});
}

TimeScaleView::TimeScaleRuler TimeScaleView::FitTimeScaleRuler()
{
    const auto minLabelSpacingNs = static_cast<double>(m_camera.widthNs) / (static_cast<double>(m_camera.rendererWidth) / static_cast<double>(MinTimeScaleLabelWidthPx));

    auto labelSpacingNs = std::pow(10.0, std::ceil(std::log10(minLabelSpacingNs)));

    while (true)
    {
        const auto half = labelSpacingNs / 2.0;

        if (half < minLabelSpacingNs)
            break;

        labelSpacingNs = half;
    }

    const auto cameraLeftPx = m_camera.PxToNs(0);

    const auto firstLabelOffsetPx = std::floor(cameraLeftPx / labelSpacingNs) * labelSpacingNs;

    return { static_cast<int64_t>(firstLabelOffsetPx), static_cast<int64_t>(labelSpacingNs) };
}
