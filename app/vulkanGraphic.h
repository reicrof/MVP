#ifndef VULKAN_GRAPHIC_H_
#define VULKAN_GRAPHIC_H_

#include <vulkan\vulkan.h>
#include "vkUtils.h"
#include "swapChain.h"
#include <memory>
#include <vector>

struct GLFWwindow;
class SwapChain;

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);

class VulkanGraphic
{
public:
	VulkanGraphic(std::vector<const char*> instanceExtensions);
	~VulkanGraphic() = default;

	bool getPysicalDevices();
	bool createLogicalDevice();
	bool createSurface(GLFWwindow* window);
	bool createSwapChain();
	bool createRenderPass();
	bool createPipeline();

private:
	struct Queue {
		int familyIndex;
		VkQueue handle;
	};

	VDeleter<VkInstance> _instance{ vkDestroyInstance };
	VDeleter<VkDevice> _device{ vkDestroyDevice };
	VkPhysicalDevice _physDevice;
	Queue _graphicQueue;
	Queue _presentationQueue;
	VDeleter<VkSurfaceKHR> _surface{ _instance, vkDestroySurfaceKHR };
	std::unique_ptr< SwapChain > _swapChain;
	VDeleter<VkRenderPass> _renderPass{ _device, vkDestroyRenderPass };
	VDeleter<VkPipelineLayout> _pipelineLayout{ _device, vkDestroyPipelineLayout };
	VDeleter<VkPipeline> _graphicsPipeline{ _device, vkDestroyPipeline };

	VDeleter<VkDebugReportCallbackEXT> _validationCallback{ _instance, DestroyDebugReportCallbackEXT };

	bool createShaderModule(const std::string& shaderPath, VDeleter<VkShaderModule>& shaderModule );
};

#endif //VULKAN_GRAPHIC_H_