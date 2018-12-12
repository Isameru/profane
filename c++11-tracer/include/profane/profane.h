// MIT License
//
// Copyright (c) 2018 Mariusz £apiñski
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <map>
#include <atomic>
#include <chrono>
#include <string>
#include <vector>
#include <cassert>
#include <cstring>
#include <iostream>

namespace profane
{
    #pragma pack(push)
    #pragma pack(1)
    template<typename Clock>
    struct WorkItemProto
    {
        typename Clock::time_point startTime;
        typename Clock::time_point stopTime;
        std::string categoryName;
        std::string workerName;
        std::string routineName;
        std::string comment;
        uint32_t taskId;
    };
    #pragma pack(pop)

    namespace bin
    {
        struct FileContent
        {
            #pragma pack(push)
            #pragma pack(1)
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
            #pragma pack(pop)

            std::vector<WorkItem> workItems;
            std::vector<std::string> dictionary;
        };

        inline FileContent Read(std::istream& in)
        {
            in.seekg(-2 * std::streamoff{sizeof(uint64_t)}, std::ios::end);
            uint64_t workItem0Pos;
            in.read(reinterpret_cast<char*>(&workItem0Pos), sizeof(workItem0Pos));
            uint64_t dictionaryPos;
            in.read(reinterpret_cast<char*>(&dictionaryPos), sizeof(dictionaryPos));

            const auto workItemCount = (dictionaryPos - workItem0Pos) / sizeof(FileContent::WorkItem);

            FileContent content;
            content.workItems.resize(workItemCount);

            in.seekg(workItem0Pos, std::ios::beg);
            in.read(reinterpret_cast<char*>(&content.workItems.front()), workItemCount * sizeof(FileContent::WorkItem));

            in.seekg(dictionaryPos, std::ios::beg);
            uint32_t stringCount;
            in.read(reinterpret_cast<char*>(&stringCount), sizeof(stringCount));

            content.dictionary.resize(stringCount);

            for (uint32_t stringIdx = 0; stringIdx < stringCount; ++stringIdx)
            {
                uint8_t stringLength;
                in.read(reinterpret_cast<char*>(&stringLength), sizeof(stringLength));
                std::string text;
                if (stringLength > 0)
                {
                    text.resize(stringLength);
                    in.read(reinterpret_cast<char*>(&text.front()), std::streamsize{stringLength});
                }
                content.dictionary[stringIdx] = std::move(text);
            }

            return content;
        };

        class BinaryWriter
        {
            std::ostream& m_out;
            std::map<std::string, uint32_t> m_dictionary { std::make_pair(std::string{}, 0) };
            uint64_t m_workItem0Pos;

        public:
            BinaryWriter(std::ostream& out) :
                m_out{out}
            {
                WriteHeader();

                m_workItem0Pos = m_out.tellp();
            }

            void Finish()
            {
                auto dictionaryPos = (uint64_t)WriteDictionaryDestructively();

                Write(m_workItem0Pos);
                Write(dictionaryPos);

                m_out.flush();
            }

            template<typename Clock>
            BinaryWriter& operator<<(WorkItemProto<Clock>&& workItemProto)
            {
                using namespace std::chrono;
                const uint64_t startTimeNs = duration_cast<nanoseconds>(workItemProto.startTime.time_since_epoch()).count();
                const uint64_t stopTimeNs = duration_cast<nanoseconds>(workItemProto.stopTime.time_since_epoch()).count();

                Write(startTimeNs);
                Write(stopTimeNs);
                WriteTextIndexed(std::move(workItemProto.categoryName));
                WriteTextIndexed(std::move(workItemProto.workerName));
                WriteTextIndexed(std::move(workItemProto.routineName));
                WriteTextIndexed(std::move(workItemProto.comment));
                Write(workItemProto.taskId);
                return *this;
            }

        private:
            template<typename T>
            void Write(const T& value)
            {
                m_out.write(reinterpret_cast<const char*>(&value), sizeof(value));
            }

            void WriteTextIndexed(std::string&& text)
            {
                Write(IndexString(std::move(text)));
            }

            void WriteTextImmediate(const std::string& text)
            {
                assert(text.size() <= 255);
                const auto textSize = (uint8_t)text.size();
                Write(textSize);
                if (textSize > 0) {
                    m_out.write(&text.front(), textSize);
                }
            }

            void WriteHeader()
            {
                m_out.write("PROFANE Peformance Logger Binary Data Stream                    ", 64);
            }

            uint32_t IndexString(std::string&& text)
            {
                const auto idx = (uint32_t)m_dictionary.size();
                const auto insertion = m_dictionary.insert(std::make_pair(std::move(text), idx));
                return insertion.first->second;
            }

            std::vector<std::string> OrderDictionaryDestructively()
            {
                std::vector<std::string> orderedDictionary;
                orderedDictionary.resize(m_dictionary.size());

                for (auto& entryKV : m_dictionary)
                {
                    orderedDictionary[entryKV.second] = std::move(entryKV.first);
                }

                m_dictionary.clear();

                return orderedDictionary;
            }

