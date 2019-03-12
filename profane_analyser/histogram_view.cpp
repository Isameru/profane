
#include "pch.h"
#include "config.h"
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

    Workload::Worker& worker = m_workload->workers[m_selectedWorkerName];
    const auto& selectedWorkItem = worker.workItems[m_selectedWorkItemIdx];
    auto& histogram = m_workload->routineToWorkItemHistogramMap[selectedWorkItem.routineName];

    const auto longestDuration = histogram[histogram.size() - 1]->duration();

    for (size_t workItemIdx = 0; workItemIdx < histogram.size(); ++workItemIdx)
    {
        const auto* const workItem = histogram[workItemIdx];

        if (workItem == &selectedWorkItem)
            SDL_SetRenderDrawColor(m_renderer, 80, 80, 200, 255);
        else
            SDL_SetRenderDrawColor(m_renderer, 80, 80, 100, 128);

        SDL_Rect rect;
        rect.x = 20 + static_cast<int>(320.0f / static_cast<float>(histogram.size()) * static_cast<float>(workItemIdx));
        rect.w = 340 - rect.x;
        rect.h = static_cast<int>(200.0f / static_cast<float>(longestDuration) * static_cast<float>(workItem->duration()));
        rect.y = rendererHeight - 20 - rect.h;
        SDL_RenderFillRect(m_renderer, &rect);
    }
}

void HistogramView::SelectWorkItem(const char* workerName, int workItemIdx)
{
    m_selectedWorkerName = workerName;
    m_selectedWorkItemIdx = workItemIdx;
}
