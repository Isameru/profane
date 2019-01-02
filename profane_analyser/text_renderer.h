
#pragma once

#include "pch.h"

std::string FormatDuration(std::chrono::nanoseconds duration, int8_t significantDigits);
std::string FormatDuration(int64_t durationNs, int8_t significantDigits);
std::string FormatTimePoint(int64_t timeNs);

class TextRenderer
{
    constexpr static int64_t FramesPerCollect = 30;
    constexpr static int64_t FramesToRetire = 60;

    struct Inscription
    {
        std::unique_ptr<SDL_Texture, decltype(&SDL_DestroyTexture)> texture;
        int64_t renderedFrameIdx;
    };

    SDL_Renderer* m_renderer;
    TTF_Font* m_font = nullptr;
    int64_t m_frameIdx = -1;
    std::map<std::string, Inscription> m_inscriptions;

public:
    TextRenderer(SDL_Renderer* renderer);
    SDL_Texture* PrepareText(const std::string& text);
    SDL_Rect RenderText(int x, int y, SDL_Texture* texture, SDL_Color color = SDL_Color{255, 255, 255, 255});
    SDL_Rect RenderText(int x, int y, const std::string& text, SDL_Color color = SDL_Color{255, 255, 255, 255});
    void OnUpdate();

private:
    void Collect();
};
