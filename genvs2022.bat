@echo off
setlocal

if not exist "Build" md "Build"
cmake . -G "Visual Studio 17 2022" -A x64 -B./Build

