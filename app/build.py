import subprocess
import sys
import platform

if platform.system() == "Linux":
   compiler = "/DLlocal/landrych/cfe-3.9.0.src/build/bin/clang++"
   compilerFlags = ["-std=c++1z", "-stdlib=libc++", "-Werror"]
   srcFiles = [ "main.cpp", "vulkanGraphic.cpp", "swapChain.cpp" ]
   coreInclude = ["../core/"]
   glfwInclude = [ "../thirdParties/glfw/include/" ]
   vulkanLibPath = ["../thirdParties/vulkan/"]
   vulkanIncludePath = ["../thirdParties/vulkan/include"]
   libs = "-lglfw3 -lvulkan-1"
elif platform.system() == "Windows":
   compiler = "clang++"
   compilerFlags = "-std=c++1z -Werror"
   srcFiles = [ "main.cpp", "vulkanGraphic.cpp", "swapChain.cpp" ]
   coreInclude = "../core/"
   glfwInclude = "../thirdParties/glfw/include/"
   glfwLibPath = "../thirdParties/glfw/lib-vc2015/"
   vulkanLibPath = "../thirdParties/vulkan/"
   libs = "-lkernel32 -lglfw3dll -luser32 -lvulkan-1"


subprocess.call( [ compiler ] + compilerFlags + srcFiles + ["-I"] + glfwInclude + ["-I"] + vulkanIncludePath )
#call clang++ %srcFiles% %clangFlags%  -I%glfwInclude% -I%coreInclude% -L%glfwLibPath% -L%vulkanLibPath% -L%windowsLibPath%  %libs% -o mvp.exe