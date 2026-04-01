# Win32HashCalc

Native Windows desktop applications demonstrating high-performance file hashing using only Win32, C++ standard libraries and Microsoft Cryptography API: Next Generation (CNG).
- Reference: https://docs.microsoft.com/en-us/windows/win32/seccng/cng-portal

<p align="center">
	<img src="Screenshot.png" width="75%">
	<br>
	<b><i>Win32 Hash Calc GUI</i></b>
</p>

## Project folder structure
- [HashCalcGUI](HashCalcGUI/) hash calculator with native graphical user interface
- [HashCalcCLI](HashCalcCLI/) hash calculator for command-line use 
- [HashCalcLib](HashCalcLib/) shared library with core hashing logic used by both GUI and CLI
- [HashCalcTests](HashCalcTests/) unit tests for hashing functions and utilities

## Build prerequisites

- Visual Studio 2022
  - https://www.visualstudio.com/
  - C++ Desktop Development Workload selected

## Build instructions

1. Open `Win32HashCalc.sln` in Visual Studio 2022.
2. Select target architecture (x64 recommended) and configuration (Release/Debug).
3. Build Solution (`Ctrl+Shift+B`).

*Note: When building locally, the Git commit hash in the About dialog defaults to a local fallback. Official release binaries built via GitHub Actions automatically receive the exact commit SHA.*