            std::streampos WriteDictionaryDestructively()
            {
                const auto startPos = m_out.tellp();

                auto orderedDictionary = OrderDictionaryDestructively();

                const uint32_t stringCount = (uint32_t)orderedDictionary.size();
                Write(stringCount);

                for (const auto& text : orderedDictionary)
                {
                    WriteTextImmediate(text);
                }

                return startPos;
            }


        };

    } // namespace bin

    struct ActorBasedTraits
    {
        //constexpr static int MaxEventCount = 256*1024;
        using Clock = std::chrono::high_resolution_clock;

        #pragma pack(push)
        #pragma pack(1)
        struct EventData
        {
            const char* workerRoutineName;
            int16_t workerId;
            uint32_t taskId;
        };
        #pragma pack(pop)

        static void OnWorkItem(const EventData& eventData, WorkItemProto<Clock>& workItemProto)
        {
            SplitWorkerRoutineName(eventData.workerRoutineName, workItemProto.workerName, workItemProto.routineName);
        }

    private:
        static void SplitWorkerRoutineName(const char* workerRoutineName, std::string& outWorkerName, std::string& outRoutineName)
        {
            using namespace std;
            auto workerRoutineNameEnd = workerRoutineName + strlen(workerRoutineName);
            auto finding = find(workerRoutineName, workerRoutineNameEnd, '.');
            assert(finding != workerRoutineNameEnd && "workerRoutineName must be in form: <workerName>.<routineName>");
            outWorkerName = string(workerRoutineName, finding);
            outRoutineName = string(finding + 1, workerRoutineNameEnd);
        }
    };

    // Collects the event logs and generates the usable data at the end.
    //
    template<typename Traits>
    class PerfLogger
    {
    private:
        #pragma pack(push)
        #pragma pack(1)
        struct Event
        {
            typename Traits::Clock::time_point startTime;
            typename Traits::Clock::time_point stopTime = {};
            typename Traits::EventData data = {};
        };
        #pragma pack(pop)

    private:
        std::ostream* m_out = nullptr;
        const char* m_outFileName = nullptr;
        typename Traits::Clock::time_point m_startTime;
        std::atomic<uint32_t> m_eventCount = 0;
        std::vector<Event> m_events;

    public:
        class Tracer
        {
            Event* m_event = nullptr;

            Tracer(Event* event) noexcept : m_event{event} {}

        public:
            Tracer() = default;
            Tracer(const Tracer&) = delete;

            Tracer(Tracer&& other) noexcept : m_event{std::exchange(other.m_event, nullptr)} {}

            ~Tracer() noexcept
            {
                TraceStop();
            }

            void Stop() noexcept
            {
                TraceStop();
                m_event = nullptr;
            }

        private:
            void TraceStop() noexcept
            {
                if (m_event != nullptr)
                    m_event->stopTime = typename Traits::Clock::now();
            }

            friend class PerfLogger<Traits>;
        };

    public:
        ~PerfLogger()
        {
            Finish();
        }

        void Enable(std::ostream& out, uint32_t eventCount)
        {
            m_startTime = typename Traits::Clock::now();
            m_events.resize(eventCount);
            m_out = &out;
            assert(m_outFileName == nullptr);
        }

        void Enable(const char* outFileName, uint32_t eventCount)
        {
            m_startTime = typename Traits::Clock::now();
            m_events.resize(eventCount);
            m_outFileName = outFileName;
            assert(m_out == nullptr);
        }

        template<typename... EventDataParams>
        Tracer Trace(EventDataParams&&... eventParams)
        {
            return TraceEvent(typename Traits::EventData{std::forward<EventDataParams>(eventParams)...});
        }

        void Finish()
        {
            auto stopTime = typename Traits::Clock::now();

            std::ofstream outFile;

            if (m_out == nullptr)
            {
                if (m_outFileName == nullptr)
                    return;

                outFile = std::ofstream{m_outFileName, std::ofstream::binary};
                m_out = &outFile;
            }

            const auto eventCount = std::min(m_eventCount.load(), static_cast<uint32_t>(m_events.size()));

            auto writer = bin::BinaryWriter{*m_out};

            for (uint32_t eventIdx = 0; eventIdx < eventCount; ++eventIdx)
            {
                Event& event = m_events[eventIdx];

                if (event.stopTime.time_since_epoch().count() == 0)
                    event.stopTime = stopTime;

                WorkItemProto<typename Traits::Clock> workItemProto{event.startTime, event.stopTime};

                typename Traits::OnWorkItem(event.data, workItemProto);

                writer << std::move(workItemProto);
            }

            writer.Finish();
        }

    private:
        Tracer TraceEvent(typename Traits::EventData&& eventData)
        {
            const auto eventIdx = m_eventCount++;
            if (eventIdx >= m_events.size())
                return {};

            Event& event = m_events[eventIdx];
            event.startTime = typename Traits::Clock::now();
            // event.stopTime is already set to 0
            event.data = std::move(eventData);
            return {&event};
        }
    };

} // namespace profane
