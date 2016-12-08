#ifndef VULKAN_GRAPHIC_H_
#define VULKAN_GRAPHIC_H_

#include "swapChain.h"
#include "vertex.h"
#include "vkUtils.h"
#include "vMemoryPool.h"
#include <fstream>
#include <memory>
#include <vector>
#include <vulkan/vulkan.h>

struct GLFWwindow;

void DestroyDebugReportCallbackEXT( VkInstance instance,
                                    VkDebugReportCallbackEXT callback,
                                    const VkAllocationCallbacks* pAllocator );

struct UniformBufferObject
{
   glm::mat4 model;
   glm::mat4 view;
   glm::mat4 proj;
};

class VulkanGraphic
{
  public:
   VulkanGraphic( std::vector<const char*> instanceExtensions );
   ~VulkanGraphic();

   bool getPysicalDevices();
   bool createLogicalDevice();
   bool createSurface( GLFWwindow* window );
   bool createSwapChain();
   bool createMemoryPool();
   bool createRenderPass();
   bool createPipeline();
   bool createFrameBuffers();
   bool createCommandPool();
   bool createDescriptorPool();
   bool createCommandBuffers();
   bool createSemaphores();
   bool createVertexBuffer( const std::vector<Vertex>& vertices );
   bool createIndexBuffer( const std::vector<uint32_t>& indices );
   bool createDescriptorSetLayout();
   bool createDescriptorSet();
   bool createUniformBuffer();
   bool createTextureImage();
   bool createTextureImageView();
   bool createTextureSampler();
   bool createDepthImage();
   void updateUBO( const UniformBufferObject& ubo );

   void render();
   void recreateSwapChain();

   const SwapChain* getSwapChain() const;

  private:
   struct Queue
   {
      int familyIndex;
      VkQueue handle;
   };

   template< unsigned SIZE >
   VMemAlloc createBuffer(VMemoryPool<SIZE>* pool,
						   VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VDeleter<VkBuffer>& buffer );

   template< unsigned SIZE >
   VMemAlloc createImage( uint32_t width,
                     uint32_t height,
                     VkFormat format,
                     VkImageTiling tiling,
                     VkImageUsageFlags usage,
					 VMemoryPool<SIZE>* memoryPool,
                     VDeleter<VkImage>& image );

   bool createShaderModule( const std::string& shaderPath, VDeleter<VkShaderModule>& shaderModule );
   void recreateSwapChainIfNotValid( VkResult res );

   VDeleter<VkInstance> _instance{vkDestroyInstance};
   VDeleter<VkDevice> _device{vkDestroyDevice};
   VkPhysicalDevice _physDevice;
   Queue _graphicQueue;
   Queue _presentationQueue;
   VDeleter<VkSurfaceKHR> _surface{_instance, vkDestroySurfaceKHR};
   std::unique_ptr<SwapChain> _swapChain;
   VDeleter<VkRenderPass> _renderPass{_device, vkDestroyRenderPass};
   VDeleter<VkDescriptorSetLayout> _descriptorSetLayout{_device, vkDestroyDescriptorSetLayout};
   VDeleter<VkPipelineLayout> _pipelineLayout{_device, vkDestroyPipelineLayout};
   VDeleter<VkPipeline> _graphicsPipeline{_device, vkDestroyPipeline};
   std::vector<VDeleter<VkFramebuffer>> _framebuffers;
   VDeleter<VkCommandPool> _commandPool{_device, vkDestroyCommandPool};
   std::vector<VkCommandBuffer> _commandBuffers;
   VDeleter<VkDescriptorPool> _descriptorPool{_device, vkDestroyDescriptorPool};
   VkDescriptorSet _descriptorSet;

   VDeleter<VkSemaphore> _imageAvailableSemaphore{_device, vkDestroySemaphore};
   VDeleter<VkSemaphore> _renderFinishedSemaphore{_device, vkDestroySemaphore};

   VDeleter<VkBuffer> _vertexBuffer{_device, vkDestroyBuffer};
   VDeleter<VkBuffer> _indexBuffer{_device, vkDestroyBuffer};

   std::unique_ptr<VMemoryPool<1024 * 1024>> _deviceLocalMemPool;
   std::unique_ptr<VMemoryPool<1024 * 1024>> _hostVisibleMemPool;
   uint32_t _verticesCount;
   uint32_t _indexCount;

   VDeleter<VkBuffer> _uniformStagingBuffer{_device, vkDestroyBuffer};
   VMemAlloc _uniformStagingBufferMemory;
   VDeleter<VkBuffer> _uniformBuffer{_device, vkDestroyBuffer};
   // VMemAlloc _uniformBufferMemory;

   VDeleter<VkImage> _stagingImage{_device, vkDestroyImage};

   VDeleter<VkImage> _textureImage{_device, vkDestroyImage};
   VDeleter<VkDeviceMemory> _textureImageMemory{_device, vkFreeMemory};
   VDeleter<VkImageView> _textureImageView{_device, vkDestroyImageView};
   VDeleter<VkSampler> _textureSampler{_device, vkDestroySampler};

   VDeleter<VkImage> _depthImage{_device, vkDestroyImage};
   VDeleter<VkDeviceMemory> _depthImageMemory{_device, vkFreeMemory};
   VDeleter<VkImageView> _depthImageView{_device, vkDestroyImageView};

   VDeleter<VkDebugReportCallbackEXT> _validationCallback{_instance, DestroyDebugReportCallbackEXT};
   std::unique_ptr<std::ofstream> _outErrorFile;
};

#endif  // VULKAN_GRAPHIC_H_

template<unsigned SIZE>
inline VMemAlloc VulkanGraphic::createBuffer(VMemoryPool<SIZE>* memoryPool, VkDeviceSize size, VkBufferUsageFlags usage, VDeleter<VkBuffer>& buffer)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CALL(vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer));

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

	const VMemAlloc alloc =
		memoryPool->alloc(memRequirements.size, memRequirements.alignment);

	vkBindBufferMemory(_device, buffer, alloc.memory, alloc.offset);

	return alloc;
}

template<unsigned SIZE>
inline VMemAlloc VulkanGraphic::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VMemoryPool<SIZE>* memoryPool, VDeleter<VkImage>& image )
{
	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;  // Optional

	VK_CALL(vkCreateImage(_device, &imageInfo, nullptr, &image));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(_device, image, &memRequirements);

	const VMemAlloc alloc =
		memoryPool->alloc(memRequirements.size, memRequirements.alignment);

	VK_CALL(vkBindImageMemory(_device, image, alloc.memory, alloc.offset));

	return alloc;
}
