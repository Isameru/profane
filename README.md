# profane
:construction: Performance tracer for C++11 programs and visualization tool

Build and run Profane Analyser with option `-o perflog.bin` to gather the performance log data.
Then run with parameter `perflog.bin` to open it for introspection.

Supported platforms: Linux (CMake/gcc) and Windows (Visual C++)

Directory `assets` has to be copied to the directory of the built executable.
The project contains Windows x64 runtime binaries of SDL2 library (with Image and TTF extensions). Those have to be copied to there also.
If you are running on Linux, you have to install these packages first: `sudo apt install g++ cmake libsdl2-dev libsdl2-image-dev libsdl2-ttf-dev`
