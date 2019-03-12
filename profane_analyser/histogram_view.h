
#pragma once

#include "pch.h"
#include "text_renderer.h"
#include "workload.h"

class HistogramView
{
    SDL_Renderer* m_renderer;
    TextRenderer& m_textRenderer;
    Workload* m_workload;

    // TODO: optional<Selection>
    const char* m_selectedWorkerName = nullptr;
    int m_selectedWorkItemIdx = -1;

public:
    HistogramView(SDL_Renderer* renderer, TextRenderer& textRenderer, Workload& workload);
    void HandleEvent(const SDL_Event& generalEvent);
    void Draw();

    void SelectWorkItem(const char* workerName, int workItemIdx);
};
