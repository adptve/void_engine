@echo off
cd /d C:\code\github\time3cap\void_engine
C:\msys64\mingw64\bin\g++.exe -c -std=c++20 -I./include -I./build/_deps/glm-src -I./build/_deps/spdlog-src/include src/ecs/stub.cpp -o build/test_manual.obj 2>&1
echo Exit code: %ERRORLEVEL%
