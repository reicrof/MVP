#include "vulkanGraphic.h"
#include <vector>
#include <algorithm>

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

VulkanGraphic::VulkanGraphic(std::vector<const char*> instanceExtensions)
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

	if (enableValidationLayers) { instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME); }

	createInfo.enabledExtensionCount = static_cast< uint32_t >(instanceExtensions.size() );
	createInfo.ppEnabledExtensionNames = instanceExtensions.data();

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

bool VulkanGraphic::getPysicalDevices()
{
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);

	std::vector< VkPhysicalDevice > devices(deviceCount);
	vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());

	int graphicFamily = -1;
	int presentFamily = -1;
	VkPhysicalDeviceFeatures features;
	VkPhysicalDeviceProperties properties;
	for (size_t i = 0; i < deviceCount; ++i)
	{
		vkGetPhysicalDeviceProperties(devices[i], &properties);
		vkGetPhysicalDeviceFeatures(devices[i], &features);

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queueFamilyCount, queueFamilies.data());

		std::vector<VkBool32> queuePresentSupport(queueFamilyCount);
		for( uint32_t j = 0; j < queueFamilyCount; ++j )
		{
			VkBool32 presentationSupported = VK_FALSE;
			vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], j, _surface, &presentationSupported);
			if (queueFamilies[j].queueCount > 0 && queueFamilies[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				graphicFamily = j;

				// If the queue support both presentation and graphic, stop here.
				if (presentationSupported) { presentFamily = j; break; }
			}

			if (presentationSupported) { presentFamily = j; }
		}

		// We have found the one.
		if (graphicFamily >= 0 && presentFamily >= 0)
		{
			_physDevice = devices[i];

			_graphicQueue.familyIndex = graphicFamily;
			_presentationQueue.familyIndex = presentFamily;
			return true;
		}
	}

	return false;
}

bool VulkanGraphic::createLogicalDevice()
{
	std::vector< int > uniqueFamilliesIndex = { _graphicQueue.familyIndex, _presentationQueue.familyIndex };
	auto last = std::unique(uniqueFamilliesIndex.begin(), uniqueFamilliesIndex.end());
	uniqueFamilliesIndex.erase(last, uniqueFamilliesIndex.end());
	std::vector< VkDeviceQueueCreateInfo > queueCreateInfos(uniqueFamilliesIndex.size());
	const float queuePriority = 1.0f;
	for (size_t i = 0; i < uniqueFamilliesIndex.size(); ++i)
	{
		queueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[i].queueFamilyIndex = uniqueFamilliesIndex[i];
		queueCreateInfos[i].queueCount = 1;
		queueCreateInfos[i].pQueuePriorities = &queuePriority;
	}

	// No special feature for now.
	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast< uint32_t >( queueCreateInfos.size() );
	createInfo.pEnabledFeatures = &deviceFeatures;

	createInfo.enabledExtensionCount = 0;

	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast< uint32_t >( VALIDATION_LAYERS.size() );
		createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
	}
	else {
		createInfo.enabledLayerCount = 0;
	}

	VK_CALL(vkCreateDevice(_physDevice, &createInfo, nullptr, &_device));

	// Get the handle of the queue.
	vkGetDeviceQueue(_device, _graphicQueue.familyIndex, 0, &_graphicQueue.handle);

	return true;
}

bool VulkanGraphic::createSurface(GLFWwindow* window)
{
	VK_CALL(glfwCreateWindowSurface(_instance, window, nullptr, &_surface));
	return true;
}
