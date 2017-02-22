#include "vulkanGraphic.h"
#include "swapChain.h"
#include "utils.h"
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <set>
#include <string>
#include <vector>
#include <cstring>
#include <stdio.h>
#include <fstream>
#include <numeric>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

const std::vector<const char*> VALIDATION_LAYERS = {"VK_LAYER_LUNARG_standard_validation"};

const std::vector<const char*> DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

const char* PIPELINE_CACHE_FILE_NAME = "PipelineCache.vk";

const bool enableValidationLayers = true;

VkResult CreateDebugReportCallbackEXT( VkInstance instance,
                                       const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
                                       const VkAllocationCallbacks* pAllocator,
                                       VkDebugReportCallbackEXT* pCallback )
{
   auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugReportCallbackEXT" );
   if ( func != nullptr )
   {
      return func( instance, pCreateInfo, pAllocator, pCallback );
   }
   else
   {
      return VK_ERROR_EXTENSION_NOT_PRESENT;
   }
}

void DestroyDebugReportCallbackEXT( VkInstance instance,
                                    VkDebugReportCallbackEXT callback,
                                    const VkAllocationCallbacks* pAllocator )
{
   auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugReportCallbackEXT" );
   if ( func != nullptr )
   {
      func( instance, callback, pAllocator );
   }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback( VkDebugReportFlagsEXT flags,
                                                     VkDebugReportObjectTypeEXT objType,
                                                     uint64_t obj,
                                                     size_t location,
                                                     int32_t code,
                                                     const char* layerPrefix,
                                                     const char* msg,
                                                     void* userData )
{
   std::cerr << "validation layer: " << msg << std::endl;
   ( *static_cast<std::ofstream*>( userData ) ) << msg << std::endl;
   return VK_FALSE;
}

namespace
{
void setupDebugCallback( const VDeleter<VkInstance>& instance,
                         VDeleter<VkDebugReportCallbackEXT>& callback,
                         std::unique_ptr<std::ofstream>& outErrorFile )
{
   if ( !enableValidationLayers )
      return;

   outErrorFile =
      std::unique_ptr<std::ofstream>( new std::ofstream( "VulkanErrors.txt", std::ios::out ) );
   VkDebugReportCallbackCreateInfoEXT createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
   createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
   createInfo.pfnCallback = debugCallback;
   createInfo.pUserData = outErrorFile.get();

   VK_CALL( CreateDebugReportCallbackEXT( instance, &createInfo, nullptr, &callback ) );
}

bool checkValidationLayerSupport()
{
   uint32_t layerCount;
   vkEnumerateInstanceLayerProperties( &layerCount, nullptr );

   std::vector<VkLayerProperties> availableLayers( layerCount );
   vkEnumerateInstanceLayerProperties( &layerCount, availableLayers.data() );

   for ( const char* layerName : VALIDATION_LAYERS )
   {
      bool layerFound = false;

      for ( const auto& layerProperties : availableLayers )
      {
         if ( std::strcmp( layerName, layerProperties.layerName ) == 0 )
         {
            layerFound = true;
            break;
         }
      }

      if ( !layerFound )
      {
         return false;
      }
   }

   return true;
}

bool isExtensionAvailable( const char* ext, const std::vector<VkExtensionProperties>& extList )
{
   return std::find_if( extList.begin(), extList.end(),
                        [&ext]( const VkExtensionProperties& extension ) {
                           return strcmp( ext, extension.extensionName );
                        } ) != extList.end();
}

bool areDeviceExtensionsSupported( const VkPhysicalDevice& device,
                                   const std::vector<const char*> extensions )
{
   bool success = true;
   uint32_t extensionCount;
   vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount, nullptr );
   std::vector<VkExtensionProperties> extensionPropertiesAvailable( extensionCount );
   vkEnumerateDeviceExtensionProperties( device, nullptr, &extensionCount,
                                         extensionPropertiesAvailable.data() );

   for ( const auto& ext : extensions )
   {
      success &= isExtensionAvailable( ext, extensionPropertiesAvailable );
   }

   return success;
}

void endSingleTimeCommands( VkCommandBuffer commandBuffer,
                            VkDevice device,
                            VkQueue& queue,
                            VCommandPool& commandPool,
                            uint32_t waitSemCount = 0,
                            VkSemaphore* waitSem = nullptr,
                            uint32_t signalSemCount = 0,
                            VkSemaphore* signalSem = nullptr,
                            VkFence fenceToSignal = VK_NULL_HANDLE )
{
   vkEndCommandBuffer( commandBuffer );

   VkSubmitInfo submitInfo = {
      VK_STRUCTURE_TYPE_SUBMIT_INFO,
      nullptr,
      waitSemCount,
      waitSem,
      nullptr,
      1,
      &commandBuffer,
      signalSemCount,
      signalSem,
   };

   vkQueueSubmit( queue, 1, &submitInfo, fenceToSignal );
}

VkCommandBuffer copyBuffer( VkBuffer source,
                            VkBuffer dest,
                            VkDeviceSize size,
                            VkDevice device,
                            VCommandPool& commandPool,
                            VkQueue& queue,
                            uint32_t waitSemCount = 0,
                            VkSemaphore* waitSem = nullptr,
                            uint32_t signalSemCount = 0,
                            VkSemaphore* signalSem = nullptr,
                            VkFence fenceToSignal = VK_NULL_HANDLE )
{
   VkCommandBuffer commandBuffer = commandPool.alloc( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

   VkBufferCopy copyRegion = {};
   copyRegion.size = size;
   vkCmdCopyBuffer( commandBuffer, source, dest, 1, &copyRegion );

   endSingleTimeCommands( commandBuffer, device, queue, commandPool, waitSemCount, waitSem,
                          signalSemCount, signalSem, fenceToSignal );

   return commandBuffer;
}

void copyImage( VkImage srcImage,
                VkImage dstImage,
                uint32_t width,
                uint32_t height,
                VDeleter<VkDevice>& device,
                VCommandPool& commandPool,
                VkQueue& queue )
{
   VkCommandBuffer commandBuffer = commandPool.alloc( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

   VkImageSubresourceLayers subResource = {};
   subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   subResource.baseArrayLayer = 0;
   subResource.mipLevel = 0;
   subResource.layerCount = 1;

   VkImageCopy region = {};
   region.srcSubresource = subResource;
   region.dstSubresource = subResource;
   region.srcOffset = {0, 0, 0};
   region.dstOffset = {0, 0, 0};
   region.extent.width = width;
   region.extent.height = height;
   region.extent.depth = 1;

   vkCmdCopyImage( commandBuffer, srcImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dstImage,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region );

   endSingleTimeCommands( commandBuffer, device, queue, commandPool );
}

VkAccessFlags vkImageLayoutToAccessFlags( VkImageLayout layout )
{
   switch ( layout )
   {
      case VK_IMAGE_LAYOUT_PREINITIALIZED:
         return VK_ACCESS_HOST_WRITE_BIT;
      case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
         return VK_ACCESS_TRANSFER_READ_BIT;
      case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
         return VK_ACCESS_TRANSFER_WRITE_BIT;
      case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
         return VK_ACCESS_SHADER_READ_BIT;
      case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
         return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      case VK_IMAGE_LAYOUT_UNDEFINED:
         return 0;
      default:
         assert( !"Invalid img laout" );
         return 0;
   }
}

void transitionImageLayout( VkImage image,
                            VkFormat format,
                            VkImageLayout oldLayout,
                            VkImageLayout newLayout,
                            VDeleter<VkDevice>& device,
                            VCommandPool& commandPool,
                            VkQueue& queue )
{
   VkCommandBuffer commandBuffer = commandPool.alloc( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

   VkImageMemoryBarrier barrier = {};
   barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.oldLayout = oldLayout;
   barrier.newLayout = newLayout;
   // Queue family ownership
   barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

   barrier.image = image;

   if ( newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL )
   {
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

      // If has stencil
      if ( format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT )
      {
         barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
   }
   else
   {
      barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
   }

   barrier.subresourceRange.baseMipLevel = 0;
   barrier.subresourceRange.levelCount = 1;
   barrier.subresourceRange.baseArrayLayer = 0;
   barrier.subresourceRange.layerCount = 1;
   barrier.srcAccessMask = vkImageLayoutToAccessFlags( oldLayout );
   barrier.dstAccessMask = vkImageLayoutToAccessFlags( newLayout );

   vkCmdPipelineBarrier( commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                         &barrier );

   endSingleTimeCommands( commandBuffer, device, queue, commandPool );
}

void createImageView( VkImage image,
                      VkFormat format,
                      VkImageAspectFlags aspectFlags,
                      VDeleter<VkImageView>& imageView,
                      VDeleter<VkDevice>& device )
{
   VkImageViewCreateInfo viewInfo = {};
   viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   viewInfo.image = image;
   viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
   viewInfo.format = format;
   viewInfo.subresourceRange.aspectMask = aspectFlags;
   viewInfo.subresourceRange.baseMipLevel = 0;
   viewInfo.subresourceRange.levelCount = 1;
   viewInfo.subresourceRange.baseArrayLayer = 0;
   viewInfo.subresourceRange.layerCount = 1;

   VK_CALL( vkCreateImageView( device, &viewInfo, nullptr, &imageView ) );
}

}  // End of anonymous namespace

void VulkanGraphic::recreateSwapChain()
{
   vkDeviceWaitIdle( _device );
   _swapChain.reset( new SwapChain( _physDevice, _device, _surface, VK_SHARING_MODE_EXCLUSIVE,
                                    _swapChain->_handle ) );

   createPipeline();
   createDepthImage();
   createFrameBuffers();
}

void VulkanGraphic::savePipelineCacheToDisk()
{
   size_t size = -1;
   // Get the size of the cache
   VK_CALL( vkGetPipelineCacheData( _device, _pipelineCache, &size, nullptr ) );
   std::vector<unsigned char> vecData( size );
   VK_CALL( vkGetPipelineCacheData( _device, _pipelineCache, &size, vecData.data() ) );

   std::ofstream out( PIPELINE_CACHE_FILE_NAME, std::ios::binary );
   out.write( (char*)vecData.data(), size );
}

VulkanGraphic::VulkanGraphic( std::vector<const char*> instanceExtensions )
{
   VkApplicationInfo appInfo = {};
   appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
   appInfo.pApplicationName = "MVP";
   appInfo.applicationVersion = VK_MAKE_VERSION( 1, 0, 0 );
   appInfo.pEngineName = "No Engine";
   appInfo.engineVersion = VK_MAKE_VERSION( 1, 0, 0 );
   appInfo.apiVersion = VK_API_VERSION_1_0;

   VkInstanceCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
   createInfo.pApplicationInfo = &appInfo;

   if ( enableValidationLayers && !checkValidationLayerSupport() )
   {
      std::cerr << "Validation layer not supported and will not be enabled." << std::endl;
   }

   if ( enableValidationLayers )
   {
      instanceExtensions.push_back( VK_EXT_DEBUG_REPORT_EXTENSION_NAME );
   }

   createInfo.enabledExtensionCount = static_cast<uint32_t>( instanceExtensions.size() );
   createInfo.ppEnabledExtensionNames = instanceExtensions.data();

   if ( enableValidationLayers )
   {
      createInfo.enabledLayerCount = static_cast<uint32_t>( VALIDATION_LAYERS.size() );
      createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
   }
   else
   {
      createInfo.enabledLayerCount = 0;
   }

   VK_CALL( vkCreateInstance( &createInfo, nullptr, &_instance ) );
   setupDebugCallback( _instance, _validationCallback, _outErrorFile );
}

void VulkanGraphic::recreateSwapChainIfNotValid( VkResult res )
{
   switch ( res )
   {
      case VK_SUBOPTIMAL_KHR:
      case VK_ERROR_OUT_OF_DATE_KHR:
         recreateSwapChain();
         break;
      default:
         break;
   }
}

bool VulkanGraphic::getPysicalDevices()
{
   uint32_t deviceCount = 0;
   vkEnumeratePhysicalDevices( _instance, &deviceCount, nullptr );

   std::vector<VkPhysicalDevice> devices( deviceCount );
   vkEnumeratePhysicalDevices( _instance, &deviceCount, devices.data() );

   int graphicFamily = -1;
   int presentFamily = -1;
   VkPhysicalDeviceFeatures features;
   VkPhysicalDeviceProperties properties;
   for ( size_t i = 0; i < deviceCount; ++i )
   {
      vkGetPhysicalDeviceProperties( devices[ i ], &properties );
      vkGetPhysicalDeviceFeatures( devices[ i ], &features );

      uint32_t queueFamilyCount = 0;
      vkGetPhysicalDeviceQueueFamilyProperties( devices[ i ], &queueFamilyCount, nullptr );

      std::vector<VkQueueFamilyProperties> queueFamilies( queueFamilyCount );
      vkGetPhysicalDeviceQueueFamilyProperties( devices[ i ], &queueFamilyCount,
                                                queueFamilies.data() );

      std::vector<VkBool32> queuePresentSupport( queueFamilyCount );
      for ( uint32_t j = 0; j < queueFamilyCount; ++j )
      {
         VkBool32 presentationSupported = VK_FALSE;
         vkGetPhysicalDeviceSurfaceSupportKHR( devices[ i ], j, _surface, &presentationSupported );
         if ( queueFamilies[ j ].queueCount > 0 &&
              queueFamilies[ j ].queueFlags & VK_QUEUE_GRAPHICS_BIT )
         {
            graphicFamily = j;

            // If the queue support both presentation and graphic, stop here.
            if ( presentationSupported )
            {
               presentFamily = j;
               break;
            }
         }

         if ( presentationSupported )
         {
            presentFamily = j;
         }
      }

      // Get a transfer queue as well.
      for ( uint32_t j = 0; j < queueFamilyCount; ++j )
      {
         if ( queueFamilies[ j ].queueCount > 0 &&
              queueFamilies[ j ].queueFlags & VK_QUEUE_TRANSFER_BIT &&
              ( j != _graphicQueue.familyIndex || j != _presentationQueue.familyIndex ) )
         {
            _transferQueue.familyIndex = j;
         }
      }

      // We have found the one.
      if ( graphicFamily >= 0 && presentFamily >= 0 )
      {
         _physDevice = devices[ i ];

         _graphicQueue.familyIndex = graphicFamily;
         _presentationQueue.familyIndex = presentFamily;
         return true;
      }
   }

   return false;
}

bool VulkanGraphic::createLogicalDevice()
{
   std::set<int> uniqueFamilliesIndex = {_graphicQueue.familyIndex, _presentationQueue.familyIndex,
                                         _transferQueue.familyIndex};
   std::vector<VkDeviceQueueCreateInfo> queueCreateInfos( uniqueFamilliesIndex.size() );
   const float queuePriority = 1.0f;
   size_t count = 0;
   for ( auto i = uniqueFamilliesIndex.begin(); i != uniqueFamilliesIndex.end(); ++i )
   {
      queueCreateInfos[ count ].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfos[ count ].queueFamilyIndex = *i;
      queueCreateInfos[ count ].queueCount = 1;
      queueCreateInfos[ count ].pQueuePriorities = &queuePriority;
      ++count;
   }

   // No special feature for now.
   VkPhysicalDeviceFeatures deviceFeatures = {};

   VkDeviceCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
   createInfo.pQueueCreateInfos = queueCreateInfos.data();
   createInfo.queueCreateInfoCount = static_cast<uint32_t>( queueCreateInfos.size() );
   createInfo.pEnabledFeatures = &deviceFeatures;

   VERIFY( areDeviceExtensionsSupported( _physDevice, DEVICE_EXTENSIONS ),
           "Not all extensions are supported." );

   createInfo.enabledExtensionCount = static_cast<uint32_t>( DEVICE_EXTENSIONS.size() );
   createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

   if ( enableValidationLayers )
   {
      createInfo.enabledLayerCount = static_cast<uint32_t>( VALIDATION_LAYERS.size() );
      createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();
   }
   else
   {
      createInfo.enabledLayerCount = 0;
   }

   VK_CALL( vkCreateDevice( _physDevice, &createInfo, nullptr, &_device ) );

   // Get the handle of the queue.
   vkGetDeviceQueue( _device, _graphicQueue.familyIndex, 0, &_graphicQueue.handle );
   vkGetDeviceQueue( _device, _presentationQueue.familyIndex, 0, &_presentationQueue.handle );
   vkGetDeviceQueue( _device, _transferQueue.familyIndex, 0, &_transferQueue.handle );

   auto& r = _thread.getThreadResources();
   r._device = _device;
   r._graphicQueueFamilly = _graphicQueue.familyIndex;
   r._transferQueueFamilly = _transferQueue.familyIndex;
   vkGetDeviceQueue(_device, _graphicQueue.familyIndex, 0, &r._graphicQueue);
   vkGetDeviceQueue(_device, _transferQueue.familyIndex, 0, &r._transferQueue);

   _memoryManager.init(_physDevice, _device);
   _thread.init(_physDevice, _device);

   return true;
}

bool VulkanGraphic::createSurface( GLFWwindow* window )
{
   VK_CALL( glfwCreateWindowSurface( _instance, window, nullptr, &_surface ) );
   return true;
}

bool VulkanGraphic::createSwapChain()
{
   _swapChain = std::make_unique<SwapChain>( _physDevice, _device, _surface );

   return true;
}

bool VulkanGraphic::createRenderPass()
{
   // Color attachement
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

   // Depth attachement
   VkAttachmentDescription depthAttachment = {};
   depthAttachment.format = VK_FORMAT_D32_SFLOAT;
   depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
   depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
   depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
   depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
   depthAttachment.initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
   depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   VkAttachmentReference depthAttachmentRef = {};
   depthAttachmentRef.attachment = 1;
   depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

   VkSubpassDescription subPass = {};
   subPass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
   subPass.colorAttachmentCount = 1;
   subPass.pColorAttachments = &colorAttachmentRef;
   subPass.pDepthStencilAttachment = &depthAttachmentRef;

   std::vector<VkSubpassDependency> subpassDependencies = {
      {
         VK_SUBPASS_EXTERNAL,                   // uint32_t  before subpass
         0,                                     // uint32_t  current subpass
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,  // VkPipelineStageFlags           srcStageMask
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // VkPipelineStageFlags dstStageMask
         VK_ACCESS_MEMORY_READ_BIT,             // VkAccessFlags                  srcAccessMask
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // VkAccessFlags                  dstAccessMask
         VK_DEPENDENCY_BY_REGION_BIT            // VkDependencyFlags              dependencyFlags
      },
      {
         0,                                              // uint32_t  current subpass
         VK_SUBPASS_EXTERNAL,                            // uint32_t  after subpass
         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // VkPipelineStageFlags srcStageMask
         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,  // VkPipelineStageFlags           dstStageMask
         VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,  // VkAccessFlags                  srcAccessMask
         VK_ACCESS_MEMORY_READ_BIT,             // VkAccessFlags                  dstAccessMask
         VK_DEPENDENCY_BY_REGION_BIT            // VkDependencyFlags              dependencyFlags
      }};

   std::array<VkAttachmentDescription, 2> attachments = {{colorAttachment, depthAttachment}};
   VkRenderPassCreateInfo renderPassInfo = {};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
   renderPassInfo.pAttachments = attachments.data();
   renderPassInfo.subpassCount = 1;
   renderPassInfo.pSubpasses = &subPass;
   renderPassInfo.dependencyCount = static_cast<uint32_t>( subpassDependencies.size() );
   renderPassInfo.pDependencies = subpassDependencies.data();

   VK_CALL( vkCreateRenderPass( _device, &renderPassInfo, nullptr, &_renderPass ) );

   return true;
}

bool VulkanGraphic::createShaderModule( const std::string& shaderPath,
                                        VDeleter<VkShaderModule>& shaderModule )
{
   std::ifstream shaderFile{shaderPath, std::ios::binary};
   std::vector<char> shaderSource( std::istreambuf_iterator<char>{shaderFile},
                                   std::istreambuf_iterator<char>{} );
   if ( shaderSource.empty() )
   {
      std::cerr << "Cannot find shader : " << shaderPath << std::endl;
      return false;
   }

   VkShaderModuleCreateInfo shaderModuleCreateInfo = {
      VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, shaderSource.size(),
      reinterpret_cast<const uint32_t*>( shaderSource.data() )};

   if ( vkCreateShaderModule( _device, &shaderModuleCreateInfo, nullptr, &shaderModule ) !=
        VK_SUCCESS )
   {
      std::cerr << "Error while creating shader module for shader : " << shaderPath << std::endl;
      return false;
   }

   return true;
}

bool VulkanGraphic::createDescriptorSetLayout()
{
   VkDescriptorSetLayoutBinding uboLayoutBinding = {};
   uboLayoutBinding.binding = 0;
   uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   uboLayoutBinding.descriptorCount = 1;
   uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
   uboLayoutBinding.pImmutableSamplers = nullptr;

   VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
   samplerLayoutBinding.binding = 1;
   samplerLayoutBinding.descriptorCount = 1;
   samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   samplerLayoutBinding.pImmutableSamplers = nullptr;
   samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

   std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
      {uboLayoutBinding, samplerLayoutBinding}};
   VkDescriptorSetLayoutCreateInfo layoutInfo = {};
   layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
   layoutInfo.bindingCount = static_cast<uint32_t>( bindings.size() );
   layoutInfo.pBindings = bindings.data();

   VK_CALL( vkCreateDescriptorSetLayout( _device, &layoutInfo, nullptr, &_descriptorSetLayout ) );

   return true;
}

bool VulkanGraphic::createPipelineCache()
{
   // Try to load from disk fist
   std::ifstream cachedFile( PIPELINE_CACHE_FILE_NAME, std::ios::binary );
   if ( cachedFile.is_open() )
   {
      cachedFile.seekg( 0, std::ios::end );
      std::vector<unsigned char> data( cachedFile.tellg() );
      cachedFile.seekg( 0, std::ios::beg );

      data.assign( ( std::istreambuf_iterator<char>( cachedFile ) ),
                   std::istreambuf_iterator<char>() );

      VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
      pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
      pipelineCacheCreateInfo.pInitialData = data.data();
      pipelineCacheCreateInfo.initialDataSize = data.size();
      VK_CALL(
         vkCreatePipelineCache( _device, &pipelineCacheCreateInfo, nullptr, &_pipelineCache ) );
   }
   else
   {
      VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
      pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
      VK_CALL(
         vkCreatePipelineCache( _device, &pipelineCacheCreateInfo, nullptr, &_pipelineCache ) );
   }

   return true;
}

bool VulkanGraphic::createPipeline()
{
   VDeleter<VkShaderModule> vertShaderModule{_device, vkDestroyShaderModule};
   VDeleter<VkShaderModule> fragShaderModule{_device, vkDestroyShaderModule};
   createShaderModule( "../shaders/vert.spv", vertShaderModule );
   createShaderModule( "../shaders/frag.spv", fragShaderModule );

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

   VkPipelineShaderStageCreateInfo shaderStages[] = {vShaderStageCreateInfo, fShaderStageInfo};

   VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
   vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
   auto bindingDescription = Vertex::getBindingDescription();
   auto attributeDescriptions = Vertex::getAttributeDescriptions();

   vertexInputInfo.vertexBindingDescriptionCount = 1;
   vertexInputInfo.vertexAttributeDescriptionCount =
      static_cast<uint32_t>( attributeDescriptions.size() );
   vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
   vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

   VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
   inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
   inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
   inputAssembly.primitiveRestartEnable = VK_FALSE;

   // Dynamic states
   std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                VK_DYNAMIC_STATE_SCISSOR};

   VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, nullptr, 0,
      static_cast<uint32_t>( dynamicStates.size() ), dynamicStates.data()};

   VkPipelineViewportStateCreateInfo viewportState = {};
   viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
   viewportState.viewportCount = 1;
   viewportState.pViewports = nullptr;
   viewportState.scissorCount = 1;
   viewportState.pScissors = nullptr;

   VkPipelineRasterizationStateCreateInfo rasterizer = {};
   rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
   rasterizer.depthClampEnable = VK_FALSE;
   rasterizer.rasterizerDiscardEnable = VK_FALSE;
   rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
   rasterizer.lineWidth = 1.0f;
   rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
   rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
   rasterizer.depthBiasEnable = VK_FALSE;

   VkPipelineMultisampleStateCreateInfo multisampling = {};
   multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
   multisampling.sampleShadingEnable = VK_FALSE;
   multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

   VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
   colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
   colorBlendAttachment.blendEnable = VK_FALSE;

   VkPipelineColorBlendStateCreateInfo colorBlending = {};
   colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
   colorBlending.logicOpEnable = VK_FALSE;
   colorBlending.logicOp = VK_LOGIC_OP_COPY;
   colorBlending.attachmentCount = 1;
   colorBlending.pAttachments = &colorBlendAttachment;
   colorBlending.blendConstants[ 0 ] = 0.0f;
   colorBlending.blendConstants[ 1 ] = 0.0f;
   colorBlending.blendConstants[ 2 ] = 0.0f;
   colorBlending.blendConstants[ 3 ] = 0.0f;

   VkPipelineDepthStencilStateCreateInfo depthStencil = {};
   depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
   depthStencil.depthTestEnable = VK_TRUE;
   depthStencil.depthWriteEnable = VK_TRUE;
   depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
   depthStencil.depthBoundsTestEnable = VK_FALSE;
   depthStencil.minDepthBounds = 0.0f;  // Optional
   depthStencil.maxDepthBounds = 1.0f;  // Optional
   depthStencil.stencilTestEnable = VK_FALSE;
   depthStencil.front = {};  // Optional
   depthStencil.back = {};   // Optional

   VkDescriptorSetLayout setLayout[] = {_descriptorSetLayout};
   VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
   pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipelineLayoutInfo.setLayoutCount = 1;
   pipelineLayoutInfo.pSetLayouts = setLayout;

   VK_CALL( vkCreatePipelineLayout( _device, &pipelineLayoutInfo, nullptr, &_pipelineLayout ) );
   VK_CALL(
      vkCreatePipelineLayout( _device, &pipelineLayoutInfo, nullptr, &_widgetPipelineLayout ) );

   VkGraphicsPipelineCreateInfo pipelineInfo = {};
   pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
   pipelineInfo.stageCount = 2;
   pipelineInfo.pStages = shaderStages;

   pipelineInfo.pVertexInputState = &vertexInputInfo;
   pipelineInfo.pInputAssemblyState = &inputAssembly;
   pipelineInfo.pViewportState = &viewportState;
   pipelineInfo.pRasterizationState = &rasterizer;
   pipelineInfo.pMultisampleState = &multisampling;
   pipelineInfo.pDepthStencilState = &depthStencil;
   pipelineInfo.pColorBlendState = &colorBlending;
   pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
   pipelineInfo.layout = _pipelineLayout;
   pipelineInfo.renderPass = _renderPass;
   pipelineInfo.subpass = 0;
   pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
   pipelineInfo.basePipelineIndex = -1;  // Optional

   VK_CALL( vkCreateGraphicsPipelines( _device, _pipelineCache, 1, &pipelineInfo, nullptr,
                                       &_graphicsPipeline ) );

   // Create widget pipeline

   VDeleter<VkShaderModule> widgetvertShaderModule{_device, vkDestroyShaderModule};
   VDeleter<VkShaderModule> widgetfragShaderModule{_device, vkDestroyShaderModule};
   createShaderModule( "../shaders/widgetVert.spv", widgetvertShaderModule );
   createShaderModule( "../shaders/widgetFrag.spv", widgetfragShaderModule );

   // Vertex stage
   VkPipelineShaderStageCreateInfo widgetvShaderStageCreateInfo = {};
   widgetvShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   widgetvShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
   widgetvShaderStageCreateInfo.module = widgetvertShaderModule;
   widgetvShaderStageCreateInfo.pName = "main";

   // Fragment stage
   VkPipelineShaderStageCreateInfo widgetfShaderStageInfo = {};
   widgetfShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   widgetfShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   widgetfShaderStageInfo.module = widgetfragShaderModule;
   widgetfShaderStageInfo.pName = "main";

   VkPipelineShaderStageCreateInfo widgetStages[] = {widgetvShaderStageCreateInfo,
                                                     widgetfShaderStageInfo};

   pipelineInfo.pStages = widgetStages;

   depthStencil.depthTestEnable = VK_FALSE;
   pipelineInfo.pDepthStencilState = &depthStencil;

   VK_CALL( vkCreateGraphicsPipelines( _device, _pipelineCache, 1, &pipelineInfo, nullptr,
                                       &_graphicsWidgetPipeline ) );

   return true;
}

bool VulkanGraphic::createFrameBuffers()
{
   _framebuffers.resize( _swapChain->_imageCount,
                         VDeleter<VkFramebuffer>{_device, vkDestroyFramebuffer} );
   for ( uint32_t i = 0; i < _swapChain->_imageCount; ++i )
   {
      std::array<VkImageView, 2> attachments = {{_swapChain->_imageViews[ i ], _depthImageView}};

      VkFramebufferCreateInfo framebufferInfo = {};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = _renderPass;
      framebufferInfo.attachmentCount = static_cast<uint32_t>( attachments.size() );
      framebufferInfo.pAttachments = attachments.data();
      framebufferInfo.width = _swapChain->_curExtent.width;
      framebufferInfo.height = _swapChain->_curExtent.height;
      framebufferInfo.layers = 1;

      VK_CALL( vkCreateFramebuffer( _device, &framebufferInfo, nullptr, &_framebuffers[ i ] ) );
   }

   return true;
}

bool VulkanGraphic::createCommandPool()
{
   // Create one graphic command pool per swapchain image.
   VkCommandPoolCreateInfo frameImagePoolInfo = {};
   frameImagePoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   frameImagePoolInfo.queueFamilyIndex = _graphicQueue.familyIndex;
   frameImagePoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
   _graphicCommandPools.resize( _swapChain->_imageCount );
   for ( auto& p : _graphicCommandPools )
   {
      p.init( *_device.get(), 5, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, _graphicQueue.familyIndex );
   }

   // Create one transfer command pool per swapchain image.
   VkCommandPoolCreateInfo transferPoolInfo = {};
   transferPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   transferPoolInfo.queueFamilyIndex = _transferQueue.familyIndex;
   transferPoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
   _transferCommandPools.resize( _swapChain->_imageCount );
   for ( auto& p : _transferCommandPools )
   {
      p.init( *_device.get(), 5, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, _transferQueue.familyIndex );
   }

   _loadCommandPool.init( *_device.get(), 10, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                          _transferQueue.familyIndex );

   return true;
}

bool VulkanGraphic::createDescriptorPool()
{
   std::array<VkDescriptorPoolSize, 2> poolSizes = {};
   poolSizes[ 0 ].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   poolSizes[ 0 ].descriptorCount = 1;
   poolSizes[ 1 ].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   poolSizes[ 1 ].descriptorCount = 1;

   VkDescriptorPoolCreateInfo poolInfo = {};
   poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
   poolInfo.poolSizeCount = static_cast<uint32_t>( poolSizes.size() );
   poolInfo.pPoolSizes = poolSizes.data();
   poolInfo.maxSets = 1;

   VK_CALL( vkCreateDescriptorPool( _device, &poolInfo, nullptr, &_descriptorPool ) );

   return true;
}

bool VulkanGraphic::createDescriptorSet()
{
   VkDescriptorSetLayout layouts[] = {_descriptorSetLayout};
   VkDescriptorSetAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
   allocInfo.descriptorPool = _descriptorPool;
   allocInfo.descriptorSetCount = 1;
   allocInfo.pSetLayouts = layouts;

   VK_CALL( vkAllocateDescriptorSets( _device, &allocInfo, &_descriptorSet ) );

   VkDescriptorBufferInfo bufferInfo = {};
   bufferInfo.buffer = _uniformBuffer;
   bufferInfo.offset = 0;
   bufferInfo.range = sizeof( UniformBufferObject );

   VkDescriptorImageInfo imageInfo = {};
   imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   imageInfo.imageView = _textureImageView;
   imageInfo.sampler = _textureSampler;

   std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};

   descriptorWrites[ 0 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   descriptorWrites[ 0 ].dstSet = _descriptorSet;
   descriptorWrites[ 0 ].dstBinding = 0;
   descriptorWrites[ 0 ].dstArrayElement = 0;
   descriptorWrites[ 0 ].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
   descriptorWrites[ 0 ].descriptorCount = 1;
   descriptorWrites[ 0 ].pBufferInfo = &bufferInfo;
   descriptorWrites[ 0 ].pImageInfo = nullptr;
   descriptorWrites[ 0 ].pTexelBufferView = nullptr;

   descriptorWrites[ 1 ].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
   descriptorWrites[ 1 ].dstSet = _descriptorSet;
   descriptorWrites[ 1 ].dstBinding = 1;
   descriptorWrites[ 1 ].dstArrayElement = 0;
   descriptorWrites[ 1 ].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
   descriptorWrites[ 1 ].descriptorCount = 1;
   descriptorWrites[ 1 ].pBufferInfo = nullptr;
   descriptorWrites[ 1 ].pImageInfo = &imageInfo;
   descriptorWrites[ 1 ].pTexelBufferView = nullptr;

   vkUpdateDescriptorSets( _device, static_cast<uint32_t>( descriptorWrites.size() ),
                           descriptorWrites.data(), 0, nullptr );

   return true;
}

static VMemAlloc static_createBuffer(VkDevice device,
	VMemoryManager& memoryManager,
	VkMemoryPropertyFlags memProperty,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkBuffer& buffer)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CALL(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	const VMemAlloc alloc = memoryManager.alloc(memRequirements, memProperty);

	vkBindBufferMemory(device, buffer, alloc.memory, alloc.offset);

	return alloc;
}

inline VMemAlloc VulkanGraphic::createBuffer( VkMemoryPropertyFlags memProperty,
                                              VkDeviceSize size,
                                              VkBufferUsageFlags usage,
                                              VDeleter<VkBuffer>& buffer )
{
   VkBufferCreateInfo bufferInfo = {};
   bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferInfo.size = size;
   bufferInfo.usage = usage;
   bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VK_CALL( vkCreateBuffer( _device, &bufferInfo, nullptr, &buffer ) );

   VkMemoryRequirements memRequirements;
   vkGetBufferMemoryRequirements( _device, buffer, &memRequirements );

   const VMemAlloc alloc = _memoryManager.alloc( memRequirements, memProperty );

   vkBindBufferMemory( _device, buffer, alloc.memory, alloc.offset );

   return alloc;
}

void VulkanGraphic::freeBuffer( VMemAlloc& alloc )
{
   _memoryManager.free( alloc );
}

void VulkanGraphic::createImage( uint32_t width,
                                 uint32_t height,
                                 VkFormat format,
                                 VkImageTiling tiling,
                                 VkImageUsageFlags usage,
                                 VkMemoryPropertyFlags memProperty,
                                 VImage& image )
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

   VK_CALL( vkCreateImage( _device, &imageInfo, nullptr, &image ) );

   VkMemoryRequirements memRequirements;
   vkGetImageMemoryRequirements( _device, image, &memRequirements );

   // Free the image memory if it was already allocated
   if ( image.isAllocated() )
   {
      _memoryManager.free( image.getMemory() );
   }

   image.setMemory( _memoryManager.alloc( memRequirements, memProperty ) );

   VK_CALL(
      vkBindImageMemory( _device, image, image.getMemory().memory, image.getMemory().offset ) );
}

