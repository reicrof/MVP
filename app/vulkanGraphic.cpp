#include "vulkanGraphic.h"
#include "utils.h"
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>
#include <assert.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

const std::vector<const char*> VALIDATION_LAYERS = {
	"VK_LAYER_LUNARG_standard_validation"
};

const std::vector<const char*> DEVICE_EXTENSIONS = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
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

	bool isExtensionAvailable(const char* ext, const std::vector<VkExtensionProperties>& extList)
	{
		return std::find_if(extList.begin(), extList.end(), 
							[&ext](const VkExtensionProperties& extension)
								  { return strcmp(ext, extension.extensionName); }) != extList.end();
	}

	bool areDeviceExtensionsSupported(const VkPhysicalDevice& device, const std::vector<const char*> extensions)
	{
		bool success = true;
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
		std::vector< VkExtensionProperties > extensionPropertiesAvailable(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensionPropertiesAvailable.data());

		for (const auto& ext : extensions)
		{
			success &= isExtensionAvailable(ext, extensionPropertiesAvailable);
		}

		return success;
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
	std::set< int > uniqueFamilliesIndex = { _graphicQueue.familyIndex, _presentationQueue.familyIndex };
	std::vector< VkDeviceQueueCreateInfo > queueCreateInfos( uniqueFamilliesIndex.size() );
	const float queuePriority = 1.0f;
	size_t count = 0;
	for (auto i = uniqueFamilliesIndex.begin(); i != uniqueFamilliesIndex.end(); ++i )
	{
		queueCreateInfos[count].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfos[count].queueFamilyIndex = *i;
		queueCreateInfos[count].queueCount = 1;
		queueCreateInfos[count].pQueuePriorities = &queuePriority;
		++count;
	}

	// No special feature for now.
	VkPhysicalDeviceFeatures deviceFeatures = {};

	VkDeviceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	createInfo.pQueueCreateInfos = queueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast< uint32_t >( queueCreateInfos.size() );
	createInfo.pEnabledFeatures = &deviceFeatures;

	VERIFY(areDeviceExtensionsSupported(_physDevice, DEVICE_EXTENSIONS), "Not all extensions are supported.");
	
	createInfo.enabledExtensionCount = static_cast< uint32_t >(DEVICE_EXTENSIONS.size() );
	createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

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

bool VulkanGraphic::createSwapChain()
{
	_swapChain = std::make_unique< SwapChain >(_physDevice, _device, _surface);
	return true;
}

