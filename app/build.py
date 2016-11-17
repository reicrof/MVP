import subprocess
import sys
import platform

outName = "mvp"
srcFiles = [ "main.cpp", "vulkanGraphic.cpp", "swapChain.cpp" ]
coreInclude = ["../core/"]

# Third parties includes
glfwInclude = [ "../thirdParties/glfw/include/" ]
glmInclude = [ "../thirdParties/" ]
vulkanLibPath = ["../thirdParties/vulkan/"]
vulkanIncludePath = ["../thirdParties/vulkan/include"]
stbIncludePath = ["../thirdParties/stb"]

if platform.system() == "Linux":
   compiler = "/DLlocal/landrych/cfe-3.9.0.src/build/bin/clang++"
   compilerFlags = ["-std=c++1z", "-stdlib=libc++", "-Werror", "-g"]
   libs = ["-lglfw3", "-lvulkan-1"]

elif platform.system() == "Windows":
   compiler = "clang++"
   compilerFlags = [ "-std=c++1z", "-Werror" ]
   glfwLibPath = ["../thirdParties/glfw/lib-vc2015/"]
   libs = [ "-lkernel32", "-lglfw3dll", "-luser32", "-lvulkan-1" ]
   outName += ".exe"

subprocess.call( [ compiler ] + compilerFlags + srcFiles + ["-I"] + glfwInclude + ["-I"] + vulkanIncludePath + \
                  ["-I"] + glmInclude + ["-I"] + stbIncludePath + ["-I"] + coreInclude + ["-L"] + glfwLibPath + ["-L"] + vulkanLibPath + libs + ["-o"] + [ outName ] )