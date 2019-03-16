# profane
:construction: Performance tracer for C++11 programs and visualization tool

Currently just a Proof of Concept.

The first run of Profane Analyser dumps the performance log to `perflog.bin`.
The second run reads the file and presents it in a form of a flame graph.

Supported platforms: Linux (CMake/gcc) and Windows (Visual C++)

The project contains Windows x64 runtime binaries of SDL2 library (with Image and TTF extensions).
