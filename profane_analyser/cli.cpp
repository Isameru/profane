
#include "pch.h"
#include "cli.h"

ParsedCommandLine ParseCommandLine(int argc, char* args[])
{
    ParsedCommandLine cl;
    cl.programFilePath = args[0];

    if (argc == 1)
        cl.printHelp = true;

    for (int idx = 1; idx < argc; ++idx)
    {
        if (std::strcmp("-h", args[idx]) == 0)
        {
            cl.printHelp = true;
        }
        else if (std::strcmp("-o", args[idx]) == 0)
        {
            ++idx;
            if (idx >= argc)
                throw std::runtime_error("Performance log output file path expected after '-o'");
            cl.perfLogOutputFilePath = args[idx];
        }
        else if (std::strcmp("-s", args[idx]) == 0)
        {
            ++idx;
            if (idx >= argc)
                throw std::runtime_error("Maximal number of collected performance samples expected after '-s'");
            cl.perfLogMaxSamples = std::stoi(args[idx]);
        }
        else
        {
            cl.inputFilePath = args[idx];
        }
    }

    return cl;
}

void PrintHelp()
{
    std::cout <<
        "Profane Analyzer\n"
        "   <file>      Input performance log file\n"
        "   -o <file>   Dump performance log to file\n"
        "   -s <int>    Max number of collected performance samples\n"
        "   -h          Help\n"
        << std::endl;
}