VkCommandBuffer VulkanGraphic::createCommandBuffers( unsigned frameIdx )
{
   VkCommandBuffer commandBuffer =
      _graphicCommandPools[ frameIdx ].alloc( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

   VkRenderPassBeginInfo renderPassInfo = {};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
   renderPassInfo.renderPass = _renderPass;
   renderPassInfo.framebuffer = _framebuffers[ frameIdx ];
   renderPassInfo.renderArea.offset = {0, 0};
   renderPassInfo.renderArea.extent = _swapChain->_curExtent;

   std::array<VkClearValue, 2> clearColors;
   clearColors[ 0 ].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
   clearColors[ 1 ].depthStencil = {1.0f, 0};

   renderPassInfo.clearValueCount = static_cast<uint32_t>( clearColors.size() );
   renderPassInfo.pClearValues = clearColors.data();

   vkCmdBeginRenderPass( commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

   vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline );

   VkViewport viewport = {0.0f,
                          0.0f,
                          static_cast<float>( _swapChain->_curExtent.width ),
                          static_cast<float>( _swapChain->_curExtent.height ),
                          0.0f,
                          1.0f};

   VkRect2D scissor = {{0, 0}, {_swapChain->_curExtent.width, _swapChain->_curExtent.height}};

   vkCmdSetViewport( commandBuffer, 0, 1, &viewport );
   vkCmdSetScissor( commandBuffer, 0, 1, &scissor );

   vkCmdBindDescriptorSets( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipelineLayout, 0, 1,
                            &_descriptorSet, 0, nullptr );

   if ( _verticesCount > 0 )
   {
      VkBuffer vertexBuffers[] = {_vertexBuffer};
      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers( commandBuffer, 0, 1, vertexBuffers, offsets );
      vkCmdBindIndexBuffer( commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT32 );
      vkCmdDrawIndexed( commandBuffer, _indexCount, 1, 0, 0, 0 );
   }

   vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsWidgetPipeline );

   if ( _verticesCount > 0 )
   {
      VkBuffer vertexBuffers[] = {_vertexBuffer};
      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers( commandBuffer, 0, 1, vertexBuffers, offsets );
      vkCmdBindIndexBuffer( commandBuffer, _indexBuffer, 0, VK_INDEX_TYPE_UINT32 );
      vkCmdDrawIndexed( commandBuffer, _indexCount, 1, 0, 0, 0 );
   }

   vkCmdEndRenderPass( commandBuffer );



   VK_CALL( vkEndCommandBuffer( commandBuffer ) );

   return commandBuffer;
}

