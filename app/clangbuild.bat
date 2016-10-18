@echo off

del /Q *.pdb

set clangFlags=-std=c++1z -Werror

set srcFiles=main.cpp vulkanGraphic.cpp swapchain.cpp
set coreInclude=../core/

set glfwInclude=../thirdParties/glfw/include/
set glfwLibPath=../thirdParties/glfw/lib-vc2015/

set vulkanLibPath=../thirdParties/vulkan/

set libs= -lkernel32 -lglfw3dll -luser32 -lvulkan-1 

call clang++ %srcFiles% %clangFlags%  -I%glfwInclude% -I%coreInclude% -L%glfwLibPath% -L%vulkanLibPath% -L%windowsLibPath%  %libs% -o mvp.exe

pause