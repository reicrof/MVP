#if defined(WIN32) || defined(_WIN32) 
#include <Windows.h>
#else
#include <dlfcn.h> 
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "utils.h"
#include "vulkanGraphic.h"

#include <iostream>
#include <string>
#include <assert.h>

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

static void onWindowResized(GLFWwindow* window, int width, int height)
{
	if (width > 0 && height > 0)
	{
		VulkanGraphic* VK = reinterpret_cast<VulkanGraphic*>(glfwGetWindowUserPointer(window));
		VK->recreateSwapChain();
	}
}

void updateCoreDll()
{
	if (shouldReloadCoreLib())
	{
		loadCoreFunctions();
	}
}

static void initVulkan(VulkanGraphic& VK, GLFWwindow* window)
{
	VERIFY(VK.createSurface(window), "Cannot create vulkan surface.");
	VERIFY(VK.getPysicalDevices(), "Cannot get physical device.");
	VERIFY(VK.createLogicalDevice(), "Cannot create logical device.");
	VERIFY(VK.createSwapChain(), "Cannot create swap chain.");
	VERIFY(VK.createRenderPass(), "Cannot create a render pass.");
	VERIFY(VK.createPipeline(), "Cannot create the pipeline.");
	VERIFY(VK.createFrameBuffers(), "Cannot create frame buffer.");
	VERIFY(VK.createCommandPool(), "Cannot create frame buffer.");
	VERIFY(VK.createCommandBuffers(), "Cannot create frame buffer.");
	VERIFY(VK.createSemaphores(), "Cannot create semaphores");
}

int main()
{
	VERIFY(glfwInit(), "Cannot init glfw.");
	VERIFY(glfwVulkanSupported(), "Vulkan not supported.");

	loadCoreFunctions();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	GLFWwindow* window = glfwCreateWindow(800, 600, "Vulkan window", nullptr, nullptr);

	VERIFY(window, "Could not create GLFW window.");

	unsigned int glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector< const char* > extensions(glfwExtensionCount);
	std::copy(glfwExtensions, glfwExtensions + glfwExtensionCount, extensions.begin());

	VulkanGraphic VK(extensions);
	initVulkan(VK, window);

	// Setup callback function
	glfwSetKeyCallback(window, keyCB);
	glfwSetWindowUserPointer(window, &VK); // Set user data
	glfwSetWindowSizeCallback(window, onWindowResized);

	while (!glfwWindowShouldClose(window))
	{
		updateCoreDll();
		glfwPollEvents();
		//std::cout << ptr() << std::endl;
		VK.render();
	}

	glfwDestroyWindow(window);
	glfwTerminate();
}