bool VulkanGraphic::createSemaphores()
{
   VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
   VK_CALL( vkCreateSemaphore( _device, &semaphoreInfo, nullptr, &_imageAvailableSemaphore ) );
   VK_CALL( vkCreateSemaphore( _device, &semaphoreInfo, nullptr, &_renderFinishedSemaphore ) );
   VK_CALL( vkCreateSemaphore( _device, &semaphoreInfo, nullptr, &_uboUpdatedSemaphore ) );

   VkFenceCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   createInfo.pNext = nullptr;
   createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

   _frameRenderedFence.resize( _swapChain->_imageCount );
   for ( size_t i = 0; i < _swapChain->_imageCount; ++i )
   {
      vkCreateFence( _device, &createInfo, nullptr, &_frameRenderedFence[ i ] );
   }

   return true;
}

bool VulkanGraphic::createVertexBuffer( const std::vector<Vertex>& vertices )
{
   const size_t bufferSize = sizeof( ( vertices[ 0 ] ) ) * vertices.size();

   VDeleter<VkBuffer> stagingBuffer{_device, vkDestroyBuffer};
   VDeleter<VkDeviceMemory> stagingBufferMemory{_device, vkFreeMemory};
   VMemAlloc hostBuffer =
      createBuffer( VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer );
   void* data;
   VK_CALL( vkMapMemory( _device, hostBuffer.memory, hostBuffer.offset, bufferSize, 0, &data ) );
   memcpy( data, vertices.data(), bufferSize );
   vkUnmapMemory( _device, hostBuffer.memory );

   createBuffer( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 _vertexBuffer );

   copyBuffer( stagingBuffer, _vertexBuffer, bufferSize, _device,
                                     _transferCommandPools[ _curFrameIdx ], _transferQueue.handle );

   _verticesCount = static_cast<uint32_t>( vertices.size() );

   vkDeviceWaitIdle( _device );
   freeBuffer( hostBuffer );

   return true;
}

