@echo off
setlocal

if not exist "Build" md "Build"
cmake . -G "Visual Studio 18 2026" -A x64 -B./Build

