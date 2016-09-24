#ifndef VULKAN_GRAPHIC_H_
#define VULKAN_GRAPHIC_H_

#include <vulkan\vulkan.h>
#include "vertex.h"
#include "vkUtils.h"
#include "swapChain.h"
#include <memory>
#include <vector>
#include <fstream>

struct GLFWwindow;
class SwapChain;

void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);

class VulkanGraphic
{
public:
	VulkanGraphic(std::vector<const char*> instanceExtensions);
	~VulkanGraphic();

	bool getPysicalDevices();
	bool createLogicalDevice();
	bool createSurface(GLFWwindow* window);
	bool createSwapChain();
	bool createRenderPass();
	bool createPipeline();
	bool createFrameBuffers();
	bool createCommandPool();
	bool createCommandBuffers();
	bool createSemaphores();
	bool createVertexBuffer(const std::vector<Vertex>& vertices);

	void render();
	void recreateSwapChain();

private:
	struct Queue {
		int familyIndex;
		VkQueue handle;
	};

	bool createShaderModule(const std::string& shaderPath, VDeleter<VkShaderModule>& shaderModule);
	void recreateSwapChainIfNotValid(VkResult res);

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
	std::vector<VDeleter<VkFramebuffer>> _framebuffers;
	VDeleter<VkCommandPool> _commandPool{ _device, vkDestroyCommandPool };
	std::vector<VkCommandBuffer> _commandBuffers;

	VDeleter<VkSemaphore> _imageAvailableSemaphore{ _device, vkDestroySemaphore };
	VDeleter<VkSemaphore> _renderFinishedSemaphore{ _device, vkDestroySemaphore };

	VDeleter<VkBuffer> _vertexBuffer{ _device, vkDestroyBuffer };
	VDeleter<VkDeviceMemory> _vertexBufferMemory{ _device, vkFreeMemory };
	uint32_t _verticesCount;

	VDeleter<VkDebugReportCallbackEXT> _validationCallback{ _instance, DestroyDebugReportCallbackEXT };
	std::ofstream _outErrorFile;
};

#endif //VULKAN_GRAPHIC_H_