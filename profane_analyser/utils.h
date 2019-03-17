
#pragma once

#include "pch.h"

SDL_Color LerpColor(SDL_Color a, SDL_Color b, float ratio) noexcept;
SDL_Color LerpColor(SDL_Color a, SDL_Color b, SDL_Color c, float ratio) noexcept;
