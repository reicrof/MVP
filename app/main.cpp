#if defined( WIN32 ) || defined( _WIN32 )
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "utils.h"
#include "vulkanGraphic.h"

#include <glm/gtc/matrix_transform.hpp>

#include <assert.h>
#include <iostream>
#include <string>
#include <chrono>

#ifndef RTLD_LAZY
#define RTLD_LAZY 1
#endif
#ifndef RTLD_NOW
#define RTLD_NOW 2
#endif
#ifndef RTLD_GLOBAL
#define RTLD_GLOBAL 4
#endif

using namespace std::chrono_literals;

static constexpr int WINDOW_TITLE_SIZE = 256;

void* loadSharedLibrary( const char* libNameNoExt, int iMode = 2 )
{
   std::string sDllName = libNameNoExt;
#if defined( WIN32 ) || defined( _WIN32 )
   sDllName += ".dll";
   return (void*)LoadLibrary( sDllName.c_str() );
#else
   sDllName += ".so";
   return dlopen( sDllName.c_str(), iMode );
#endif
}

void* getFunction( void* lib, const char* funcName )
{
#if defined( WIN32 ) || defined( _WIN32 )
   return (void*)GetProcAddress( (HINSTANCE)lib, funcName );
#else
   return dlsym( lib, funcName );
#endif
}

bool freeSharedLibrary( void* handle )
{
#if defined( WIN32 ) || defined( _WIN32 )
   return FreeLibrary( (HINSTANCE)handle ) == TRUE;
#else
   return dlclose( handle );
#endif
}

#include "../core/core.h"

const char* coreDllName = "../core/core.dll";
const char* tempCoreDllName = "../core/coreRunning.dll";
#if defined( _WIN32 )
FILETIME lastLoadedCoreDllWriteTime;
#endif
void* coreDll = nullptr;
typedef int ( *getOneFnPtr )();
getOneFnPtr ptr = nullptr;

#if defined( WIN32 ) || defined( _WIN32 )
FILETIME
getFileTimeFromFile( const char* fileName )
{
   WIN32_FIND_DATA findFileData;
   FindFirstFile( fileName, &findFileData );
   return findFileData.ftLastWriteTime;
}
#else
#endif

static void loadCoreFunctions()
{
#if defined( WIN32 ) || defined( _WIN32 )
   if ( coreDll )
   {
      freeSharedLibrary( coreDll );
   }

   CopyFile( coreDllName, tempCoreDllName, FALSE );
   coreDll = loadSharedLibrary( "../core/coreRunning" );
   lastLoadedCoreDllWriteTime = getFileTimeFromFile( coreDllName );
   // coreDll = loadSharedLibrary("../../core/core");
   VERIFY( coreDll != nullptr, "Cannot load core lib" );

   ptr = (getOneFnPtr)getFunction( coreDll, "getOne" );
   VERIFY( ptr, "cannot get function" );
#endif
}

bool shouldReloadCoreLib()
{
#if defined( WIN32 ) || defined( _WIN32 )
   const auto fileTime = getFileTimeFromFile( coreDllName );
   return CompareFileTime( &fileTime, &lastLoadedCoreDllWriteTime ) == 1;
#else
   return false;
#endif
}

static void keyCB( GLFWwindow* window, int key, int scancode, int action, int mods )
{
   if ( key == GLFW_KEY_ESCAPE && action == GLFW_PRESS )
   {
      glfwSetWindowShouldClose( window, GLFW_TRUE );
   }
}

static void onWindowResized( GLFWwindow* window, int width, int height )
{
   if ( width > 0 && height > 0 )
   {
      VulkanGraphic* VK = reinterpret_cast<VulkanGraphic*>( glfwGetWindowUserPointer( window ) );
      VK->recreateSwapChain();
   }
}

void updateCoreDll()
{
   if ( shouldReloadCoreLib() )
   {
      loadCoreFunctions();
   }
}

