#!/bin/sh
# 用便携 MSYS2 的 mingw64 g++ 编译（照 build-mingw.sh 的命令，去掉交叉前缀）
set -e
cd /e/Temp/AI_Study/TaskbarPopup_With_GroupIcon
export PATH=/e/Temp/AI_Study/mingw-w64-toolchain/msys64/mingw64/bin:$PATH
g++ --version | head -1
windres src/app.rc -O coff -o build-app.res
g++ -std=c++17 -O2 -s -Wall -Wextra -Wno-missing-field-initializers \
    -DUNICODE -D_UNICODE -DNOMINMAX -D_WIN32_WINNT=0x0A00 -DWINVER=0x0A00 \
    src/*.cpp build-app.res -o TaskbarPopup.exe \
    -mwindows -municode -static -static-libgcc -static-libstdc++ \
    -luser32 -lgdi32 -lgdiplus -lshell32 -lcomctl32 -ldwmapi -lshcore -ladvapi32 -lole32 -luuid -loleaut32 -ldxva2 -lwinhttp -liphlpapi -limm32 -lcomdlg32
rm -f build-app.res
echo BUILD_DONE
ls -la TaskbarPopup.exe