bool VulkanGraphic::createIndexBuffer( const std::vector<uint32_t>& indices )
{
   const size_t bufferSize = indices.size() * sizeof( uint32_t );
   VDeleter<VkBuffer> stagingBuffer{_device, vkDestroyBuffer};
   VDeleter<VkDeviceMemory> stagingBufferMemory{_device, vkFreeMemory};
   VMemAlloc hostBuffer =
      createBuffer( VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer );
   void* data;
   VK_CALL( vkMapMemory( _device, hostBuffer.memory, hostBuffer.offset, bufferSize, 0, &data ) );
   memcpy( data, indices.data(), bufferSize );
   vkUnmapMemory( _device, hostBuffer.memory );

   createBuffer( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 _indexBuffer );

   copyBuffer( stagingBuffer, _indexBuffer, bufferSize, _device,
                                     _transferCommandPools[ _curFrameIdx ], _transferQueue.handle );

   _indexCount = static_cast<uint32_t>( indices.size() );

   vkDeviceWaitIdle( _device );
   freeBuffer( hostBuffer );

   return true;
}

static void addGeomImp(VThread::VThreadResources* resources,
	const std::vector<Vertex>& vertices,
	const std::vector<uint32_t>& indices)
{
	//// Create fence for syncronization
	//VkFence fence;
	//VkFenceCreateInfo createInfo = {};
	//createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	//createInfo.pNext = nullptr;
	//createInfo.flags = 0;
	//vkCreateFence(resources->_device, &createInfo, nullptr, &fence);

	//VkBuffer vertexBuffer, indexBuffer;


	///*VGeom geom;
	//geom._verticesCount = static_cast<uint32_t>(vertices.size());
	//geom._indexCount = static_cast<uint32_t>(indices.size());*/

	//const size_t vertexBufferSize = sizeof((vertices[0])) * vertices.size();
	//const size_t indexBufferSize = indices.size() * sizeof(uint32_t);
	//const size_t combinedSize = vertexBufferSize + indexBufferSize;

	//// Allocate staging buffer for both vertices and indices
	//VDeleter<VkBuffer> stagingBuffer{ resources->_device, vkDestroyBuffer };
	//VDeleter<VkDeviceMemory> stagingBufferMemory{ resources->_device, vkFreeMemory };
	//VMemAlloc hostBuffer =
	//	static_createBuffer(resources->_device, resources->_memoryManager,
	//		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	//		combinedSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, *stagingBuffer.get());

	//// Copy vertices to it
	//void* data;
	//VK_CALL(vkMapMemory(resources->_device, hostBuffer.memory, hostBuffer.offset, combinedSize, 0, &data));
	//memcpy(data, vertices.data(), vertexBufferSize);
	//memcpy(static_cast<char*>(data) + vertexBufferSize, indices.data(), indexBufferSize);
	//vkUnmapMemory(resources->_device, hostBuffer.memory);

	//static_createBuffer(resources->_device, resources->_memoryManager, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize,
	//	VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
	//	vertexBuffer);
	//static_createBuffer(resources->_device, resources->_memoryManager, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize,
	//	VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
	//	indexBuffer);

	//VkCommandBuffer cpyVert = copyBuffer(stagingBuffer, vertexBuffer, vertexBufferSize, resources->_device,
	//	resources->_transferCommandPool, resources->_transferQueue);
	//VkCommandBuffer cpyIdx = copyBuffer(stagingBuffer, indexBuffer, indexBufferSize, resources->_device,
	//	resources->_transferCommandPool, resources->_transferQueue, 0, nullptr, 0, nullptr, fence);

	//// Wait for the transfer to finish
	//VK_CALL(vkWaitForFences(resources->_device, 1, &fence, VK_FALSE, std::numeric_limits<uint64_t>::max()));

	//vkDeviceWaitIdle(resources->_device);
	////{
	////	std::lock_guard<std::mutex> lock(_geomsMutex);
	////	_geoms[_geomsToDraw] = std::move(geom);
	////}
	////++_geomsToDraw;

	////freeBuffer(hostBuffer);
	////_loadCommandPool.free(cpyVert);
	////_loadCommandPool.free(cpyIdx);
}


