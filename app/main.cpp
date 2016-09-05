#if defined(WIN32) || defined(_WIN32) 
#include <Windows.h>
#else
#include <dlfcn.h> 
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkanGraphic.h"

#include <iostream>
#include <string>
#include <assert.h>

template< typename T >
void VERIFY(T x, const char* msg)
{
	if (!x) {
		std::cerr << msg << std::endl;
		exit(EXIT_FAILURE);
	}
}

#define RTLD_LAZY   1
#define RTLD_NOW    2
#define RTLD_GLOBAL 4

void* loadSharedLibrary(const char* libNameNoExt, int iMode = 2)
{
	std::string sDllName = libNameNoExt;
#if defined(WIN32) || defined(_WIN32) 
	sDllName += ".dll";
	return (void*)LoadLibrary(sDllName.c_str());
#else
	sDllName += ".so";
	return dlopen(sDllName.c_str(), iMode);
#endif
}
void* getFunction(void* lib, char* funcName)
{
#if defined(WIN32) || defined(_WIN32) 
	return (void*)GetProcAddress((HINSTANCE)lib, funcName);
#else
	return dlsym(Lib, Fnname);
#endif
}

bool freeSharedLibrary(void *handle)
{
#if defined(WIN32) || defined(_WIN32) 
	return FreeLibrary((HINSTANCE)handle) == TRUE;
#else
	return dlclose(hDLL);
#endif
}

#include "../core/core.h"

const char* coreDllName = "../core/core.dll";
const char* tempCoreDllName = "../core/coreRunning.dll";
#if defined(_WIN32)
FILETIME lastLoadedCoreDllWriteTime;
#endif
void* coreDll = nullptr;
typedef int(*getOneFnPtr)();
getOneFnPtr ptr = nullptr;

#if defined(WIN32) || defined(_WIN32)
FILETIME getFileTimeFromFile(const char* fileName)
{
	WIN32_FIND_DATA findFileData;
	FindFirstFile(fileName, &findFileData);
	return findFileData.ftLastWriteTime;
}
#else
#endif

static void loadCoreFunctions()
{
	if (coreDll) { freeSharedLibrary(coreDll); }

	int res = CopyFile(coreDllName, tempCoreDllName, FALSE);
	coreDll = loadSharedLibrary("../core/coreRunning");
	lastLoadedCoreDllWriteTime = getFileTimeFromFile(coreDllName);
	//coreDll = loadSharedLibrary("../../core/core");
	VERIFY(coreDll != nullptr, "Cannot load core lib");

	ptr = (getOneFnPtr)getFunction(coreDll, "getOne");
	VERIFY(ptr, "cannot get function");
}

bool shouldReloadCoreLib()
{
#if defined(WIN32) || defined(_WIN32)
	return CompareFileTime(&getFileTimeFromFile(coreDllName), &lastLoadedCoreDllWriteTime) == 1;
#else
#endif
}

static void keyCB(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	}
}

void updateCoreDll()
{
	if (shouldReloadCoreLib())
	{
		loadCoreFunctions();
	}
}

int main()
{	
	VERIFY(glfwInit(), "Cannot init glfw.");
	VERIFY(glfwVulkanSupported(), "Vulkan not supported.");

	loadCoreFunctions();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

	VERIFY(window, "Could not create GLFW window.");

	// Setup callback function
	glfwSetKeyCallback(window, keyCB);

	VulkanGraphic VK;

	while (!glfwWindowShouldClose(window))
	{
		updateCoreDll();
		glfwPollEvents();
		std::cout << ptr() << std::endl;
	}

	glfwDestroyWindow(window);

	glfwTerminate();
}