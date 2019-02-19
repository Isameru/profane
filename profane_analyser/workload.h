
#pragma once

#include "pch.h"

struct Workload
{
    struct CStrLess {
        bool operator()(const char* a, const char* b) const { return std::strcmp(a, b) < 0; }
    };

    struct WorkItem
    {
        const char* routineName;
        uint64_t startTimeNs;
        uint64_t stopTimeNs;
        uint8_t stackLevel;

        uint64_t duration() const noexcept { return stopTimeNs - startTimeNs; }
        float durationRatio;
    };

    struct Worker
    {
        const char* name;
        std::vector<WorkItem> workItems;
        uint8_t stackLevels;
    };

    using WorkerMap = std::map<const char*, Worker, CStrLess>;

    std::vector<std::string> dictionary;
    WorkerMap workers;
    int64_t startTimeNs;

    std::map<const char*, std::vector<WorkItem*>> routineToWorkItemHistogramMap;
};

Workload BuildWorkload(profane::bin::FileContent&& fileContent);