void VulkanGraphic::addGeom(const std::vector<Vertex>& vertices,
	const std::vector<uint32_t>& indices)
{
	_thread.addJob(addGeomImp, vertices, indices);
}

bool VulkanGraphic::createUniformBuffer()
{
   VkDeviceSize bufferSize = sizeof( UniformBufferObject );

   _uniformStagingBufferMemory =
      createBuffer( VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, _uniformStagingBuffer );
   createBuffer( VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 _uniformBuffer );

   return true;
}

bool VulkanGraphic::createTextureImage()
{
   int width, height, channels;
   stbi_uc* pixels =
      stbi_load( "../textures/chalet.jpg", &width, &height, &channels, STBI_rgb_alpha );
   VkDeviceSize size = width * height * 4;

   assert( pixels && "Error loading texture" );

   createImage( width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                _stagingImage );

   void* data;
   vkMapMemory( _device, _stagingImage.getMemory().memory, 0, size, 0, &data );
   memcpy( data, pixels, (size_t)size );
   vkUnmapMemory( _device, _stagingImage.getMemory().memory );

   stbi_image_free( pixels );

   createImage( width, height, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _textureImage );

   transitionImageLayout( _stagingImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_PREINITIALIZED,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, _device, _loadCommandPool,
                          _transferQueue.handle );
   transitionImageLayout( _textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, _device, _loadCommandPool,
                          _transferQueue.handle );
   copyImage( _stagingImage, _textureImage, width, height, _device, _loadCommandPool,
              _transferQueue.handle );
   transitionImageLayout(
      _textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, _device, _loadCommandPool, _transferQueue.handle );

   vkDeviceWaitIdle( _device );

   return true;
}

