
#include "pch.h"
#include "utils.h"

SDL_Color LerpColor(SDL_Color a, SDL_Color b, float ratio) noexcept
{
    return SDL_Color{
        static_cast<uint8_t>(static_cast<float>(a.r) * (1.0f - ratio) + static_cast<float>(b.r) * ratio),
        static_cast<uint8_t>(static_cast<float>(a.g) * (1.0f - ratio) + static_cast<float>(b.g) * ratio),
        static_cast<uint8_t>(static_cast<float>(a.b) * (1.0f - ratio) + static_cast<float>(b.b) * ratio),
        static_cast<uint8_t>(static_cast<float>(a.a) * (1.0f - ratio) + static_cast<float>(b.a) * ratio)
    };
}

SDL_Color LerpColor(SDL_Color a, SDL_Color b, SDL_Color c, float ratio) noexcept
{
    if (ratio < 0.5f)
        return LerpColor(a, b, 2.0f * ratio);
    else
        return LerpColor(b, c, 2.0f * (ratio - 0.5f));
}
