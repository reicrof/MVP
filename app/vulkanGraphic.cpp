#include "vulkanGraphic.h"
#include "utils.h"
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

const std::vector<const char*> VALIDATION_LAYERS = {"VK_LAYER_LUNARG_standard_validation"};

const std::vector<const char*> DEVICE_EXTENSIONS = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

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
                         std::ofstream& outErrorFile )
{
   if ( !enableValidationLayers )
      return;

   VkDebugReportCallbackCreateInfoEXT createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
   createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
   createInfo.pfnCallback = debugCallback;
   createInfo.pUserData = &outErrorFile;

   VK_CALL( CreateDebugReportCallbackEXT( instance, &createInfo, nullptr, &callback ) );
   outErrorFile = std::ofstream( "VulkanErrors.txt" );
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
         if ( strcmp( layerName, layerProperties.layerName ) == 0 )
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

uint32_t findMemoryType( const VkPhysicalDevice& physDevice,
                         uint32_t typeFilter,
                         VkMemoryPropertyFlags properties )
{
   VkPhysicalDeviceMemoryProperties memProperties;
   vkGetPhysicalDeviceMemoryProperties( physDevice, &memProperties );
   for ( uint32_t i = 0; i < memProperties.memoryTypeCount; ++i )
   {
      if ( ( typeFilter & ( 1 << i ) ) &&
           ( memProperties.memoryTypes[ i ].propertyFlags & properties ) == properties )
      {
         return i;
      }
   }
   return -1;
}
}  // End of anonymous namespace

void VulkanGraphic::recreateSwapChain()
{
   vkDeviceWaitIdle( _device );
   _swapChain.reset( new SwapChain( _physDevice, _device, _surface, VK_SHARING_MODE_EXCLUSIVE,
                                    _swapChain->_handle ) );
   createPipeline();
   createFrameBuffers();
   createCommandBuffers();
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
   std::set<int> uniqueFamilliesIndex = {_graphicQueue.familyIndex, _presentationQueue.familyIndex};
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

   VkRenderPassCreateInfo renderPassInfo = {};
   renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
   renderPassInfo.attachmentCount = 1;
   renderPassInfo.pAttachments = &colorAttachment;
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
   rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
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

   VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
   pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
   pipelineLayoutInfo.setLayoutCount = 0;
   pipelineLayoutInfo.pushConstantRangeCount = 0;

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
   pipelineInfo.pDepthStencilState = nullptr;  // Optional
   pipelineInfo.pColorBlendState = &colorBlending;
   pipelineInfo.pDynamicState = &dynamicStateCreateInfo;
   pipelineInfo.layout = _pipelineLayout;
   pipelineInfo.renderPass = _renderPass;
   pipelineInfo.subpass = 0;
   pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
   pipelineInfo.basePipelineIndex = -1;  // Optional

   VK_CALL( vkCreateGraphicsPipelines( _device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                       &_graphicsPipeline ) );

   return true;
}

bool VulkanGraphic::createFrameBuffers()
{
   _framebuffers.resize( _swapChain->_imageCount,
                         VDeleter<VkFramebuffer>{_device, vkDestroyFramebuffer} );
   for ( uint32_t i = 0; i < _swapChain->_imageCount; ++i )
   {
      VkImageView attachments[] = {_swapChain->_imageViews[ i ]};

      VkFramebufferCreateInfo framebufferInfo = {};
      framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebufferInfo.renderPass = _renderPass;
      framebufferInfo.attachmentCount = 1;
      framebufferInfo.pAttachments = attachments;
      framebufferInfo.width = _swapChain->_curExtent.width;
      framebufferInfo.height = _swapChain->_curExtent.height;
      framebufferInfo.layers = 1;

      VK_CALL( vkCreateFramebuffer( _device, &framebufferInfo, nullptr, &_framebuffers[ i ] ) );
   }

   return true;
}

bool VulkanGraphic::createCommandPool()
{
   VkCommandPoolCreateInfo poolInfo = {};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.queueFamilyIndex = _graphicQueue.familyIndex;
   poolInfo.flags =
      0;  // VK_COMMAND_POOL_CREATE_TRANSIENT_BIT or VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT

   VK_CALL( vkCreateCommandPool( _device, &poolInfo, nullptr, &_commandPool ) );

   return true;
}

