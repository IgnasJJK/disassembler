@echo off

IF NOT EXIST build MKDIR build

pushd build

cl -Zi -W4 ..\src\main.cpp

popd