bool VulkanGraphic::createTextureImageView()
{
   createImageView( _textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT,
                    _textureImageView, _device );
   return true;
}

bool VulkanGraphic::createTextureSampler()
{
   VkSamplerCreateInfo samplerInfo = {};
   samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
   samplerInfo.magFilter = VK_FILTER_LINEAR;
   samplerInfo.minFilter = VK_FILTER_LINEAR;
   samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
   samplerInfo.anisotropyEnable = VK_TRUE;
   samplerInfo.maxAnisotropy = 16;
   samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
   samplerInfo.unnormalizedCoordinates = VK_FALSE;
   samplerInfo.compareEnable = VK_FALSE;
   samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
   samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
   samplerInfo.mipLodBias = 0.0f;
   samplerInfo.minLod = 0.0f;
   samplerInfo.maxLod = 0.0f;

   VK_CALL( vkCreateSampler( _device, &samplerInfo, nullptr, &_textureSampler ) );

   return true;
}

bool VulkanGraphic::createDepthImage()
{
   createImage( _swapChain->_curExtent.width, _swapChain->_curExtent.height, VK_FORMAT_D32_SFLOAT,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, _depthImage );
   createImageView( _depthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, _depthImageView,
                    _device );

   transitionImageLayout( _depthImage, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, _device,
                          _loadCommandPool, _transferQueue.handle );

   vkDeviceWaitIdle( _device );
   return true;
}

