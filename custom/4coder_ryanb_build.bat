@echo off
call bin\buildsuper_x64-win.bat 4coder_ryanb.cpp
copy custom_4coder.dll ..\custom_4coder.dll
