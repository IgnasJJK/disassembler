@echo off

IF NOT EXIST build MKDIR build

pushd build

rem C4201: Using nameless struct 

cl -Zi -W4 -wd4201 ..\src\main.cpp

popd