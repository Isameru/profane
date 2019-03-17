
#include "pch.h"
#include "config.h"
#include "utils.h"
#include "histogram_view.h"

HistogramView::HistogramView(SDL_Renderer* renderer, TextRenderer& textRenderer, Workload& workload) :
    m_renderer{renderer},
    m_textRenderer{textRenderer},
    m_workload{&workload}
{
}

void HistogramView::HandleEvent(const SDL_Event& generalEvent)
{
}

void HistogramView::Draw()
{
    if (m_selectedWorkerName == nullptr) return;

    int rendererWidth, rendererHeight;
    SDL_GetRendererOutputSize(m_renderer, &rendererWidth, &rendererHeight);

    const int histogramWidth = 360;
    const int histogramHeight = 225;
    const int histogramMargin = 24;
    const int histogramPadding = 8;

    SDL_Rect rect;
    rect.x = histogramMargin;
    rect.w = histogramWidth + 2 * histogramPadding;
    rect.h = histogramHeight + 2 * histogramPadding;
    rect.y = rendererHeight - rect.h - histogramMargin;

    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 80);
    SDL_RenderFillRect(m_renderer, &rect);

    Workload::Worker& worker = m_workload->workers[m_selectedWorkerName];
    const auto& selectedWorkItem = worker.workItems[m_selectedWorkItemIdx];
    auto& histogram = m_workload->routineToWorkItemHistogramMap[selectedWorkItem.routineName];

    const int barWidth = static_cast<int>(static_cast<float>(histogramWidth + (histogram.size() - 1)) / static_cast<float>(histogram.size()));
    const auto longestDuration = histogram[histogram.size() - 1]->duration();

    int selectedWorkItemIdx = -1;

    for (size_t workItemIdx = 0; workItemIdx < histogram.size(); ++workItemIdx)
    {
        const auto* const workItem = histogram[workItemIdx];

        if (workItem == &selectedWorkItem)
            selectedWorkItemIdx = workItemIdx;

        const float colorRatio = workItem->durationOrderRatio;
        auto color = LerpColor(cfg->WorkItemBackgroundColor_Fast, cfg->WorkItemBackgroundColor_Mid, cfg->WorkItemBackgroundColor_Slow, colorRatio);
        SDL_SetRenderDrawColor(m_renderer, color.r, color.g, color.b, color.a);

        SDL_Rect rect;
        rect.x = histogramMargin + histogramPadding + static_cast<int>(static_cast<float>(histogramWidth) / static_cast<float>(histogram.size()) * static_cast<float>(workItemIdx));
        rect.w = barWidth;
        rect.h = static_cast<int>(static_cast<float>(histogramHeight) / static_cast<float>(longestDuration) * static_cast<float>(workItem->duration()));
        rect.y = rendererHeight - histogramMargin - histogramPadding - rect.h;
        SDL_RenderFillRect(m_renderer, &rect);
    }

    {
        assert(selectedWorkItemIdx != -1);
        const auto* const workItem = histogram[selectedWorkItemIdx];

        SDL_SetRenderDrawColor(m_renderer, cfg->MouseMarkerColor.r, cfg->MouseMarkerColor.g, cfg->MouseMarkerColor.b, cfg->MouseMarkerColor.a);

        SDL_Rect rect;
        rect.x = histogramMargin + histogramPadding + static_cast<int>(static_cast<float>(histogramWidth) / static_cast<float>(histogram.size()) * static_cast<float>(selectedWorkItemIdx));
        rect.w = barWidth;
        rect.h = histogramHeight;
        rect.y = rendererHeight - histogramMargin - histogramPadding - rect.h;
        SDL_RenderDrawRect(m_renderer, &rect);
    }
}

void HistogramView::SelectWorkItem(const char* workerName, int workItemIdx)
{
    m_selectedWorkerName = workerName;
    m_selectedWorkItemIdx = workItemIdx;
}