bool VulkanGraphic::createCommandBuffers()
{
   if ( _commandBuffers.size() > 0 )
   {
      vkFreeCommandBuffers( _device, _commandPool, static_cast<uint32_t>( _commandBuffers.size() ),
                            _commandBuffers.data() );
   }

   _commandBuffers.resize( _swapChain->_imageCount );

   VkCommandBufferAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.commandPool = _commandPool;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandBufferCount = _swapChain->_imageCount;

   VK_CALL( vkAllocateCommandBuffers( _device, &allocInfo, _commandBuffers.data() ) );

   for ( size_t i = 0; i < _commandBuffers.size(); i++ )
   {
      VkCommandBufferBeginInfo beginInfo = {};
      beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
      beginInfo.pInheritanceInfo = nullptr;

      vkBeginCommandBuffer( _commandBuffers[ i ], &beginInfo );

      VkRenderPassBeginInfo renderPassInfo = {};
      renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
      renderPassInfo.renderPass = _renderPass;
      renderPassInfo.framebuffer = _framebuffers[ i ];
      renderPassInfo.renderArea.offset = {0, 0};
      renderPassInfo.renderArea.extent = _swapChain->_curExtent;

      VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
      renderPassInfo.clearValueCount = 1;
      renderPassInfo.pClearValues = &clearColor;

      vkCmdBeginRenderPass( _commandBuffers[ i ], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE );

      vkCmdBindPipeline( _commandBuffers[ i ], VK_PIPELINE_BIND_POINT_GRAPHICS, _graphicsPipeline );

      VkViewport viewport = {0.0f,
                             0.0f,
                             static_cast<float>( _swapChain->_curExtent.width ),
                             static_cast<float>( _swapChain->_curExtent.height ),
                             0.0f,
                             1.0f};

      VkRect2D scissor = {{0, 0}, {_swapChain->_curExtent.width, _swapChain->_curExtent.height}};

      vkCmdSetViewport( _commandBuffers[ i ], 0, 1, &viewport );
      vkCmdSetScissor( _commandBuffers[ i ], 0, 1, &scissor );

      VkBuffer vertexBuffers[] = {_vertexBuffer};
      VkDeviceSize offsets[] = {0};
      vkCmdBindVertexBuffers( _commandBuffers[ i ], 0, 1, vertexBuffers, offsets );

      vkCmdDraw( _commandBuffers[ i ], _verticesCount, 1, 0, 0 );

      vkCmdEndRenderPass( _commandBuffers[ i ] );

      VK_CALL( vkEndCommandBuffer( _commandBuffers[ i ] ) );
   }

   return true;
}

bool VulkanGraphic::createSemaphores()
{
   VkSemaphoreCreateInfo semaphoreInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
   VK_CALL( vkCreateSemaphore( _device, &semaphoreInfo, nullptr, &_imageAvailableSemaphore ) );
   VK_CALL( vkCreateSemaphore( _device, &semaphoreInfo, nullptr, &_renderFinishedSemaphore ) );
   return true;
}

bool VulkanGraphic::createVertexBuffer( const std::vector<Vertex>& vertices )
{
   VkBufferCreateInfo createInfo = {};
   createInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   createInfo.size = sizeof( ( vertices[ 0 ] ) ) * vertices.size();
   createInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
   createInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VK_CALL( vkCreateBuffer( _device, &createInfo, nullptr, &_vertexBuffer ) );

   VkMemoryRequirements memRequirements;
   vkGetBufferMemoryRequirements( _device, _vertexBuffer, &memRequirements );

   VkMemoryAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = memRequirements.size;
   allocInfo.memoryTypeIndex =
      findMemoryType( _physDevice, memRequirements.memoryTypeBits,
                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT );

   VK_CALL( vkAllocateMemory( _device, &allocInfo, nullptr, &_vertexBufferMemory ) );

   // then we can now associate this memory with the buffer using
   vkBindBufferMemory( _device, _vertexBuffer, _vertexBufferMemory, 0 );

   void* data;
   vkMapMemory( _device, _vertexBufferMemory, 0, createInfo.size, 0, &data );
   memcpy( data, vertices.data(), (size_t)createInfo.size );
   vkUnmapMemory( _device, _vertexBufferMemory );

   _verticesCount = static_cast<uint32_t>( vertices.size() );

   return true;
}

void VulkanGraphic::render()
{
   // Acquire the next image from the swap chain
   uint32_t imageIndex;
   VkResult res =
      vkAcquireNextImageKHR( _device, _swapChain->_handle, std::numeric_limits<uint64_t>::max(),
                             _imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex );
   recreateSwapChainIfNotValid( res );

   VkSubmitInfo submitInfo = {};
   submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

   VkSemaphore waitSemaphores[] = {_imageAvailableSemaphore};
   VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
   submitInfo.waitSemaphoreCount = 1;
   submitInfo.pWaitSemaphores = waitSemaphores;
   submitInfo.pWaitDstStageMask = waitStages;
   submitInfo.commandBufferCount = 1;
   submitInfo.pCommandBuffers = &_commandBuffers[ imageIndex ];

   VkSemaphore renderingFinisehdSemaphore[] = {_renderFinishedSemaphore};
   submitInfo.signalSemaphoreCount = 1;
   submitInfo.pSignalSemaphores = renderingFinisehdSemaphore;

   VK_CALL( vkQueueSubmit( _graphicQueue.handle, 1, &submitInfo, VK_NULL_HANDLE ) );

   VkPresentInfoKHR presentInfo = {};
   presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

   presentInfo.waitSemaphoreCount = 1;
   presentInfo.pWaitSemaphores = renderingFinisehdSemaphore;

   VkSwapchainKHR swapChains[] = {_swapChain->_handle};
   presentInfo.swapchainCount = 1;
   presentInfo.pSwapchains = swapChains;
   presentInfo.pImageIndices = &imageIndex;
   presentInfo.pResults = nullptr;

   res = vkQueuePresentKHR( _presentationQueue.handle, &presentInfo );
   recreateSwapChainIfNotValid( res );
}

VulkanGraphic::~VulkanGraphic()
{
   vkDeviceWaitIdle( _device );
}