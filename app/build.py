#!/usr/bin/env python
import subprocess
import sys
import platform
import getopt
import datetime

outName = "mvp"
srcFiles = [ "main.cpp", "vulkanGraphic.cpp", "swapChain.cpp", "MemoryPool.cpp", "vMemoryPool.cpp", "vImage.cpp", "vCommandPool.cpp", "Camera.cpp", "vThread.cpp"]
coreInclude = ["../core/"]

# Third parties includes
#clang = "-stdlib=libc++"
stdlib = [""]
analyzer = [""]
glfwInclude = [ "../thirdParties/glfw/include/" ]
glmInclude = [ "../thirdParties/" ]
vulkanLibPath = ["../thirdParties/vulkan/"]
vulkanIncludePath = ["../thirdParties/vulkan/include"]
stbIncludePath = ["../thirdParties/stb"]
tinyobjIncludePath = ["../thirdParties/tiny_obj_loader"]
commonCompilerFlags = [ "-Wall", "-Werror", ]
cppVersion = ["-std=c++1z"]

if platform.system() == "Linux":
   compiler = "/DLlocal/landrych/llvm-build/bin/clang++"
   compilerFlags = [ "-g", "-D_DEBUG"]
   glfwLibPath = ["../thirdParties/glfw/"]
   libs = ["-lglfw3", "-lvulkan", "-lrt", "-lm", "-ldl", "-lXrandr", "-lXinerama", "-lXxf86vm", "-lXcursor", "-lXext", "-lXrender", "-lXfixes", "-lX11", "-lpthread", "-lxcb", "-lXau" ]

elif platform.system() == "Windows":
   compiler = "clang++"
   compilerFlags = [ "-std=c++1z" ]
   glfwLibPath = ["../thirdParties/glfw/lib-vc2015/"]
   libs = [ "-lkernel32", "-lglfw3dll", "-luser32", "-lvulkan-1" ]
   outName += ".exe"

shouldLink = True;

# Parse arguments to replace default values
opts, args = getopt.getopt(sys.argv[1:],"ac:s:l:")
for opt, arg in opts:
   if opt == '-c':
      compiler = arg
   elif opt == '-s':
      cppVersion = ["-std="+str(arg)]
   elif opt == '-l':
      stdlib = ["-stdlib="+str(arg)]
   elif opt == '-a':
      shouldLink = False
      analyzer = ["--analyze", "-Xanalyzer", "-analyzer-output=html"]

buildCommand = [ compiler ] + cppVersion + stdlib + analyzer + commonCompilerFlags + compilerFlags + srcFiles + ["-I"] + glfwInclude + ["-I"] + vulkanIncludePath + \
               ["-I"] + glmInclude + ["-I"] + stbIncludePath + ["-I"] + tinyobjIncludePath +  ["-I"] + coreInclude

linkCommand =  ["-L"] + glfwLibPath + ["-L"] + vulkanLibPath + libs + ["-o"] + [ outName ]

if shouldLink:
    buildCommand += linkCommand

buildStartTime = datetime.datetime.now()
print "Building on " + platform.system() + " with " + compiler + " at " + str(buildStartTime)
result = subprocess.call( buildCommand )

buildEndTime = datetime.datetime.now()
if result != 0:
   print "Build failed!"
else:
   print "Build succeed at " + str( buildEndTime )
