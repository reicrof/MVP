import subprocess
import sys
import platform
import getopt
import datetime

outName = "mvp"
srcFiles = [ "main.cpp", "vulkanGraphic.cpp", "swapChain.cpp", "MemoryPool.cpp", "vMemoryPool.cpp", "VImage.cpp" ]
coreInclude = ["../core/"]

# Third parties includes
#clang = "-stdlib=libc++"
glfwInclude = [ "../thirdParties/glfw/include/" ]
glmInclude = [ "../thirdParties/" ]
vulkanLibPath = ["../thirdParties/vulkan/"]
vulkanIncludePath = ["../thirdParties/vulkan/include"]
stbIncludePath = ["../thirdParties/stb"]
commonCompilerFlags = [ "-Wall", "-Werror", ]
cppVersion = ["-std=c++1z"]

if platform.system() == "Linux":
   compiler = "/DLlocal/landrych/cfe-3.9.0.src/build/bin/clang++"
   compilerFlags = [ "-g"]
   glfwLibPath = ["../thirdParties/glfw/"]
   libs = ["-lglfw3", "-lvulkan-1"]

elif platform.system() == "Windows":
   compiler = "clang++"
   compilerFlags = [ "-std=c++1z" ]
   glfwLibPath = ["../thirdParties/glfw/lib-vc2015/"]
   libs = [ "-lkernel32", "-lglfw3dll", "-luser32", "-lvulkan-1" ]
   outName += ".exe"

# Parse arguments to replace default values
opts, args = getopt.getopt(sys.argv[1:],"c:s:l:")
for opt, arg in opts:
   if opt == '-c':
      compiler = arg
   elif opt == '-s':
      cppVersion = ["-std="+str(arg)]
   elif opt == '-l':
      stdlib = ["-stdlib"+str(arg)]

buildStartTime = datetime.datetime.now()
print "Building on " + platform.system() + " with " + compiler + " at " + str(buildStartTime)
result = subprocess.call( [ compiler ] + cppVersion + commonCompilerFlags + compilerFlags + srcFiles + ["-I"] + glfwInclude + ["-I"] + vulkanIncludePath + \
                  ["-I"] + glmInclude + ["-I"] + stbIncludePath + ["-I"] + coreInclude + ["-L"] + glfwLibPath + ["-L"] + vulkanLibPath + libs + ["-o"] + [ outName ] )

buildEndTime = datetime.datetime.now()
if result != 0:
   print "Build failed!"
else:
   print "Build succeed at " + str( buildEndTime )