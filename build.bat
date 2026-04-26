@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat"

set IMGUI_DIR=imgui
set SOURCES=main.cpp imgui_custom_widgets.cpp vm_wrapper.cpp %IMGUI_DIR%\imgui.cpp %IMGUI_DIR%\imgui_draw.cpp %IMGUI_DIR%\imgui_tables.cpp %IMGUI_DIR%\imgui_widgets.cpp %IMGUI_DIR%\backends\imgui_impl_dx11.cpp %IMGUI_DIR%\backends\imgui_impl_win32.cpp
set INCLUDES=/I %IMGUI_DIR% /I %IMGUI_DIR%\backends
set LIBS=user32.lib d3d11.lib d3dcompiler.lib dxgi.lib advapi32.lib shell32.lib ole32.lib

rc resource.rc
cl /nologo /EHsc /MD /O2 /std:c++20 %INCLUDES% %SOURCES% resource.res /Fe:WhoTheFuckIsMike.exe /link %LIBS%
