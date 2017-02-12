#ifndef VULKAN_GRAPHIC_H_
#define VULKAN_GRAPHIC_H_

#include "swapChain.h"
#include "vertex.h"
#include "utils.h"
#include "vkUtils.h"
#include "vMemoryPool.h"
#include "vImage.h"
#include "vCommandPool.h"
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
   bool createWidgetRenderPass();
   bool createPipelineCache();
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
   // bool loadModel(const std::string& path);
   void updateUBO( const UniformBufferObject& ubo );

   void render();
   void recreateSwapChain();

   void savePipelineCacheToDisk();

   const SwapChain* getSwapChain() const;

  private:
   struct Queue
   {
      int familyIndex;
      VkQueue handle;
   };

   VMemAlloc createBuffer( VkMemoryPropertyFlags memProperty,
                           VkDeviceSize size,
                           VkBufferUsageFlags usage,
                           VDeleter<VkBuffer>& buffer );

   void createImage( uint32_t width,
                     uint32_t height,
                     VkFormat format,
                     VkImageTiling tiling,
                     VkImageUsageFlags usage,
                     VkMemoryPropertyFlags memProperty,
                     VImage& image );

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
   VDeleter<VkRenderPass> _widgetRenderPass{_device, vkDestroyRenderPass};
   VDeleter<VkDescriptorSetLayout> _descriptorSetLayout{_device, vkDestroyDescriptorSetLayout};
   VDeleter<VkPipelineCache>  _pipelineCache{ _device, vkDestroyPipelineCache };
   VDeleter<VkPipelineLayout> _pipelineLayout{_device, vkDestroyPipelineLayout};
   VDeleter<VkPipeline> _graphicsPipeline{_device, vkDestroyPipeline};
   VDeleter<VkPipelineLayout> _widgetPipelineLayout{_device, vkDestroyPipelineLayout};
   VDeleter<VkPipeline> _graphicsWidgetPipeline{_device, vkDestroyPipeline};
   std::vector<VDeleter<VkFramebuffer>> _framebuffers;
   VDeleter<VkCommandPool> _commandPool{_device, vkDestroyCommandPool};
   VCommandPool _singleTimeCommandPool{_device, 100};
   std::vector<VkCommandBuffer> _commandBuffers;
   VDeleter<VkDescriptorPool> _descriptorPool{_device, vkDestroyDescriptorPool};
   VkDescriptorSet _descriptorSet;

   VDeleter<VkSemaphore> _imageAvailableSemaphore{_device, vkDestroySemaphore};
   VDeleter<VkSemaphore> _renderFinishedSemaphore{_device, vkDestroySemaphore};

   VDeleter<VkFence> _uboUpdatedFence{_device, vkDestroyFence};
   VkCommandBuffer* _uboUpdateCmdBuf;

   VDeleter<VkBuffer> _vertexBuffer{_device, vkDestroyBuffer};
   VDeleter<VkBuffer> _indexBuffer{_device, vkDestroyBuffer};

   VMemoryManager _memoryManager{_physDevice, _device};
   uint32_t _verticesCount;
   uint32_t _indexCount;

   VDeleter<VkBuffer> _uniformStagingBuffer{_device, vkDestroyBuffer};
   VMemAlloc _uniformStagingBufferMemory;
   VDeleter<VkBuffer> _uniformBuffer{_device, vkDestroyBuffer};

   VImage _stagingImage{_device};

   VImage _textureImage{_device};
   VDeleter<VkDeviceMemory> _textureImageMemory{_device, vkFreeMemory};
   VDeleter<VkImageView> _textureImageView{_device, vkDestroyImageView};
   VDeleter<VkSampler> _textureSampler{_device, vkDestroySampler};

   VImage _depthImage{_device};
   VDeleter<VkDeviceMemory> _depthImageMemory{_device, vkFreeMemory};
   VDeleter<VkImageView> _depthImageView{_device, vkDestroyImageView};

   VDeleter<VkDebugReportCallbackEXT> _validationCallback{_instance, DestroyDebugReportCallbackEXT};
   std::unique_ptr<std::ofstream> _outErrorFile;
};

#endif  // VULKAN_GRAPHIC_H_
