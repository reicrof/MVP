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

void transitionImageLayout(VkImage image,
	VkFormat format,
	VkImageLayout oldLayout,
	VkImageLayout newLayout,
	VkImageSubresourceRange subresource,
	VDeleter<VkDevice>& device,
	VCommandPool& commandPool,
	VkQueue& queue)
{
	VkCommandBuffer commandBuffer = commandPool.alloc(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	// Queue family ownership
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;
	barrier.subresourceRange = subresource;

	barrier.srcAccessMask = vkImageLayoutToAccessFlags(oldLayout);
	barrier.dstAccessMask = vkImageLayoutToAccessFlags(newLayout);

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
		&barrier);

	endSingleTimeCommands(commandBuffer, device, queue, commandPool);
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
   const float queuePriority[] = {1.0f, 1.0f};
   size_t count = 0;
   for ( auto i = uniqueFamilliesIndex.begin(); i != uniqueFamilliesIndex.end(); ++i )
   {
      queueCreateInfos[ count ].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
      queueCreateInfos[ count ].queueFamilyIndex = *i;
      queueCreateInfos[ count ].queueCount = *i == 0 ? 2 : 1;
      queueCreateInfos[ count ].pQueuePriorities = &queuePriority[ 0 ];
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
   vkGetDeviceQueue( _device, _graphicQueue.familyIndex, 1, &r._graphicQueue );

   _memoryManager.init( _physDevice, _device );
   _thread.init( _physDevice, _device );

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

bool VulkanGraphic::createDescriptorSetLayout()
{
    std::vector< VkDescriptorSetLayoutBinding > bindings = 
    {
        // binding, descriptorType, descriptor count, stage flags, immutable samplers
        { 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,  VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,  VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,  VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
        { 3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,  VK_SHADER_STAGE_FRAGMENT_BIT, nullptr },
    };
    _descriptorSetLayout.set( VkUtils::createDescriptorSetLayout(_device, bindings) );

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
   vertShaderModule.set( VkUtils::createShaderModule( _device, "../shaders/vert.spv" ) );
   fragShaderModule.set( VkUtils::createShaderModule( _device, "../shaders/frag.spv" ) );

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
   widgetvertShaderModule.set( VkUtils::createShaderModule( _device, "../shaders/widgetVert.spv" ) );
   widgetfragShaderModule.set( VkUtils::createShaderModule( _device, "../shaders/widgetFrag.spv" ) );

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


   // Create skybox pipeline

   VDeleter<VkShaderModule> skyboxvertShaderModule{ _device, vkDestroyShaderModule };
   VDeleter<VkShaderModule> skyboxfragShaderModule{ _device, vkDestroyShaderModule };
   skyboxvertShaderModule.set(VkUtils::createShaderModule(_device, "../shaders/skyboxVert.spv"));
   skyboxfragShaderModule.set(VkUtils::createShaderModule(_device, "../shaders/skyboxFrag.spv"));

   // Vertex stage
   VkPipelineShaderStageCreateInfo skyboxvShaderStageCreateInfo = {};
   skyboxvShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   skyboxvShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
   skyboxvShaderStageCreateInfo.module = skyboxvertShaderModule;
   skyboxvShaderStageCreateInfo.pName = "main";

   // Fragment stage
   VkPipelineShaderStageCreateInfo skyboxfShaderStageInfo = {};
   skyboxfShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
   skyboxfShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
   skyboxfShaderStageInfo.module = skyboxfragShaderModule;
   skyboxfShaderStageInfo.pName = "main";

   VkPipelineShaderStageCreateInfo skyboxStages[] = { skyboxvShaderStageCreateInfo,
       skyboxfShaderStageInfo };

   pipelineInfo.pStages = skyboxStages;

   depthStencil.depthTestEnable = VK_TRUE;
   pipelineInfo.pDepthStencilState = &depthStencil;

   VK_CALL(vkCreateGraphicsPipelines(_device, _pipelineCache, 1, &pipelineInfo, nullptr,
       &_skyboxPipeline));

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
   poolSizes[ 1 ].descriptorCount = 3;

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
   bufferInfo.range = sizeof( UniformBufferObject ) + sizeof( PBRMaterial );

   VkDescriptorImageInfo imageInfo = {};
   imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   imageInfo.imageView = _textureImageView;
   imageInfo.sampler = _textureSampler;

   VkDescriptorImageInfo irradianceImageInfo = {};
   irradianceImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   irradianceImageInfo.imageView = _irradianceTexture._imageView;
   irradianceImageInfo.sampler = _irradianceTexture._sampler;

   VkDescriptorImageInfo radianceImageInfo = {};
   radianceImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
   radianceImageInfo.imageView = _radianceTexture._imageView;
   radianceImageInfo.sampler = _radianceTexture._sampler;

   std::array<VkWriteDescriptorSet, 4> descriptorWrites =
   {
    VkUtils::createWriteDescriptorSet(_descriptorSet, 0, 0, &bufferInfo, 1),
    VkUtils::createWriteDescriptorSet(_descriptorSet, 1, 0, &imageInfo, 1),
    VkUtils::createWriteDescriptorSet(_descriptorSet, 2, 0, &irradianceImageInfo, 1),
    VkUtils::createWriteDescriptorSet(_descriptorSet, 3, 0, &radianceImageInfo, 1)
   };

   vkUpdateDescriptorSets( _device, static_cast<uint32_t>( descriptorWrites.size() ),
                           descriptorWrites.data(), 0, nullptr );

   return true;
}

void VulkanGraphic::freeBuffer( VMemAlloc& alloc )
{
   _memoryManager.free( alloc );
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

   for ( size_t i = 0; i < _geomsToDraw; ++i )
   {
      VkBuffer vertexBuffers[] = {_geoms[ i ]._vertexBuffer};
      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers( commandBuffer, 0, 1, vertexBuffers, offsets );
      vkCmdBindIndexBuffer( commandBuffer, _geoms[ i ]._indexBuffer, 0, VK_INDEX_TYPE_UINT32 );
      vkCmdDrawIndexed( commandBuffer, _geoms[ i ]._indexCount, 1, 0, 0, 0 );
   }

   //vkCmdBindPipeline( commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsWidgetPipeline );

   //for ( size_t i = 0; i < _geomsToDraw; ++i )
   //{
   //   VkBuffer vertexBuffers[] = {_geoms[ i ]._vertexBuffer};
   //   VkDeviceSize offsets[] = {0};
   //   vkCmdBindVertexBuffers( commandBuffer, 0, 1, vertexBuffers, offsets );
   //   vkCmdBindIndexBuffer( commandBuffer, _geoms[ i ]._indexBuffer, 0, VK_INDEX_TYPE_UINT32 );
   //   vkCmdDrawIndexed( commandBuffer, _geoms[ i ]._indexCount, 1, 0, 0, 0 );
   //}


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

static auto addGeomImp( VThread::VThreadResources* resources,
                        const std::vector<Vertex>& vertices,
                        const std::vector<uint32_t>& indices,
                        VGeom* geom,
                        std::atomic_uint32_t* geomCount )
{
   // Create fence for syncronization
   VDeleter<VkFence> fence{resources->_device, vkDestroyFence};
   VkFenceCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   createInfo.pNext = nullptr;
   createInfo.flags = 0;
   vkCreateFence( resources->_device, &createInfo, nullptr, &fence );

   geom->_verticesCount = static_cast<uint32_t>( vertices.size() );
   geom->_indexCount = static_cast<uint32_t>( indices.size() );

   const size_t vertexBufferSize = sizeof( ( vertices[ 0 ] ) ) * vertices.size();
   const size_t indexBufferSize = indices.size() * sizeof( uint32_t );
   const size_t combinedSize = vertexBufferSize + indexBufferSize;

   // Allocate staging buffer for both vertices and indices
   VDeleter<VkBuffer> stagingBuffer{resources->_device, vkDestroyBuffer};
   VDeleter<VkDeviceMemory> stagingBufferMemory{resources->_device, vkFreeMemory};
   VMemAlloc hostBuffer = VkUtils::createBuffer(
      resources->_device, resources->_memoryManager,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, combinedSize,
      VK_BUFFER_USAGE_TRANSFER_SRC_BIT, *stagingBuffer.get() );

   // Copy vertices to it
   void* data;
   VK_CALL( vkMapMemory( resources->_device, hostBuffer.memory, hostBuffer.offset, combinedSize, 0,
                         &data ) );
   memcpy( data, vertices.data(), vertexBufferSize );
   memcpy( static_cast<char*>( data ) + vertexBufferSize, indices.data(), indexBufferSize );
   vkUnmapMemory( resources->_device, hostBuffer.memory );

   VkUtils::createBuffer( resources->_device, resources->_memoryManager,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBufferSize,
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                        geom->_vertexBuffer );
   VkUtils::createBuffer( resources->_device, resources->_memoryManager,
                        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBufferSize,
                        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                        geom->_indexBuffer );


   VkCommandBuffer commandBuffer =
      resources->_graphicCommandPool.alloc( VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT );

   VkBufferCopy copyVertex = {};
   copyVertex.size = vertexBufferSize;
   vkCmdCopyBuffer( commandBuffer, stagingBuffer, geom->_vertexBuffer, 1, &copyVertex );

   VkBufferCopy copyIndices = {};
   copyIndices.size = indexBufferSize;
   copyIndices.srcOffset = vertexBufferSize;
   vkCmdCopyBuffer( commandBuffer, stagingBuffer, geom->_indexBuffer, 1, &copyIndices );

   endSingleTimeCommands( commandBuffer, resources->_device, resources->_graphicQueue,
                          resources->_graphicCommandPool, 0, nullptr, 0, nullptr, fence );

   // Wait for the transfer to finish
   VK_CALL( vkWaitForFences( resources->_device, 1, fence.get(), VK_FALSE,
                             std::numeric_limits<uint64_t>::max() ) );

   ++( *geomCount );
   return true;
}


std::future<bool> VulkanGraphic::addGeom( const std::vector<Vertex>& vertices,
                                          const std::vector<uint32_t>& indices )
{
   return _thread.addJob( addGeomImp, vertices, indices, &_geoms[ _geomsToDraw ], &_geomsToDraw );
}

bool VulkanGraphic::createUniformBuffer()
{
   VkDeviceSize bufferSize = sizeof( UniformBufferObject ) + sizeof( PBRMaterial );

   _uniformStagingBufferMemory = VkUtils::createBuffer(_device, _memoryManager,
       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, *_uniformStagingBuffer.get());

   VkUtils::createBuffer( _device, _memoryManager, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, *_uniformBuffer.get() );

   return true;
}

bool VulkanGraphic::createTextureImage()
{
   int width, height, channels;

   stbi_uc* pixels =
      stbi_load( "../textures/chalet.jpg", &width, &height, &channels, STBI_rgb_alpha );
   VkDeviceSize size = width * height * 4;

   assert( pixels && "Error loading texture" );

   VkUtils::createImage( _device, _memoryManager, width, height, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_LINEAR,
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                _stagingImage );

   void* data;
   vkMapMemory( _device, _stagingImage.getMemory().memory, 0, size, 0, &data );
   memcpy( data, pixels, (size_t)size );
   vkUnmapMemory( _device, _stagingImage.getMemory().memory );

   stbi_image_free( pixels );

   VkUtils::createImage(_device, _memoryManager, width, height, 1, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
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
    VkUtils::createImage(_device, _memoryManager, _swapChain->_curExtent.width, _swapChain->_curExtent.height, 1, VK_FORMAT_D32_SFLOAT,
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

bool VulkanGraphic::createIBLTexture()
{
	//createCubeMap("../textures/irradiance.ktx", _irradianceTexture, _irradianceImageView, _irradianceSampler);
	//createCubeMap("../textures/radiance.ktx", _radianceTexture, _radianceImageView, _radianceSampler);
   VImage::loadCubeTexture("../textures/radiance.ktx", _radianceTexture, _device, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, _memoryManager, _loadCommandPool,
      _transferQueue.handle);

	VImage::loadCubeTexture("../textures/irradiance.ktx", _irradianceTexture, _device, VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, _memoryManager, _loadCommandPool,
		_transferQueue.handle);

	return true;
}

bool VulkanGraphic::createCubeMap( const std::string& path, VImage& img, VDeleter<VkImageView>& imgView, VDeleter<VkSampler>& imgSampler)
{
	VDeleter<VkBuffer> stagingBuffer{ _device, vkDestroyBuffer };
	gli::texture_cube tex(gli::load(path) );

	img._width = tex.extent().x;
	img._height = tex.extent().y;
	img._mips = tex.levels();
	img._format = VK_FORMAT_R16G16B16A16_SFLOAT;
	img._size = tex.size();

	VMemAlloc hostBuffer = VkUtils::createBuffer( _device, _memoryManager, 
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, tex.size(), 
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT, *stagingBuffer.get() );

	void* data;
	VK_CALL( vkMapMemory(_device, hostBuffer.memory, hostBuffer.offset, tex.size(), 0, &data) );
	memcpy(data, tex.data(), (size_t)tex.size());
	vkUnmapMemory(_device, hostBuffer.memory);

	std::vector<VkBufferImageCopy> bufferCopyRegions;
	size_t offset = 0;

	for (uint32_t face = 0; face < 6; ++face)
	{
		for (uint32_t level = 0; level < tex.levels(); ++level)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = level;
			bufferCopyRegion.imageSubresource.baseArrayLayer = face;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(tex[face][level].extent().x);
			bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(tex[face][level].extent().y);
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);

			// Increase offset into staging buffer for next level / face
			offset += tex[face][level].size();
		}
	}

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	imageCreateInfo.mipLevels = (uint32_t)tex.levels();
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent = { (uint32_t) tex.extent().x, (uint32_t)tex.extent().y, 1 };
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	// Ensure that the TRANSFER_DST bit is set for staging
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	// Cube faces count as array layers in Vulkan
	imageCreateInfo.arrayLayers = 6;
	// This flag is required for cube map images
	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VK_CALL( vkCreateImage(_device, &imageCreateInfo, nullptr, &img) );

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(_device, img, &memRequirements);

	img.setMemory(_memoryManager.alloc(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

	VK_CALL(
		vkBindImageMemory(_device, img, img.getMemory().memory, img.getMemory().offset));

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = (uint32_t)tex.levels();
	subresourceRange.layerCount = 6;

	transitionImageLayout(img, img._format, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange, _device,
		_loadCommandPool, _transferQueue.handle);

	VkCommandBuffer commandBuffer = _loadCommandPool.alloc(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

	endSingleTimeCommands(commandBuffer, _device, _transferQueue.handle, _loadCommandPool);

	transitionImageLayout(img, img._format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, subresourceRange, _device,
		_loadCommandPool, _transferQueue.handle);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
	samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxAnisotropy = 8;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = (float)tex.levels();
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CALL(vkCreateSampler(_device, &samplerCreateInfo, nullptr, &imgSampler));

	// Create image view
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewCreateInfo.format = img._format;
	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	viewCreateInfo.subresourceRange.layerCount = 6;
	viewCreateInfo.subresourceRange.levelCount = (uint32_t) tex.levels();
	viewCreateInfo.image = img;
	VK_CALL(vkCreateImageView(_device, &viewCreateInfo, nullptr, &imgView));

	vkDeviceWaitIdle(_device);

	return true;
}

void VulkanGraphic::updateUBO( const UniformBufferObject& ubo, const PBRMaterial& pbr )
{
   void* data;
   vkMapMemory( _device, _uniformStagingBufferMemory.memory, _uniformStagingBufferMemory.offset,
                sizeof( ubo ) + sizeof( pbr ), 0, &data );
   memcpy( data, &ubo, sizeof( ubo ));
   memcpy((char*)data + sizeof(ubo), &pbr, sizeof(pbr));
   vkUnmapMemory( _device, _uniformStagingBufferMemory.memory );

   _uboUpdateCmdBuf = copyBuffer( _uniformStagingBuffer, _uniformBuffer, sizeof( ubo ) + sizeof(pbr), _device,
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

   VK_CALL( vkWaitForFences( _device, 1, &_frameRenderedFence[ _curFrameIdx ], VK_FALSE, 100000000000 ) );
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

   for ( size_t i = 0; i < _geomsToDraw; ++i )
   {
      vkDestroyBuffer( _device, _geoms[ i ]._vertexBuffer, nullptr );
      vkDestroyBuffer( _device, _geoms[ i ]._indexBuffer, nullptr );
   }

   // Free the frame fences
   for ( auto& f : _frameRenderedFence )
   {
      vkDestroyFence( _device, f, nullptr );
   }
}
