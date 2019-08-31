// MIT License
//
// Copyright (c) 2018-2019 Mariusz £apiñski
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
#include <ctime>
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
        // Replaces the value of obj with new_value and returns the old value of obj.
        // This utility function is available since C++14 (see: https://en.cppreference.com/w/cpp/utility/exchange).
        template<typename T, typename OtherT>
        T exchange(T& obj, OtherT&& new_value)
        {
            auto obj_copy = std::move(obj);
            obj = std::forward<OtherT>(new_value);
            return obj_copy;
        }
    }

    // A prototype of a single work item serialized to a file by the BinaryWriter, understandable by the Profane Analyser.
    // As custom PerfLogger Traits may trace any time-stamped data, finally it must fill up this structure.
    //
    #pragma pack(push)
    #pragma pack(1)
    template<typename Clock>
    struct WorkItemProto
    {
        typename Clock::time_point startTime;   // Time stamp of the work beginning.
        typename Clock::time_point stopTime;    // Time stamp of the work end.
        std::string categoryName;               // Name of the group of workers.
        std::string workerName;                 // Name of the execution thread or actor's handler.
        std::string routineName;                // Name of the function or routine. (routines are stacked within a worker)
        std::string comment;                    // Additional description, comment.
        uint32_t taskId;                        // Numeric identifier of a task or a flow.
    };
    #pragma pack(pop)

    namespace bin
    {
        // The version of binary format. It is written to the manifest section of a file.
        constexpr uint32_t FormatVersion = 3;

        // The string index within a dictionary (which is an array of strings). An index of 0 is always an empty string.
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
        static_assert(sizeof(FileHeader) == 128, "profane::bin::FileHeader is expected to be 128 bytes long");

        struct SectionHeader
        {
            uint64_t dictionaryPos;
            uint64_t nextSectionPos;
        };

        struct ManifestSection : public SectionHeader
        {
            uint32_t formatVersion;
            StringIdx programNameIdx;
            StringIdx descriptionIdx;
            uint64_t dateTime;
        };

        struct WorkItemArraySectionHeader : public SectionHeader
        {
            uint32_t workItemCount;

            uint64_t startTimeNsBase;
            uint64_t durationTimeNsBase;
            StringIdx categoryNameIdxBase;
            StringIdx workerNameIdxBase;
            StringIdx routineNameIdxBase;
            StringIdx commentNameIdxBase;
            uint32_t taskIdBase;

            // Byte sizes of individual attributes (from 0 to 8).
            // 0 means that the attribute is not encoded at all.
            uint8_t startTimeNsSize : 4;
            uint8_t durationTimeNsSize : 4;
            uint8_t categoryNameIdxSize : 4;
            uint8_t workerNameIdxSize : 4;
            uint8_t routineNameIdxSize : 4;
            uint8_t commentNameIdxSize : 4;
            uint8_t taskIdSize : 4;
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
            struct Issue
            {
                std::string code;
                std::string message;
            };

            std::vector<std::string> dictionary { "" };     // String at index 0 in the dictionary is always an empty string.
            StringIdx programNameIdx = 0;
            StringIdx descriptionIdx = 0;
            std::vector<WorkItem> workItems;
            std::vector<Issue> issues;
        };

        // Utility class for packing integers in a space efficient manner.
        // Usage:
        //   1. Instantiate the class, determining whether 0 is an often value.
        //   2. Call Peek() for every value in the packed sequence.
        //   3. Call DeterminePackingSize() to determine the packing size.
        //   4. You may call base() to get the minimal packable value (not including 0 if ZeroIsAbsolute).
        //   5. Call Pack() for every value in the packed sequence.
        //
        template<typename IntT, bool ZeroIsAbsolute>
        class IntBitPacker
        {
            IntT m_base = std::numeric_limits<IntT>::max();
            IntT m_max = 0;
            uint8_t m_packingSize = sizeof(IntT);

        public:
            void Peek(IntT value)
            {
                if (!ZeroIsAbsolute || value != 0)
                    m_base = std::min(m_base, value);
                m_max = std::max(m_max, value);
            }

            IntT base()
            {
                assert(m_base <= m_max && "Call DeterminePackingSize() first");
                return m_base;
            }

            uint8_t DeterminePackingSize()
            {
                if (m_max == 0)
                    m_base = 0;

                auto valueRange = m_max - m_base;

                if (ZeroIsAbsolute && valueRange > 0)
                    ++valueRange;

                uint64_t fullByteValue = 0;
                m_packingSize = 0;

                while (valueRange > fullByteValue)
                {
                    fullByteValue = (fullByteValue << 8) | 0xFF;
                    ++m_packingSize;
                }

                return m_packingSize;
            }

            void Pack(std::ostream& out, IntT value)
            {
                if (m_packingSize == 0)
                    return;

                if (ZeroIsAbsolute) {
                    assert(value == 0 || (value >= m_base && value <= m_max));
                    if (value > 0)
                        value -= m_base - 1;
                }
                else {
                    assert(value >= m_base && value <= m_max);
                    value -= m_base;
                }

                out.write(reinterpret_cast<const char*>(&value), m_packingSize);
            }
        };

        template<typename IntT, bool ZeroIsAbsolute>
        class IntBitUnpacker
        {
            const IntT m_base;
            const uint8_t m_packingSize;

        public:
            IntBitUnpacker(IntT base, uint8_t packingSize) :
                m_base{base},
                m_packingSize{packingSize}
            {}

            IntT Unpack(std::istream& in)
            {
                auto value = IntT{0};

                if (m_packingSize > 0)
                {
                    in.read(reinterpret_cast<char*>(&value), m_packingSize);
                }

                if (ZeroIsAbsolute)
                {
                    if (value != 0)
                        value += m_base - 1;
                }
                else
                {
                    value += m_base;
                }

                return value;
            }
        };

        // TODO: Protect this function against corrupted/malicious data.
        // TODO: Add simple warning message in case of partial data retrieval.
        inline FileContent Read(std::istream& in)
        {
            FileContent content;

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

            in.seekg(std::streamoff{0}, std::ios::beg);

            in.seekg(std::streamoff{sizeof(FileHeader)}, std::ios::beg);

            ManifestSection manifest;
            in.read(reinterpret_cast<char*>(&manifest), sizeof(manifest));
            content.programNameIdx = manifest.programNameIdx;
            content.descriptionIdx = manifest.descriptionIdx;

            readDictionary(manifest.dictionaryPos);

            std::streampos sectionPos = manifest.nextSectionPos;

            while (sectionPos != -1)
            {
                in.seekg(sectionPos);

                WorkItemArraySectionHeader section {};
                in.read(reinterpret_cast<char*>(&section), sizeof(section));

                if (section.workItemCount == 0)
                    break;

                const auto origWorkItemCount = content.workItems.size();
                content.workItems.reserve(origWorkItemCount + section.workItemCount);

                IntBitUnpacker<uint64_t, false> startTimeNsUnpacker { section.startTimeNsBase, section.startTimeNsSize };
                IntBitUnpacker<uint64_t, false> durationNsPacker { section.durationTimeNsBase, section.durationTimeNsSize };
                IntBitUnpacker<StringIdx, true> categoryNameIdxPacker { section.categoryNameIdxBase, section.categoryNameIdxSize };
                IntBitUnpacker<StringIdx, true> workerNameIdxPacker { section.workerNameIdxBase, section.workerNameIdxSize };
                IntBitUnpacker<StringIdx, true> routineNameIdxPacker { section.routineNameIdxBase, section.routineNameIdxSize };
                IntBitUnpacker<StringIdx, true> commentNameIdxPacker { section.commentNameIdxBase, section.commentNameIdxSize };
                IntBitUnpacker<uint32_t, false> taskIdPacker { section.taskIdBase, section.taskIdSize };

                for (uint32_t workItemIdx = 0; workItemIdx < section.workItemCount; ++workItemIdx)
                {
                    const auto startTimeNs = startTimeNsUnpacker.Unpack(in);

                    auto workItem = WorkItem{
                        startTimeNs,
                        startTimeNs + durationNsPacker.Unpack(in),
                        categoryNameIdxPacker.Unpack(in),
                        workerNameIdxPacker.Unpack(in),
                        routineNameIdxPacker.Unpack(in),
                        commentNameIdxPacker.Unpack(in),
                        taskIdPacker.Unpack(in)
                    };

                    content.workItems.push_back(std::move(workItem));
                }

                readDictionary(section.dictionaryPos);

                sectionPos = section.nextSectionPos;
            }

            return content;
        };

        class BinaryWriter
        {
            // Output stream
            std::ostream& m_out;
            // Strings already serialized
            std::map<std::string, StringIdx> m_savedDictionary { std::make_pair(std::string{}, 0) };
            // Strings to be serialized upon next section closure
            std::map<std::string, StringIdx> m_dictionary;
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

            // Adds the work item data to be written to a file.
            // By default the data is not serialized immediately, but rather stored in a vector.
            // When the number of items to be serialized reaches some specific threshold (WorkItemsPerSection), all the items are written to a file, enclosed within a single section.
            //
            template<bool AllowFlushWrite = true, typename Clock>
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

                if (AllowFlushWrite && m_workItems.size() >= WorkItemsPerSection)
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

            // Given a string, it looks for matching string in dictionaries.
            // First in dictionary of already serialized strings, if not found then in dictionary of string to be serialized upon current section closure.
            // If the string is not present in both dictionaries, it is added to the latter dictionary.
            // Returns the unique index of the string in the dictionary.
            //
            StringIdx IndexString(std::string&& text)
            {
                const auto iter_1 = m_savedDictionary.find(text);
                if (iter_1 != std::end(m_savedDictionary)) {
                    return iter_1->second;
                }

                const auto idx = static_cast<StringIdx>(m_savedDictionary.size() + m_dictionary.size());

                const auto iter_2 = m_dictionary.insert(std::make_pair(std::move(text), idx));
                return iter_2.first->second;
            }

            // Writes the string to the file.
            // First comes the byte of string size (0 - 255), then the null-terminated string content.
            // The string may contain multi-byte characters, but the null-termination is a single byte '\0'.
            //
            void WriteTextImmediate(const std::string& text)
            {
                assert(text.size() <= 255);
                const auto textSize = static_cast<uint8_t>(text.size());
                WriteAtom(textSize);
                if (textSize > 0) {
                    m_out.write(&text.front(), textSize);
                }
            }

            // Writes the file header.
            // It starts at the file beginning and has not the form of a section.
            // First 7 bytes are magic: PROFANE
            //
            void WriteHeader()
            {
                FileHeader fileHeader;
                m_out.write(reinterpret_cast<const char*>(&fileHeader), sizeof(fileHeader));
            }

            // Writes the file manifest section describing its content.
            // Returns the file offset of the section beginning.
            //
            std::streampos WriteManifest(std::string programName, std::string description)
            {
                const auto startPos = m_out.tellp();

                // Write the manifest section.
                // This is always the first section of the file and it occurs once.
                //
                ManifestSection manifest;
                manifest.dictionaryPos      = std::streampos{-1};
                manifest.nextSectionPos     = std::streampos{-1};
                manifest.formatVersion      = FormatVersion;
                manifest.programNameIdx     = IndexString(std::move(programName));
                manifest.descriptionIdx     = IndexString(std::move(description));
                manifest.dateTime           = static_cast<uint64_t>(std::time(nullptr));
                m_out.write(reinterpret_cast<const char*>(&manifest), sizeof(manifest));

                // Write the dictionary of the manifest and patch the section header.
                //
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

                WorkItemArraySectionHeader sectionHeader {};
                sectionHeader.dictionaryPos     = std::streampos{-1};
                sectionHeader.nextSectionPos    = std::streampos{-1};
                sectionHeader.workItemCount     = 0;
                // The other member data are irrelevant unless populated by the work items.
                m_out.write(reinterpret_cast<const char*>(&sectionHeader), sizeof(sectionHeader));
            }

            void EndWorkItemArraySection()
            {
                assert(m_lastSectionPos != -1);

                WorkItemArraySectionHeader sectionHeader = WriteWorkItems();

                sectionHeader.dictionaryPos = static_cast<uint64_t>(WriteDictionary());
                sectionHeader.nextSectionPos = static_cast<uint64_t>(m_out.tellp());

                m_workItems.clear();

                m_out.seekp(m_lastSectionPos);
                m_out.write(reinterpret_cast<const char*>(&sectionHeader), sizeof(sectionHeader));

                m_out.seekp(0, std::ios_base::end);
            }

            WorkItemArraySectionHeader WriteWorkItems()
            {
                WorkItemArraySectionHeader sectionHeader;
                sectionHeader.dictionaryPos     = std::streampos{-1};
                sectionHeader.nextSectionPos    = std::streampos{-1};
                sectionHeader.workItemCount     = static_cast<uint32_t>(m_workItems.size());

                IntBitPacker<uint64_t, false> startTimeNsPacker;
                IntBitPacker<uint64_t, false> durationNsPacker;         // When writing to a file, the work item duration is stored instead of stopTime, as it is much smaller.
                IntBitPacker<StringIdx, true> categoryNameIdxPacker;
                IntBitPacker<StringIdx, true> workerNameIdxPacker;
                IntBitPacker<StringIdx, true> routineNameIdxPacker;
                IntBitPacker<StringIdx, true> commentNameIdxPacker;
                IntBitPacker<uint32_t, false> taskIdPacker;

                for (const auto& workItem : m_workItems)
                {
                    auto durationNs = workItem.stopTimeNs - workItem.startTimeNs;

                    startTimeNsPacker.Peek(workItem.startTimeNs);
                    durationNsPacker.Peek(durationNs);
                    categoryNameIdxPacker.Peek(workItem.categoryNameIdx);
                    workerNameIdxPacker.Peek(workItem.workerNameIdx);
                    routineNameIdxPacker.Peek(workItem.routineNameIdx);
                    commentNameIdxPacker.Peek(workItem.commentNameIdx);
                    taskIdPacker.Peek(workItem.taskId);
                }

                sectionHeader.startTimeNsSize       = startTimeNsPacker.DeterminePackingSize();
                sectionHeader.startTimeNsBase       = startTimeNsPacker.base();
                sectionHeader.durationTimeNsSize    = durationNsPacker.DeterminePackingSize();
                sectionHeader.durationTimeNsBase    = durationNsPacker.base();
                sectionHeader.categoryNameIdxSize   = categoryNameIdxPacker.DeterminePackingSize();
                sectionHeader.categoryNameIdxBase   = categoryNameIdxPacker.base();
                sectionHeader.workerNameIdxSize     = workerNameIdxPacker.DeterminePackingSize();
                sectionHeader.workerNameIdxBase     = workerNameIdxPacker.base();
                sectionHeader.routineNameIdxSize    = routineNameIdxPacker.DeterminePackingSize();
                sectionHeader.routineNameIdxBase    = routineNameIdxPacker.base();
                sectionHeader.commentNameIdxSize    = commentNameIdxPacker.DeterminePackingSize();
                sectionHeader.commentNameIdxBase    = commentNameIdxPacker.base();
                sectionHeader.taskIdSize            = taskIdPacker.DeterminePackingSize();
                sectionHeader.taskIdBase            = taskIdPacker.base();

                for (const auto& workItem : m_workItems)
                {
                    auto durationNs = workItem.stopTimeNs - workItem.startTimeNs;

                    startTimeNsPacker.Pack(m_out, workItem.startTimeNs);
                    durationNsPacker.Pack(m_out, durationNs);
                    categoryNameIdxPacker.Pack(m_out, workItem.categoryNameIdx);
                    workerNameIdxPacker.Pack(m_out, workItem.workerNameIdx);
                    routineNameIdxPacker.Pack(m_out, workItem.routineNameIdx);
                    commentNameIdxPacker.Pack(m_out, workItem.commentNameIdx);
                    taskIdPacker.Pack(m_out, workItem.taskId);
                }

                return sectionHeader;
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
            workItemProto.taskId = eventData.taskId;
        }

    private:
        // Splits an examplar string "Worker.Routine" into "Worker" and "Routine".
        //
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

    // Collects the event logs and generates the usable data upon finish.
    //
    template<typename Traits>
    class PerfLogger
    {
        #pragma pack(push)
        #pragma pack(1)
        struct Event
        {
            typename Traits::Clock::time_point startTime;
            typename Traits::Clock::time_point stopTime = {};
            typename Traits::EventData data = {};
        };
        #pragma pack(pop)

        std::ostream* m_out = nullptr;
        const char* m_outFileName = nullptr;
        typename Traits::Clock::time_point m_startTime;
        std::atomic<uint32_t> m_eventCount = {0};
        std::vector<Event> m_events;

    public:
        // The purpose of a Tracer object is put a timestamp on Event::stopTime of the specified event object upon its destruction.
        //
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

        std::string ProgramName;
        std::string Description;

        ~PerfLogger()
        {
            Finish();
        }

        void Enable(std::ostream& out, uint32_t eventCount)
        {
            m_startTime = Traits::Clock::now();
            m_events.resize(eventCount);
            m_out = &out;
            assert(m_outFileName == nullptr && "PerfLogger has been already enabled to write to a file.");
        }

        void Enable(const char* outFileName, uint32_t eventCount)
        {
            m_startTime = Traits::Clock::now();
            m_events.resize(eventCount);
            m_outFileName = outFileName;
            assert(m_out == nullptr && "PerfLogger has been already enabled to write to a stream.");
        }

        void Disable()
        {
            /*const auto eventCount =*/ StopNewEvents();

            /* It is better to leave m_events intact, so the following check is not necessary.

                // Check whether all the events are finished (have set stopTime).
                for (uint32_t eventIdx = 0; eventIdx < eventCount; ++eventIdx)
                {
                    Event& event = m_events[eventIdx];
                    if (event.stopTime == typename Traits::Clock::time_point{})
                        throw std::runtime_error("Unable to disable a PerfLogger while there are pending tracers");
                }
            */

            m_out = nullptr;
            m_outFileName = {};
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

            const auto eventCount = StopNewEvents();

            auto writer = bin::BinaryWriter{*m_out, ProgramName, Description};

            for (uint32_t eventIdx = 0; eventIdx < eventCount; ++eventIdx)
            {
                Event& event = m_events[eventIdx];

                if (event.stopTime.time_since_epoch().count() == 0)
                    event.stopTime = stopTime;

                WorkItemProto<typename Traits::Clock> workItemProto { event.startTime, event.stopTime };

                Traits::OnWorkItem(event.data, workItemProto);

                writer.WriteWorkItem(std::move(workItemProto));
            }

            writer.Finish();

            m_out = nullptr;
            m_outFileName = {};
        }

    private:
        // Timestamps the beginning of a new event.
        // Returns a Tracer, which will timestamp the end upon its destructor.
        //
        Tracer TraceEvent(typename Traits::EventData&& eventData)
        {
            const auto eventIdx = m_eventCount++;
            if (eventIdx >= m_events.size())
            {
                // Consider: --m_eventCount;
                return {};
            }

            Event& event = m_events[eventIdx];
            event.startTime = Traits::Clock::now();
            // event.stopTime is already set to 0
            event.data = std::move(eventData);
            return {&event};
        }

        // Prevents the logger from starting new events.
        // Returns the number of currently stored events.
        //
        uint32_t StopNewEvents()
        {
            const auto eventsSize = static_cast<uint32_t>(m_events.size());
            return std::min(m_eventCount.exchange(eventsSize), eventsSize);
        }
    };

} // namespace profane