bool VulkanGraphic::createThreadResources()
{
	_thread.init(_physDevice, _device);
	return true;
}

void VulkanGraphic::updateUBO( const UniformBufferObject& ubo )
{
   void* data;
   vkMapMemory( _device, _uniformStagingBufferMemory.memory, _uniformStagingBufferMemory.offset,
                sizeof( ubo ), 0, &data );
   memcpy( data, &ubo, sizeof( ubo ) );
   vkUnmapMemory( _device, _uniformStagingBufferMemory.memory );

   _uboUpdateCmdBuf = copyBuffer( _uniformStagingBuffer, _uniformBuffer, sizeof( ubo ), _device,
                                  _transferCommandPools[ _curFrameIdx ], _transferQueue.handle, 0,
                                  nullptr, 1, _uboUpdatedSemaphore.get() );
}

void VulkanGraphic::onNewFrame()
{
   // Acquire the next image from the swap chain
   VkResult res =
      vkAcquireNextImageKHR( _device, _swapChain->_handle, std::numeric_limits<uint64_t>::max(),
                             _imageAvailableSemaphore, VK_NULL_HANDLE, &_curFrameIdx );
   recreateSwapChainIfNotValid( res );

   VK_CALL( vkWaitForFences( _device, 1, &_frameRenderedFence[ _curFrameIdx ], VK_FALSE, 1000 ) );
   vkResetFences( _device, 1, &_frameRenderedFence[ _curFrameIdx ] );

   _transferCommandPools[ _curFrameIdx ].freeAll( VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT );
   _graphicCommandPools[ _curFrameIdx ].freeAll( VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT );
}

