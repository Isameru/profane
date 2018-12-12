
#pragma once

#include <map>
#include <chrono>
#include <vector>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <algorithm>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

#include "profane/profane.h"

using Clock = std::chrono::high_resolution_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

using PerfLogger = profane::PerfLogger<profane::ActorBasedTraits>;
extern PerfLogger* perfLogger;

#define CONCATENATE2(a, b) a ## b
#define CONCATENATE(a, b) CONCATENATE2(a, b)
#define PERFTRACE(workerRoutineName) const PerfLogger::Tracer CONCATENATE(_perftracer_, __LINE__) = (perfLogger != nullptr) ? perfLogger->Trace(workerRoutineName) : PerfLogger::Tracer{};
