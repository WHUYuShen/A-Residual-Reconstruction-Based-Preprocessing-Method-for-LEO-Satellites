# A Residual Reconstruction Based Preprocessing Method for LEO Satellites

This repository contains the C++ implementation associated with the paper **基于观测残差重构的低轨卫星 GNSS 数据预处理方法**. The method, named **Recheck**, is designed for onboard GNSS data preprocessing in low-Earth-orbit (LEO) satellite precise orbit determination.

## Background

LEO satellites move quickly and often produce short, discontinuous onboard GNSS observation arcs. Conventional preprocessing methods based on Melbourne-Wubbena (MW) and Geometry-Free (GF) combinations may mistakenly mark valid observations as cycle slips or outliers, especially near the beginning and end of daily observation arcs. These false deletions reduce observation continuity and can affect precise orbit determination accuracy.

## Method Overview

The Recheck method introduces simplified dynamic orbit determination results into the conventional preprocessing workflow. It reconstructs observed-minus-calculated (OMC) residuals by combining:

- satellite orbit information,
- onboard GNSS receiver clock estimates,
- ambiguity estimates,
- original GNSS observations.

The reconstructed residual sequence is then used as a new detection quantity for cycle-slip and outlier rechecking. Previously deleted observations are inspected again, and valid data are restored where the residuals and ambiguity consistency satisfy the configured thresholds.

## Implementation

The Visual Studio C++ program in this repository:

- reads OMC records and groups them by satellite PRN and station,
- parses LOG ambiguity/deletion intervals while preserving the original header,
- detects missing epochs in observation intervals,
- detects residual jumps using a configurable threshold,
- reconstructs AMB/DEL records and updates ambiguity statistics in the output LOG file,
- prints diagnostic statistics for the maximum ambiguity count in a single epoch.

## Experimental Context

The paper evaluates the method using GRACE-C and GRACE-D onboard GNSS observations from 2022 DOY 189 to DOY 196 with a 10-second sampling interval. The results show that Recheck can restore valid observations without introducing obvious additional noise.

Key reported results include:

- average full-day visible-satellite increase: **2.81%**,
- average increase within the first and last 60 epochs of each day: **1.6 satellites**, with an increase rate of **9.65%**,
- improved radial, along-track, and cross-track orbit residuals,
- accuracy improvements exceeding **20%** in some weak-observation segments.

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

## Usage

After building the project, run the executable with three command-line arguments:

```powershell
Task2.exe <input_log_file> <input_omc_file> <output_log_file>
```

Example:

```powershell
Task2.exe grac1970.19o.log_orig omc_2019197 grac1970.19o_output
```

The program no longer depends on fixed local file paths. Input and output paths are supplied at runtime.

The repository intentionally excludes local Visual Studio files, build outputs, generated output logs, and large local OMC data files.
## Keywords

GNSS, LEO satellites, observation residuals, OMC reconstruction, cycle-slip detection, precise orbit determination, GRACE-C, GRACE-D.

## License

No license has been specified yet.