void VulkanGraphic::render()
{
   const uint32_t frameIdx = _curFrameIdx;
   VkCommandBuffer commandBuffer = createCommandBuffers( frameIdx );

   VkSubmitInfo submitInfo = {};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

   std::array<VkSemaphore, 2> waitSemaphores = {{_imageAvailableSemaphore, _uboUpdatedSemaphore}};
   VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                                        VK_PIPELINE_STAGE_VERTEX_INPUT_BIT};
   submitInfo.waitSemaphoreCount = (uint32_t)waitSemaphores.size();
   submitInfo.pWaitSemaphores = waitSemaphores.data();
   submitInfo.pWaitDstStageMask = waitStages;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &commandBuffer;

   VkSemaphore renderingFinisehdSemaphore[] = {_renderFinishedSemaphore};
   submitInfo.signalSemaphoreCount = 1;
   submitInfo.pSignalSemaphores = renderingFinisehdSemaphore;

   VK_CALL(
      vkQueueSubmit( _graphicQueue.handle, 1, &submitInfo, _frameRenderedFence[ frameIdx ] ) );

   VkPresentInfoKHR presentInfo = {};
   presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

   presentInfo.waitSemaphoreCount = 1;
   presentInfo.pWaitSemaphores = renderingFinisehdSemaphore;

   VkSwapchainKHR swapChains[] = {_swapChain->_handle};
   presentInfo.swapchainCount = 1;
   presentInfo.pSwapchains = swapChains;
   presentInfo.pImageIndices = &frameIdx;
   presentInfo.pResults = nullptr;

   VkResult res = vkQueuePresentKHR( _presentationQueue.handle, &presentInfo );
   recreateSwapChainIfNotValid( res );
}

const SwapChain* VulkanGraphic::getSwapChain() const
{
   return _swapChain.get();
}

void VulkanGraphic::_debugPrintMemoryMgrInfo() const
{
   _memoryManager._debugPrint();
}

VulkanGraphic::~VulkanGraphic()
{
   vkDeviceWaitIdle( _device );

   // Free the frame fences
   for ( auto& f : _frameRenderedFence )
   {
      vkDestroyFence( _device, f, nullptr );
   }
}
