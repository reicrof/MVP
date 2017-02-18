#if defined( WIN32 ) || defined( _WIN32 )
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "Camera.h"
#include "utils.h"
#include "vulkanGraphic.h"

#include <glm/gtc/matrix_transform.hpp>

#include <assert.h>
#include <chrono>
#include <iostream>
#include <string>

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
   if( coreDll )
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

static void onWindowResized( GLFWwindow* window, int width, int height )
{
   if( width > 0 && height > 0 )
   {
      VulkanGraphic* VK = reinterpret_cast<VulkanGraphic*>(
          glfwGetWindowUserPointer( window ) );
      VK->recreateSwapChain();
   }
}

void updateCoreDll()
{
   if( shouldReloadCoreLib() )
   {
      loadCoreFunctions();
   }
}

#include "vertex.h"
const std::vector<Vertex> vertices = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
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

   bool success =
       tinyobj::LoadObj( &attrib, &shapes, &materials, &err, path.c_str() );
   if( success )
   {
      vertices->reserve( attrib.vertices.size() / 3 );
      indices->reserve( attrib.vertices.size() );
      for( const auto& shape : shapes )
      {
         for( const auto& index : shape.mesh.indices )
         {
            Vertex vertex = {};
            vertex.pos = {attrib.vertices[3 * index.vertex_index + 0],
                          attrib.vertices[3 * index.vertex_index + 1],
                          attrib.vertices[3 * index.vertex_index + 2]};
            if( !attrib.normals.empty() )
            {
               vertex.normal = {attrib.normals[3 * index.vertex_index + 0],
                                attrib.normals[3 * index.vertex_index + 1],
                                attrib.normals[3 * index.vertex_index + 2]};
            }

            if( attrib.texcoords.size() > 0 )
            {
               vertex.texCoord = {
                   attrib.texcoords[2 * index.texcoord_index + 0],
                   1.0f - attrib.texcoords[2 * index.texcoord_index + 1]};
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
static auto loadModel( ThreadPool& jobPool, const std::string& path,
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
   VERIFY( VK.createWidgetRenderPass(), "Cannot create a render pass." );
   VERIFY( VK.createDescriptorSetLayout(),
           "Cannot create descriptor set layout" );
   VERIFY(VK.createPipelineCache(), "Cannot create pipeline cache.");
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
   float time = std::chrono::duration_cast<std::chrono::milliseconds>(
                    currentTime - startTime )
                    .count() /
                10000.0f;

   ubo.model =
      glm::rotate( glm::mat4(), time * glm::radians( 90.0f ), glm::vec3( 0.0f, 1.0f, 0.0f ) );

   ubo.view = cam.getView();
   ubo.proj = cam.getProj();
}

#include "glmIncludes.h"
Camera cam( 45.0f, 1920, 1080, 0.1f, 20 );
VulkanGraphic* VKPtr = nullptr;
void onMousePos( GLFWwindow* window, double x, double y )
{
   constexpr double X_SENSITIVITY = 0.002;
   constexpr double Y_SENSITIVITY = 0.002;

   if (glfwGetInputMode(window, GLFW_CURSOR) != GLFW_CURSOR_DISABLED)
   {
	   return;
   }

   static glm::vec2 startPos( x, y );

   const glm::quat ori( glm::vec3( ( startPos.y - y ) * Y_SENSITIVITY,
                                   ( startPos.x - x ) * X_SENSITIVITY, 0.0f ) );
   cam.setOrientation( ori );
}

namespace KeyAction
{
enum Action
{
   UNDEF_ACTION,

   // Movement stuff
   MOVE_FORWARD,
   MOVE_BACKWARD,
   STRAFE_LEFT,
   STRAFE_RIGHT,

   // Menu stuff
   EXIT,
   TOGGLE_MOUSE_CAPTURE,

   LAST_KEY_ACTION
};
}

static std::array<int, GLFW_KEY_LAST> actionKeyStates = {};
static std::array<KeyAction::Action, GLFW_KEY_LAST> glfwKeyToAction = {};

static void keyboardActionInit()
{
   glfwKeyToAction[GLFW_KEY_W] = KeyAction::MOVE_FORWARD;
   glfwKeyToAction[GLFW_KEY_S] = KeyAction::MOVE_BACKWARD;
   glfwKeyToAction[GLFW_KEY_A] = KeyAction::STRAFE_LEFT;
   glfwKeyToAction[GLFW_KEY_D] = KeyAction::STRAFE_RIGHT;
   glfwKeyToAction[GLFW_KEY_ESCAPE] = KeyAction::EXIT;
   glfwKeyToAction[GLFW_KEY_SPACE] = KeyAction::TOGGLE_MOUSE_CAPTURE;

   actionKeyStates[KeyAction::MOVE_FORWARD] = GLFW_RELEASE;
   actionKeyStates[KeyAction::MOVE_BACKWARD] = GLFW_RELEASE;
   actionKeyStates[KeyAction::STRAFE_LEFT] = GLFW_RELEASE;
   actionKeyStates[KeyAction::STRAFE_RIGHT] = GLFW_RELEASE;
   actionKeyStates[KeyAction::EXIT] = GLFW_RELEASE;
   actionKeyStates[KeyAction::TOGGLE_MOUSE_CAPTURE] = GLFW_RELEASE;
}

static void keyCB( GLFWwindow* window, int key, int scancode, int action,
                   int mods )
{
   actionKeyStates[glfwKeyToAction[key]] = action;

   if( key == GLFW_KEY_P && action == GLFW_PRESS )
   {
      VKPtr->_debugPrintMemoryMgrInfo();
   }
}

static void pollKeyboard( GLFWwindow* window )
{
   if( actionKeyStates[KeyAction::MOVE_FORWARD] > 0 )
   {
      actionKeyStates[KeyAction::MOVE_FORWARD] = GLFW_REPEAT;
      cam.setPos( cam.getPos() - cam.getForward() * 0.01f );
   }
   else if( actionKeyStates[KeyAction::MOVE_BACKWARD] > 0 )
   {
      actionKeyStates[KeyAction::MOVE_BACKWARD] = GLFW_REPEAT;
      cam.setPos( cam.getPos() + cam.getForward() * 0.01f );
   }

   if( actionKeyStates[KeyAction::STRAFE_LEFT] > 0 )
   {
      actionKeyStates[KeyAction::STRAFE_LEFT] = GLFW_REPEAT;
      cam.setPos( cam.getPos() - cam.getRight() * 0.01f );
   }
   else if( actionKeyStates[KeyAction::STRAFE_RIGHT] > 0 )
   {
      actionKeyStates[KeyAction::STRAFE_RIGHT] = GLFW_REPEAT;
      cam.setPos( cam.getPos() + cam.getRight() * 0.01f );
   }

   if( actionKeyStates[KeyAction::EXIT] == GLFW_PRESS )
   {
      glfwSetWindowShouldClose( window, GLFW_TRUE );
      actionKeyStates[KeyAction::EXIT] = GLFW_REPEAT;
   }

   if( actionKeyStates[KeyAction::TOGGLE_MOUSE_CAPTURE] == GLFW_PRESS )
   {
      const auto mode = glfwGetInputMode( window, GLFW_CURSOR );
      glfwSetInputMode( window, GLFW_CURSOR,
                        mode == GLFW_CURSOR_DISABLED ? GLFW_CURSOR_NORMAL
                                                     : GLFW_CURSOR_DISABLED );
      actionKeyStates[KeyAction::TOGGLE_MOUSE_CAPTURE] = GLFW_REPEAT;
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
   auto done =
       loadModel( threadPool, "../models/armadillo.obj", &vertices, &indices );
   // auto done = loadModel(threadPool, "../models/crate.obj", &vertices,
   // &indices);

   glfwWindowHint( GLFW_CLIENT_API, GLFW_NO_API );
   GLFWwindow* window = glfwCreateWindow( 800, 600, "MVP", nullptr, nullptr );

   VERIFY( window, "Could not create GLFW window." );

   unsigned int glfwExtensionCount = 0;
   const char** glfwExtensions;
   glfwExtensions = glfwGetRequiredInstanceExtensions( &glfwExtensionCount );
   std::vector<const char*> extensions( glfwExtensionCount );
   std::copy( glfwExtensions, glfwExtensions + glfwExtensionCount,
              extensions.begin() );

   VulkanGraphic VK( extensions );
   initVulkan( VK, window );
   VKPtr = &VK;

   cam.setPos( glm::vec3( 0.0f, 0.0f, 10.0f ) );

   // Setup callback function
   keyboardActionInit();
   glfwSetKeyCallback( window, keyCB );
   glfwSetWindowUserPointer( window, &VK );  // Set user data
   glfwSetWindowSizeCallback( window, onWindowResized );
   glfwSetCursorPosCallback( window, onMousePos );
   glfwSetInputMode( window, GLFW_CURSOR, GLFW_CURSOR_DISABLED );

   UniformBufferObject ubo = {};

   char windowTitle[WINDOW_TITLE_SIZE] = {};
   auto simStartTime = std::chrono::steady_clock::now();
   auto nextFpsPrintTime = 1s;
   unsigned frameRendered = 0;

   cam.setExtent( VK.getSwapChain()->_curExtent.width,
                  VK.getSwapChain()->_curExtent.height );
   bool modelLoaded = false;
   while( !glfwWindowShouldClose( window ) )
   {
      if( !modelLoaded &&
          done.wait_for( std::chrono::seconds( 0 ) ) ==
              std::future_status::ready )
      {
         VK.createVertexBuffer( vertices );
         VK.createIndexBuffer( indices );
         VK.recreateSwapChain();
         modelLoaded = true;
      }
      updateCoreDll();
      glfwPollEvents();
      pollKeyboard( window );
      updateUBO( cam, ubo );
      VK.updateUBO( ubo );
      // std::cout << ptr() << std::endl;
      VK.render();

      ++frameRendered;
      if( std::chrono::steady_clock::now() - simStartTime > nextFpsPrintTime )
      {
         std::snprintf( windowTitle, WINDOW_TITLE_SIZE, "MVP - %i FPS",
                        frameRendered );
         glfwSetWindowTitle( window, windowTitle );
         frameRendered = 0;
         nextFpsPrintTime += 1s;
      }
   }

   VK.savePipelineCacheToDisk();

   threadPool.stop();

   glfwDestroyWindow( window );
   glfwTerminate();
}
