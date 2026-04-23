# A Residual Reconstruction Based Preprocessing Method for LEO Satellites

This repository contains a Visual Studio C++ implementation for preprocessing LEO satellite observation log data using OMC residual information.

The program reads ambiguity/deletion records from a LOG file, matches them with OMC data by PRN and station, detects missing epochs and residual jumps, then reconstructs updated ambiguity records for downstream processing.

## Main Features

- Reads OMC records and groups them by satellite PRN and station.
- Parses LOG ambiguity/deletion intervals while preserving the original header.
- Detects missing epochs in observation intervals.
- Detects residual jumps using a configurable threshold.
- Reconstructs AMB/DEL records and updates ambiguity statistics in the output LOG file.
- Provides console statistics for maximum ambiguity count in a single epoch.

## Project Structure

```text
Task2.sln
Task2/
  mission1.cpp
  Task2.vcxproj
  Task2.vcxproj.filters
```

## Build Environment

- Microsoft Visual Studio 2022
- MSVC toolset v143
- C++20 for the x64 Debug configuration

Open `Task2.sln` in Visual Studio and build the `Task2` project.

## Notes

The current source code uses fixed local file paths for input and output files. Before running the program on another machine, update the following variables in `Task2/mission1.cpp`:

```cpp
LOG_FILE
OMC_FILE
OUTPUT_FILE
```

## License

No license has been specified yet.