#include "vertex.h"
const std::vector<Vertex> vertices = {{{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                      {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                      {{0.5f, 0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                      {{-0.5f, 0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},

                                      {{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
                                      {{0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
                                      {{0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
                                      {{-0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};
const std::vector<uint32_t> indices = {0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4};

static void initVulkan( VulkanGraphic& VK, GLFWwindow* window )
{
   VERIFY( VK.createSurface( window ), "Cannot create vulkan surface." );
   VERIFY( VK.getPysicalDevices(), "Cannot get physical device." );
   VERIFY( VK.createLogicalDevice(), "Cannot create logical evice." );
   VERIFY( VK.createSwapChain(), "Cannot create swap chain." );
   VERIFY( VK.createMemoryPool(), "Cannot create buffer memory pool." );
   VERIFY( VK.createRenderPass(), "Cannot create a render pass." );
   VERIFY( VK.createDescriptorSetLayout(), "Cannot create descriptor set layout" );
   VERIFY( VK.createPipeline(), "Cannot create the pipeline." );
   VERIFY( VK.createCommandPool(), "Cannot create command pool." );
   VERIFY( VK.createTextureImage(), "Cannot create texture" );
   VERIFY( VK.createTextureImageView(), "Cannot create texture view" );
   VERIFY( VK.createTextureSampler(), "Cannot create texture sampler" );
   VERIFY( VK.createDepthImage(), "Cannot create depth image." );
   VERIFY( VK.createFrameBuffers(), "Cannot create frame buffer." );
   VERIFY( VK.createVertexBuffer( vertices ), "Cannot create vertex buffer." );
   VERIFY( VK.createIndexBuffer( indices ), "Cannot create index buffer." );
   VERIFY( VK.createUniformBuffer(), "Cannot create uniform buffer." );
   VERIFY( VK.createDescriptorPool(), "Cannot create descriptor pool." );
   VERIFY( VK.createDescriptorSet(), "Cannot create descriptor set." );
   VERIFY( VK.createCommandBuffers(), "Cannot create frame buffer." );
   VERIFY( VK.createSemaphores(), "Cannot create semaphores." );
}

void updateUBO( UniformBufferObject& ubo, const SwapChain* swapChain )
{
   static auto startTime = std::chrono::high_resolution_clock::now();

   auto currentTime = std::chrono::high_resolution_clock::now();
   float time =
      std::chrono::duration_cast<std::chrono::milliseconds>( currentTime - startTime ).count() /
      1000.0f;

   ubo.model =
      glm::rotate( glm::mat4(), time * glm::radians( 90.0f ), glm::vec3( 0.0f, 0.0f, 1.0f ) );
   ubo.view = glm::lookAt( glm::vec3( 2.0f, 2.0f, 2.0f ), glm::vec3( 0.0f, 0.0f, 0.0f ),
                           glm::vec3( 0.0f, 0.0f, 1.0f ) );
   ubo.proj = glm::perspective( glm::radians( 45.0f ),
                                swapChain->_curExtent.width / (float)swapChain->_curExtent.height,
                                0.1f, 10.0f );
}

int main()
{
   VERIFY( glfwInit(), "Cannot init glfw." );
   VERIFY( glfwVulkanSupported(), "Vulkan not supported." );

   loadCoreFunctions();

   glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
   GLFWwindow* window = glfwCreateWindow( 800, 600, "MVP", nullptr, nullptr );

   VERIFY( window, "Could not create GLFW window." );

   unsigned int glfwExtensionCount = 0;
   const char** glfwExtensions;
   glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );
   std::vector<const char*> extensions( glfwExtensionCount );
   std::copy( glfwExtensions, glfwExtensions + glfwExtensionCount, extensions.begin() );

   VulkanGraphic VK( extensions );
   initVulkan( VK, window );

   // Setup callback function
   glfwSetKeyCallback( window, keyCB );
   glfwSetWindowUserPointer( window, &VK );  // Set user data
   glfwSetWindowSizeCallback( window, onWindowResized );

   UniformBufferObject ubo = {};

   char windowTitle[ WINDOW_TITLE_SIZE ] = {};
   auto simStartTime = std::chrono::steady_clock::now();
   auto nextFpsPrintTime = 1s;
   unsigned frameRendered = 0;

   while ( !glfwWindowShouldClose( window ) )
   {
      updateCoreDll();
      glfwPollEvents();
      updateUBO( ubo, VK.getSwapChain() );
      VK.updateUBO( ubo );
      // std::cout << ptr() << std::endl;
      VK.render();

      ++frameRendered;
      if ( std::chrono::steady_clock::now() - simStartTime > nextFpsPrintTime )
      {
         std::snprintf( windowTitle, WINDOW_TITLE_SIZE, "MVP - %i FPS", frameRendered );
         glfwSetWindowTitle( window, windowTitle );
         frameRendered = 0;
         nextFpsPrintTime += 1s;
      }
   }

   glfwDestroyWindow( window );
   glfwTerminate();
}