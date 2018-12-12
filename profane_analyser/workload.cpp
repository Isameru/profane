
#include "pch.h"
#include "workload.h"

/*
struct WorkItem
{
uint64_t startTimeNs;
uint64_t stopTimeNs;
uint32_t categoryNameIdx;
uint32_t workerNameIdx;
uint32_t routineNameIdx;
uint32_t commentNameIdx;
uint32_t taskId;
};


std::vector<WorkItem> workItems;
std::map<uint32_t, std::string> dictionary;
*/

void UpdateStackLevel(Workload::Worker& worker)
{
    std::vector<uint64_t> endTimesStack;

    for (auto& workItem : worker.workItems)
    {
        for (int stackLevel = 0; true; ++stackLevel)
        {
            if (stackLevel <= static_cast<int>(endTimesStack.size()))
                endTimesStack.push_back(0);

            if (workItem.startTimeNs >= endTimesStack[stackLevel])
            {
                endTimesStack[stackLevel] = workItem.stopTimeNs;
                workItem.stackLevel = stackLevel;
                break;
            }
        }
    }
}

Workload BuildWorkload(profane::bin::FileContent&& fileContent)
{
    Workload workload;

    workload.dictionary = std::move(fileContent.dictionary);
    const auto& dictionary = workload.dictionary;

    //workload.workItems.reserve(fileContent.workItems.size());

    for (const auto& workItem : fileContent.workItems)
    {
        const char* const workerName = dictionary[workItem.workerNameIdx].c_str();
        auto worker_iter = workload.workers.find(workerName);
        if (worker_iter == std::end(workload.workers))
            worker_iter = workload.workers.insert(std::make_pair(workerName, Workload::Worker{workerName})).first;
        Workload::Worker& worker = worker_iter->second;

        const char* const routineName = dictionary[workItem.routineNameIdx].c_str();
        worker.workItems.push_back(Workload::WorkItem{
            routineName,
            workItem.startTimeNs,
            workItem.stopTimeNs,
            0
        });
    }

    for (auto& workerKV : workload.workers)
    {
        UpdateStackLevel(workerKV.second);
    }

    return workload;
}
