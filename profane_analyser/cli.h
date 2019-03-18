
#pragma once

#include "pch.h"

struct ParsedCommandLine
{
    const char* programFilePath = nullptr;
    bool printHelp = false;
    const char* perfLogOutputFilePath = nullptr;
    uint32_t perfLogMaxSamples = 0;
    const char* inputFilePath = nullptr;
};

ParsedCommandLine ParseCommandLine(int argc, char* args[]);
void PrintHelp();
