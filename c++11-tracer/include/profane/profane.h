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
    namespace detail
    {
        template<typename T, typename OtherT>
        T exchange(T& v, OtherT&& other)
        {
            auto v_copy = std::move(v);
            v = std::forward<OtherT>(other);
            return v_copy;
        }
    }

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
        constexpr uint32_t FormatVersion = 2;

        using StringIdx = uint32_t;

        #pragma pack(push)
        #pragma pack(1)
        struct FileHeader
        {
            const char headerText[128] =
                "PROFANE Peformance Logger Binary Data Stream                   \n"
                "                                                              \n";
            constexpr static auto HeaderTextSize = sizeof(headerText) * sizeof(char);
        };
        static_assert(sizeof(FileHeader) == 128, "profane::bin::File Header is expected to be 128 bytes long");

        struct SectionHeader
        {
            uint64_t dictionaryPos          = -1;
            uint64_t nextSectionPos         = -1;
        };

        struct ManifestSectionHeader : public SectionHeader
        {
            const uint32_t formatVersion    = FormatVersion;
            StringIdx programNameIdx        = 0;
            StringIdx descriptionIdx        = 0;
            //uint64_t dateTime;
        };

        struct WorkItemArraySectionHeader : public SectionHeader
        {
            uint32_t workItemCount          = 0;
        };

        struct WorkItem
        {
            uint64_t startTimeNs;
            uint64_t stopTimeNs;
            StringIdx categoryNameIdx;
            StringIdx workerNameIdx;
            StringIdx routineNameIdx;
            StringIdx commentNameIdx;
            uint32_t taskId;
        };
        #pragma pack(pop)

        struct FileContent
        {
            std::vector<std::string> dictionary;
            StringIdx programNameIdx = 0;
            StringIdx descriptionIdx = 0;
            std::vector<WorkItem> workItems;
        };

        // TODO: Protect this function against corrupted/malicious data.
        // TODO: Add simple warning message in case of partial data retrieval.
        inline FileContent Read(std::istream& in)
        {
            FileContent content;
            // String at index 0 is always an empty string.
            content.dictionary.push_back(std::string{});

            auto readDictionary = [&](std::streampos pos) {
                in.seekg(pos);

                uint32_t stringCount;
                in.read(reinterpret_cast<char*>(&stringCount), sizeof(stringCount));

                content.dictionary.reserve(content.dictionary.size() + stringCount);

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
                    content.dictionary.push_back(std::move(text));
                }
            };

            in.seekg(std::streamoff{sizeof(FileHeader)}, std::ios::beg);

            ManifestSectionHeader manifest;
            in.read(reinterpret_cast<char*>(&manifest), sizeof(manifest));
            content.programNameIdx = manifest.programNameIdx;
            content.descriptionIdx = manifest.descriptionIdx;

            readDictionary(manifest.dictionaryPos);

            std::streampos sectionPos = manifest.nextSectionPos;

            while (sectionPos != -1)
            {
                in.seekg(sectionPos);

                WorkItemArraySectionHeader section;
                in.read(reinterpret_cast<char*>(&section), sizeof(section));

                if (section.workItemCount > 0)
                {
                    const auto origWorkItemCount = content.workItems.size();
                    content.workItems.resize(origWorkItemCount + section.workItemCount);
                    in.read(reinterpret_cast<char*>(&content.workItems[origWorkItemCount]), section.workItemCount * sizeof(WorkItem));

                    readDictionary(section.dictionaryPos);
                }

                sectionPos = section.nextSectionPos;
            }

            return content;
        };

        class BinaryWriter
        {
            // Output stream
            std::ostream& m_out;
            // Strings already serialized
            std::map<std::string, uint32_t> m_savedDictionary { std::make_pair(std::string{}, 0) };
            // Strings to be serialized upon next section closure
            std::map<std::string, uint32_t> m_dictionary;
            // Output position of current section header
            std::streampos m_lastSectionPos = -1;
            // Work items in current section
            std::vector<WorkItem> m_workItems;

        public:
            // Number of work items cached before writing them to the output
            size_t WorkItemsPerSection = 8 * 1024;

            BinaryWriter(std::ostream& out, const std::string& programName, const std::string& description) :
                m_out{out}
            {
                assert(programName.size() <= 255);
                assert(description.size() <= 255);

                WriteHeader();
                WriteManifest(programName, description);
                StartWorkItemArraySection();
            }

            void Finish()
            {
                EndWorkItemArraySection();
                m_out.flush();

                assert(m_dictionary.empty());
                assert(m_workItems.empty());
            }

            template<typename Clock>
            void WriteWorkItem(WorkItemProto<Clock>&& workItemProto)
            {
                using namespace std::chrono;
                const uint64_t startTimeNs = duration_cast<nanoseconds>(workItemProto.startTime.time_since_epoch()).count();
                const uint64_t stopTimeNs = duration_cast<nanoseconds>(workItemProto.stopTime.time_since_epoch()).count();

                m_workItems.push_back(WorkItem{
                    startTimeNs,
                    stopTimeNs,
                    IndexString(std::move(workItemProto.categoryName)),
                    IndexString(std::move(workItemProto.workerName)),
                    IndexString(std::move(workItemProto.routineName)),
                    IndexString(std::move(workItemProto.comment)),
                    workItemProto.taskId});

                if (m_workItems.size() >= WorkItemsPerSection)
                {
                    EndWorkItemArraySection();
                    StartWorkItemArraySection();
                }
            }

        private:
            template<typename T>
            void WriteAtom(const T& value)
            {
                m_out.write(reinterpret_cast<const char*>(&value), sizeof(value));
            }

            void WriteAtom(std::streampos value)
            {
                WriteAtom(static_cast<uint64_t>(value));
            }

            void WriteTextIndexed(std::string&& text)
            {
                WriteAtom(IndexString(std::move(text)));
            }

            uint32_t IndexString(std::string&& text)
            {
                const auto iter_1 = m_savedDictionary.find(text);
                if (iter_1 != std::end(m_savedDictionary)) {
                    return iter_1->second;
                }

                const auto idx = static_cast<uint32_t>(m_savedDictionary.size() + m_dictionary.size());

                const auto iter_2 = m_dictionary.insert(std::make_pair(std::move(text), idx));
                return iter_2.first->second;
            }

            void WriteTextImmediate(const std::string& text)
            {
                assert(text.size() <= 255);
                const auto textSize = static_cast<uint8_t>(text.size());
                WriteAtom(textSize);
                if (textSize > 0) {
                    m_out.write(&text.front(), textSize);
                }
            }

            void WriteHeader()
            {
                FileHeader fileHeader;
                m_out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
            }

            std::streampos WriteManifest(std::string programName, std::string description)
            {
                const auto startPos = m_out.tellp();

                ManifestSectionHeader manifest;
                manifest.programNameIdx = IndexString(std::move(programName));
                manifest.descriptionIdx = IndexString(std::move(description));
                m_out.write(reinterpret_cast<const char*>(&manifest), sizeof(manifest));

                SectionHeader sectionHeader;
                sectionHeader.dictionaryPos = static_cast<uint64_t>(WriteDictionary());
                sectionHeader.nextSectionPos = static_cast<uint64_t>(m_out.tellp());

                m_out.seekp(startPos);
                m_out.write(reinterpret_cast<const char*>(&sectionHeader), sizeof(sectionHeader));

                m_out.seekp(0, std::ios_base::end);

                return startPos;
            }

            void StartWorkItemArraySection()
            {
                m_lastSectionPos = m_out.tellp();
                assert(m_workItems.empty());

                WorkItemArraySectionHeader sectionHeader;
                sectionHeader.dictionaryPos = std::streampos{-1};
                sectionHeader.nextSectionPos = std::streampos{-1};
                sectionHeader.workItemCount = 0;
                m_out.write(reinterpret_cast<const char*>(&sectionHeader), sizeof(sectionHeader));
            }

            void EndWorkItemArraySection()
            {
                assert(m_lastSectionPos != -1);

                if (!m_workItems.empty()) {
                    m_out.write(reinterpret_cast<const char*>(&m_workItems[0]), sizeof(m_workItems[0]) * m_workItems.size());
                }

                WorkItemArraySectionHeader sectionHeader;
                sectionHeader.dictionaryPos = static_cast<uint64_t>(WriteDictionary());
                sectionHeader.nextSectionPos = static_cast<uint64_t>(m_out.tellp());
                sectionHeader.workItemCount = static_cast<uint32_t>(m_workItems.size());

                m_workItems.clear();

                m_out.seekp(m_lastSectionPos);
                m_out.write(reinterpret_cast<const char*>(&sectionHeader), sizeof(sectionHeader));

                m_out.seekp(0, std::ios_base::end);
            }

            std::vector<std::string> FetchOrderedDictionary()
            {
                const auto startIdx = m_savedDictionary.size();

                std::vector<std::string> orderedDictionary;
                orderedDictionary.resize(m_dictionary.size());

                for (auto& entryKV : m_dictionary)
                {
                    assert(entryKV.second >= startIdx);
                    orderedDictionary[entryKV.second - startIdx] = std::move(entryKV.first);
                }

                m_dictionary.clear();

                return orderedDictionary;
            }

            std::streampos WriteDictionary()
            {
                const auto startPos = m_out.tellp();

                auto orderedDictionary = FetchOrderedDictionary();

                const uint32_t stringCount = static_cast<uint32_t>(orderedDictionary.size());
                WriteAtom(stringCount);

                for (uint32_t stringIdx = 0; stringIdx < stringCount; ++stringIdx)
                {
                    WriteTextImmediate(orderedDictionary[stringIdx]);
                    m_savedDictionary.insert(std::make_pair(std::move(orderedDictionary[stringIdx]), static_cast<uint32_t>(m_savedDictionary.size())));
                }

                return startPos;
            }
        };

    } // namespace bin

    struct ActorBasedTraits
    {
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
        std::atomic<uint32_t> m_eventCount = {0};
        std::vector<Event> m_events;

    public:
        class Tracer
        {
            Event* m_event = nullptr;

            Tracer(Event* event) noexcept : m_event{event} {}

        public:
            Tracer() = default;
            Tracer(const Tracer&) = delete;

            Tracer(Tracer&& other) noexcept : m_event{detail::exchange(other.m_event, nullptr)} {}

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
                    m_event->stopTime = Traits::Clock::now();
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
            m_startTime = Traits::Clock::now();
            m_events.resize(eventCount);
            m_out = &out;
            assert(m_outFileName == nullptr);
        }

        void Enable(const char* outFileName, uint32_t eventCount)
        {
            m_startTime = Traits::Clock::now();
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
            auto stopTime = Traits::Clock::now();

            std::ofstream outFile;

            if (m_out == nullptr)
            {
                if (m_outFileName == nullptr)
                    return;

                outFile = std::ofstream{m_outFileName, std::ofstream::binary};
                m_out = &outFile;
            }

            const auto eventCount = std::min(m_eventCount.load(), static_cast<uint32_t>(m_events.size()));

            auto writer = bin::BinaryWriter{*m_out, "Profane Analyser", "The input of the output"};

            for (uint32_t eventIdx = 0; eventIdx < eventCount; ++eventIdx)
            {
                Event& event = m_events[eventIdx];

                if (event.stopTime.time_since_epoch().count() == 0)
                    event.stopTime = stopTime;

                WorkItemProto<typename Traits::Clock> workItemProto{event.startTime, event.stopTime};

                Traits::OnWorkItem(event.data, workItemProto);

                writer.WriteWorkItem(std::move(workItemProto));
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
            event.startTime = Traits::Clock::now();
            // event.stopTime is already set to 0
            event.data = std::move(eventData);
            return {&event};
        }
    };

} // namespace profane
