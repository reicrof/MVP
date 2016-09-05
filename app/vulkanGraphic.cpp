#include "vulkanGraphic.h"
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <assert.h>
#define VK_CALL(func)									 \
if( func != VK_SUCCESS )								 \
{														 \
	std::cerr << "Error when calling " << #func << " at "\
	<< __FILE__ << ":" << __LINE__ << std::endl;		 \
	assert( false );									 \
}

const std::vector<const char*> VALIDATION_LAYERS = {
	"VK_LAYER_LUNARG_standard_validation"
};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback) {
	auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pCallback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (func != nullptr) {
		func(instance, callback, pAllocator);
	}
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData) {
	std::cerr << "validation layer: " << msg << std::endl;

	return VK_FALSE;
}

namespace
{
	std::vector<const char*> getRequiredExtensions()
	{
		std::vector<const char*> extensions;

		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		for (unsigned int i = 0; i < glfwExtensionCount; ++i)
		{
			extensions.push_back(glfwExtensions[i]);
		}

		if (enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		return extensions;
	}

	void setupDebugCallback( const VDeleter<VkInstance>& instance, VDeleter<VkDebugReportCallbackEXT>& callback)
	{
		if (!enableValidationLayers) return;

		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = debugCallback;

		if (CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS) {
			throw std::runtime_error("failed to set up debug callback!");
		}
	}

	bool checkValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : VALIDATION_LAYERS)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound) { return false; }
		}

		return true;
	}
}

VulkanGraphic::VulkanGraphic()
{
	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "MVP";
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = "No Engine";
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	if (enableValidationLayers && !checkValidationLayerSupport())
	{
		std::cerr << "Validation layer not supported and will not be enabled." << std::endl;
	}

	auto extensions = getRequiredExtensions();
	createInfo.enabledExtensionCount = static_cast< uint32_t >( extensions.size() );
	createInfo.ppEnabledExtensionNames = extensions.data();

	if (enableValidationLayers)
	{
		createInfo.enabledLayerCount = static_cast< uint32_t >( VALIDATION_LAYERS.size() );
		createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
	}
	else
	{
		createInfo.enabledLayerCount = 0;
	}

	VK_CALL( vkCreateInstance(&createInfo, nullptr, &_instance) );
	setupDebugCallback(_instance, _validationCallback);
}