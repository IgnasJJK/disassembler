@echo off

IF NOT EXIST temp MKDIR temp
IF NOT EXIST build call build.bat
IF NOT EXIST build\main.exe call build.bat

IF NOT EXIST tests\%1.asm EXIT /b

nasm tests\%1.asm -o temp\assembly
build\main.exe temp\assembly > temp\disassembly.asm
nasm temp\disassembly.asm -o temp\disassembly

IF EXIST temp\disassembly fc temp\disassembly temp\assembly
