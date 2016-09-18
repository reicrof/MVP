#include "vulkanGraphic.h"
#include "utils.h"
#include <vector>
#include <set>
#include <algorithm>
#include <iostream>
#include <assert.h>
#include <fstream>
#include <string>

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
} // End of anonymous namespace

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

bool VulkanGraphic::createRenderPass()
{
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = _swapChain->getCurrentFormat();
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subPass = {};
	subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subPass.colorAttachmentCount = 1;
	subPass.pColorAttachments = &colorAttachmentRef;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colorAttachment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subPass;

	VK_CALL(vkCreateRenderPass(_device, &renderPassInfo, nullptr, &_renderPass));

	return true;
}

bool VulkanGraphic::createPipeline()
{
	VDeleter<VkShaderModule> vertShaderModule{ _device, vkDestroyShaderModule };
	VDeleter<VkShaderModule> fragShaderModule{ _device, vkDestroyShaderModule };
	createShaderModule("../shaders/vert.spv", vertShaderModule);
	createShaderModule("../shaders/frag.spv", fragShaderModule);

	// Vertex stage
	VkPipelineShaderStageCreateInfo vShaderStageCreateInfo = {};
	vShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vShaderStageCreateInfo.module = vertShaderModule;
	vShaderStageCreateInfo.pName = "main";

	// Fragment stage
	VkPipelineShaderStageCreateInfo fShaderStageInfo = {};
	fShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fShaderStageInfo.module = fragShaderModule;
	fShaderStageInfo.pName = "main";

	VkPipelineShaderStageCreateInfo shaderStages[] = { vShaderStageCreateInfo, fShaderStageInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.vertexAttributeDescriptionCount = 0;

	VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	VkViewport viewport = {};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = static_cast< float >( _swapChain->_curExtent.width );
	viewport.height = static_cast< float >( _swapChain->_curExtent.height );
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	VkRect2D scissor = {};
	scissor.offset = { 0, 0 };
	scissor.extent = _swapChain->_curExtent;

	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = VK_FALSE;
	rasterizer.rasterizerDiscardEnable = VK_FALSE;
	rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer.lineWidth = 1.0f;
	rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer.depthBiasEnable = VK_FALSE;

	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = VK_FALSE;
	multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
	colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	colorBlendAttachment.blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlending = {};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &colorBlendAttachment;
	colorBlending.blendConstants[0] = 0.0f;
	colorBlending.blendConstants[1] = 0.0f;
	colorBlending.blendConstants[2] = 0.0f;
	colorBlending.blendConstants[3] = 0.0f;

	VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutInfo.setLayoutCount = 0;
	pipelineLayoutInfo.pushConstantRangeCount = 0;

	VK_CALL(vkCreatePipelineLayout(_device, &pipelineLayoutInfo, nullptr, &_pipelineLayout));



	VkGraphicsPipelineCreateInfo pipelineInfo = {};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineInfo.stageCount = 2;
	pipelineInfo.pStages = shaderStages;

	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.renderPass = _renderPass;
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
	pipelineInfo.basePipelineIndex = -1; // Optional

	VK_CALL(vkCreateGraphicsPipelines(_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &_graphicsPipeline));

	return true;
}

bool VulkanGraphic::createShaderModule(const std::string& shaderPath, VDeleter<VkShaderModule>& shaderModule)
{
	std::vector<char> shaderSource(std::istreambuf_iterator< char >{ std::ifstream(shaderPath, std::ios::binary) }, std::istreambuf_iterator< char >{});
	if (shaderSource.empty())
	{
		std::cerr << "Cannot find shader : " << shaderPath << std::endl;
		return false;
	}

	VkShaderModuleCreateInfo shaderModuleCreateInfo =
	{
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr, 0,
		shaderSource.size(),
		reinterpret_cast<const uint32_t*>(shaderSource.data())
	};

	if (vkCreateShaderModule(_device, &shaderModuleCreateInfo, nullptr, &shaderModule) != VK_SUCCESS)
	{
		std::cerr << "Error while creating shader module for shader : " << shaderPath << std::endl;
		return false;
	}

	return true;
}