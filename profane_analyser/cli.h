
#pragma once

#include "pch.h"

struct ParsedCommandLine
{
    std::string programFilePath;
    std::string programDirPath;
    bool printHelp = false;
    const char* perfLogOutputFilePath = nullptr;
    uint32_t perfLogMaxSamples = 0;
    const char* inputFilePath = nullptr;
};

ParsedCommandLine ParseCommandLine(int argc, char* args[]);
void PrintHelp();
