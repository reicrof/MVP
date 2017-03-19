#ifndef VULKAN_GRAPHIC_H_
#define VULKAN_GRAPHIC_H_

#include "swapChain.h"
#include "vertex.h"
#include "utils.h"
#include "vkUtils.h"
#include "vMemoryPool.h"
#include "vImage.h"
#include "vCommandPool.h"
#include "vThread.h"
#include "vGeom.h"

#include <gli/gli.hpp>

#include <fstream>
#include <memory>
#include <vector>
#include <atomic>
#include <vulkan/vulkan.h>

struct GLFWwindow;
class VGeom;

void DestroyDebugReportCallbackEXT( VkInstance instance,
                                    VkDebugReportCallbackEXT callback,
                                    const VkAllocationCallbacks* pAllocator );

struct UniformBufferObject
{
   glm::mat4 model;
   glm::mat4 view;
   glm::mat4 proj;
};

struct PBRMaterial
{
	glm::vec3 color;
	float rough;
	float metal;
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
   bool createRenderPass();
   bool createPipelineCache();
   bool createPipeline();
   bool createFrameBuffers();
   bool createCommandPool();
   bool createDescriptorPool();
   VkCommandBuffer createCommandBuffers( unsigned frameIdx );
   bool createSemaphores();
   bool createDescriptorSetLayout();
   bool createDescriptorSet();
   bool createUniformBuffer();
   bool createTextureImage();
   bool createTextureImageView();
   bool createTextureSampler();
   bool createDepthImage();
   bool createIBLTexture();

   std::future<bool> addGeom( const std::vector<Vertex>& vertices,
                              const std::vector<uint32_t>& indices );

   void updateUBO( const UniformBufferObject& ubo, const PBRMaterial& pbr);

   void onNewFrame();
   void render();
   void recreateSwapChain();

   void savePipelineCacheToDisk();

   const SwapChain* getSwapChain() const;

   void _debugPrintMemoryMgrInfo() const;

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
   static VMemAlloc createBuffer(VkDevice device,
								VMemoryManager& memoryManager,
								VkMemoryPropertyFlags memProperty,
								VkDeviceSize size,
								VkBufferUsageFlags usage,
								VkBuffer& buffer);
   void freeBuffer( VMemAlloc& alloc );

   void createImage( uint32_t width,
                     uint32_t height,
					 uint32_t mips,
                     VkFormat format,
                     VkImageTiling tiling,
                     VkImageUsageFlags usage,
                     VkMemoryPropertyFlags memProperty,
                     VImage& image );
   std::unique_ptr< gli::texture > createImage(const std::string& path,
	   VkFormat format,
	   VkImageTiling tiling,
	   VkImageUsageFlags usage,
	   VkMemoryPropertyFlags memProperty,
	   VImage& image);

   bool createCubeMap(const std::string& path, VImage& img, VDeleter<VkImageView>& imgView, VDeleter<VkSampler>& imgSampler);

   bool createShaderModule( const std::string& shaderPath, VDeleter<VkShaderModule>& shaderModule );
   void recreateSwapChainIfNotValid( VkResult res );

   VDeleter<VkInstance> _instance{vkDestroyInstance};
   VDeleter<VkDevice> _device{vkDestroyDevice};
   VkPhysicalDevice _physDevice;
   Queue _graphicQueue;
   Queue _presentationQueue;
   Queue _transferQueue;
   VDeleter<VkSurfaceKHR> _surface{_instance, vkDestroySurfaceKHR};
   std::unique_ptr<SwapChain> _swapChain;
   VDeleter<VkRenderPass> _renderPass{_device, vkDestroyRenderPass};
   VDeleter<VkRenderPass> _widgetRenderPass{_device, vkDestroyRenderPass};
   VDeleter<VkDescriptorSetLayout> _descriptorSetLayout{_device, vkDestroyDescriptorSetLayout};
   VDeleter<VkPipelineCache> _pipelineCache{_device, vkDestroyPipelineCache};
   VDeleter<VkPipelineLayout> _pipelineLayout{_device, vkDestroyPipelineLayout};
   VDeleter<VkPipeline> _graphicsPipeline{_device, vkDestroyPipeline};
   VDeleter<VkPipelineLayout> _widgetPipelineLayout{_device, vkDestroyPipelineLayout};
   VDeleter<VkPipeline> _graphicsWidgetPipeline{_device, vkDestroyPipeline};
   std::vector<VDeleter<VkFramebuffer>> _framebuffers;
   std::vector<VCommandPool> _graphicCommandPools;
   std::vector<VCommandPool> _transferCommandPools;
   VCommandPool _loadCommandPool;
   VDeleter<VkDescriptorPool> _descriptorPool{_device, vkDestroyDescriptorPool};
   VkDescriptorSet _descriptorSet;

   VDeleter<VkSemaphore> _imageAvailableSemaphore{_device, vkDestroySemaphore};
   VDeleter<VkSemaphore> _renderFinishedSemaphore{_device, vkDestroySemaphore};
   VDeleter<VkSemaphore> _uboUpdatedSemaphore{_device, vkDestroySemaphore};

   VDeleter<VkFence> _uboUpdatedFence{_device, vkDestroyFence};
   VkCommandBuffer _uboUpdateCmdBuf;

   VMemoryManager _memoryManager{_physDevice, _device};

   VDeleter<VkBuffer> _uniformStagingBuffer{_device, vkDestroyBuffer};
   VMemAlloc _uniformStagingBufferMemory;
   VDeleter<VkBuffer> _uniformBuffer{_device, vkDestroyBuffer};

   VImage _stagingImage{_device};
   VImage _textureImage{_device};
   VImage _radianceTexture{ _device };
   VImage _irradianceTexture{ _device };
   VDeleter<VkImageView> _textureImageView{_device, vkDestroyImageView};
   VDeleter<VkImageView> _radianceImageView{ _device, vkDestroyImageView };
   VDeleter<VkImageView> _irradianceImageView{ _device, vkDestroyImageView };
   VDeleter<VkSampler> _textureSampler{_device, vkDestroySampler};
   VDeleter<VkSampler> _radianceSampler{ _device, vkDestroySampler };
   VDeleter<VkSampler> _irradianceSampler{ _device, vkDestroySampler };

   VImage _depthImage{_device};
   VDeleter<VkDeviceMemory> _depthImageMemory{_device, vkFreeMemory};
   VDeleter<VkImageView> _depthImageView{_device, vkDestroyImageView};

   std::array<VGeom, 100> _geoms;
   std::atomic_uint32_t _geomsToDraw{0};

   VDeleter<VkDebugReportCallbackEXT> _validationCallback{_instance, DestroyDebugReportCallbackEXT};
   std::unique_ptr<std::ofstream> _outErrorFile;

   uint32_t _curFrameIdx;
   std::vector<VkFence> _frameRenderedFence;

   VThread _thread;
};

#endif  // VULKAN_GRAPHIC_H_
