@echo off

IF NOT EXIST build MKDIR build

pushd build

cl -Zi ..\src\main.cpp

popd