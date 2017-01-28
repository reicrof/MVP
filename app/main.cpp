#if defined( WIN32 ) || defined( _WIN32 )
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "utils.h"
#include "vulkanGraphic.h"
#include "Camera.h"

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

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

static bool loadModelImp( const std::string& path,
                          std::vector<Vertex>* vertices,
                          std::vector<uint32_t>* indices )
{
   tinyobj::attrib_t attrib;
   std::vector<tinyobj::shape_t> shapes;
   std::vector<tinyobj::material_t> materials;
   std::string err;

   bool success = tinyobj::LoadObj( &attrib, &shapes, &materials, &err, "../models/armadillo.obj" );
   if ( success )
   {
      vertices->reserve( attrib.vertices.size() / 3 );
      indices->reserve( attrib.vertices.size() );
      for ( const auto& shape : shapes )
      {
         for ( const auto& index : shape.mesh.indices )
         {
            Vertex vertex = {};
            vertex.pos = {attrib.vertices[ 3 * index.vertex_index + 0 ],
                          attrib.vertices[ 3 * index.vertex_index + 1 ],
                          attrib.vertices[ 3 * index.vertex_index + 2 ]};
            vertex.normal = {attrib.normals[ 3 * index.vertex_index + 0 ],
                             attrib.normals[ 3 * index.vertex_index + 1 ],
                             attrib.normals[ 3 * index.vertex_index + 2 ]};

            if ( attrib.texcoords.size() > 0 )
            {
               vertex.texCoord = {attrib.texcoords[ 2 * index.texcoord_index + 0 ],
                                  1.0f - attrib.texcoords[ 2 * index.texcoord_index + 1 ]};
            }

            vertices->push_back( vertex );
            indices->push_back( (uint32_t)indices->size() );
         }
      }
      return true;
   }
   else
   {
      std::cerr << err << std::endl;
   }
   return false;
}

#include "ThreadPool.h"
static auto loadModel( ThreadPool& jobPool,
                       const std::string& path,
                       std::vector<Vertex>* vertices,
                       std::vector<uint32_t>* indices )
{
   return jobPool.addJob( loadModelImp, path, vertices, indices );
}

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
   VERIFY( VK.createUniformBuffer(), "Cannot create uniform buffer." );
   VERIFY( VK.createDescriptorPool(), "Cannot create descriptor pool." );
   VERIFY( VK.createDescriptorSet(), "Cannot create descriptor set." );
   VERIFY( VK.createCommandBuffers(), "Cannot create frame buffer." );
   VERIFY( VK.createSemaphores(), "Cannot create semaphores." );
}

void updateUBO( const Camera& cam, UniformBufferObject& ubo )
{
   static auto startTime = std::chrono::high_resolution_clock::now();

   auto currentTime = std::chrono::high_resolution_clock::now();
   float time =
      std::chrono::duration_cast<std::chrono::milliseconds>( currentTime - startTime ).count() /
      10000.0f;

   ubo.model =
      glm::rotate( glm::mat4(), time * glm::radians( 90.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) );

   ubo.view = cam.getView();
   ubo.proj = cam.getProj();
}

#define GLM_ENABLE_EXPERIMENTAL
#include "glm/gtx/euler_angles.hpp"
Camera cam( 45.0f, 1920, 1080, 0.1f, 20 );
void onMousePos( GLFWwindow* window, double x, double y )
{
   static bool isPressed = false;
   static double onPressX = 0;
   static double onPressY = 0;
   static glm::quat startOri;
   if ( glfwGetMouseButton( window, GLFW_MOUSE_BUTTON_1 ) == GLFW_PRESS )
   {
      if ( isPressed == false )
      {
         isPressed = true;
         onPressX = x;
         onPressY = y;
         startOri = cam.getOrientation();
      }

      float deltaX = (float)( x - onPressX );
      float deltaY = (float)( y - onPressY );

      float xRatio = deltaX / (float)cam.getWidth();
      float yRatio = deltaY / (float)cam.getHeight();
      float rotX = ( glm::radians( 360.0f ) ) * yRatio;
      float rotY = ( glm::radians( 360.0f ) ) * xRatio;

      const glm::quat matRotX = glm::rotate( glm::quat(), rotX, glm::vec3( 1.0f, 0.0f, 0.0f ) );
      const glm::quat appliedRot = glm::rotate( matRotX, rotY, glm::vec3( 0.0f, 1.0f, 0.0f ) );
      const glm::quat finalOri = appliedRot * startOri;

      cam.setOrientation( finalOri );
      cam.setPos( ( cam.getForward() * 8.0f ) * finalOri );
   }
   else
   {
      isPressed = false;
   }
}


int main()
{
   VERIFY( glfwInit(), "Cannot init glfw." );
   VERIFY( glfwVulkanSupported(), "Vulkan not supported." );

   loadCoreFunctions();

   ThreadPool threadPool( std::thread::hardware_concurrency() );

   std::vector<Vertex> vertices;
   std::vector<uint32_t> indices;
   auto done = loadModel( threadPool, "../models/armadillo.obj", &vertices, &indices );

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
   glfwSetCursorPosCallback( window, onMousePos );

   UniformBufferObject ubo = {};

   char windowTitle[ WINDOW_TITLE_SIZE ] = {};
   auto simStartTime = std::chrono::steady_clock::now();
   auto nextFpsPrintTime = 1s;
   unsigned frameRendered = 0;

   cam.setExtent( VK.getSwapChain()->_curExtent.width, VK.getSwapChain()->_curExtent.height );
   bool modelLoaded = false;
   while ( !glfwWindowShouldClose( window ) )
   {
      if ( !modelLoaded && done.wait_for( std::chrono::seconds( 0 ) ) == std::future_status::ready )
      {
         VK.createVertexBuffer( vertices );
         VK.createIndexBuffer( indices );
         VK.recreateSwapChain();
         modelLoaded = true;
      }
      updateCoreDll();
      glfwPollEvents();
      updateUBO( cam, ubo );
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