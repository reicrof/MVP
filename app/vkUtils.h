#ifndef VK_UTILS_H_
#define VK_UTILS_H_
#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include <assert.h>
#include <iostream>

#define VK_CALL( func )                                                                    \
   if ( func != VK_SUCCESS )                                                               \
   {                                                                                       \
      std::cerr << "Error when calling " << #func << " at " << __FILE__ << ":" << __LINE__ \
                << std::endl;                                                              \
      assert( false );                                                                     \
   }

template <typename T>
class VDeleter
{
  public:
   VDeleter() : VDeleter( []( T _ ) {} ) {}
   VDeleter( std::function<void( T, VkAllocationCallbacks* )> deletef )
   {
      this->deleter = [=]( T obj ) { deletef( obj, nullptr ); };
   }

   VDeleter( const VDeleter<VkInstance>& instance,
             std::function<void( VkInstance, T, VkAllocationCallbacks* )> deletef )
   {
      this->deleter = [&instance, deletef]( T obj ) { deletef( instance, obj, nullptr ); };
   }

   VDeleter( const VDeleter<VkDevice>& device,
             std::function<void( VkDevice, T, VkAllocationCallbacks* )> deletef )
   {
      this->deleter = [&device, deletef]( T obj ) { deletef( device, obj, nullptr ); };
   }

   VDeleter( VkDevice device, std::function<void( VkDevice, T, VkAllocationCallbacks* )> deletef )
   {
      this->deleter = [device, deletef]( T obj ) { deletef( device, obj, nullptr ); };
   }

   T* get() { return &object; }
   void set( const T& newObject )
   {
      cleanup();
      object = newObject;
   }

   const T* get() const { return &object; }
   const T* operator&() const { return &object; }
   ~VDeleter() { cleanup(); }
   T* operator&()
   {
      cleanup();
      return &object;
   }

   operator T() const { return object; }
  private:
   T object{VK_NULL_HANDLE};
   std::function<void( T )> deleter;

   void cleanup()
   {
      if ( object != VK_NULL_HANDLE )
      {
         deleter( object );
      }
      object = VK_NULL_HANDLE;
   }
};

struct VMemAlloc;
class VMemoryManager;
class VCommandPool;
class VImage;

namespace VkUtils
{
void endCmdBufferAndSubmit( VkCommandBuffer cmdBuf,
                            VkQueue& queue,
                            uint32_t waitSemCount = 0,
                            VkSemaphore* waitSem = nullptr,
                            uint32_t signalSemCount = 0,
                            VkSemaphore* signalSem = nullptr,
                            VkFence fenceToSignal = VK_NULL_HANDLE );

VkCommandBuffer copyBuffer( VkCommandBuffer cmdBuffer,
                            VkBuffer source,
                            VkBuffer dest,
                            VkDeviceSize size );

VMemAlloc createBuffer( VkDevice device,
                        VMemoryManager& memoryManager,
                        VkMemoryPropertyFlags memProperty,
                        VkDeviceSize size,
                        VkBufferUsageFlags usage,
                        VkBuffer& buffer );

void createImage( VkDevice device,
                  VMemoryManager& memoryManager,
                  uint32_t width,
                  uint32_t height,
                  uint32_t mips,
                  VkFormat format,
                  VkImageTiling tiling,
                  VkImageUsageFlags usage,
                  VkMemoryPropertyFlags memProperty,
                  VImage& img );

VkCommandBuffer transitionImgLayout( VkImage image,
                                     VkFormat format,
                                     VkImageLayout oldLayout,
                                     VkImageLayout newLayout,
                                     VkCommandBuffer cmdBuffer );

VkShaderModule createShaderModule( VkDevice device, const std::string& path );

VkDescriptorSetLayout createDescriptorSetLayout(
   VkDevice device,
   const std::vector<VkDescriptorSetLayoutBinding>& bindings );

VkWriteDescriptorSet createWriteDescriptorSet( VkDescriptorSet dstSet,
                                               uint32_t dstBinding,
                                               uint32_t dstArrayEl,
                                               const VkDescriptorBufferInfo* bufInfo,
                                               uint32_t bufElCount );

VkWriteDescriptorSet createWriteDescriptorSet( VkDescriptorSet dstSet,
                                               uint32_t dstBinding,
                                               uint32_t dstArrayEl,
                                               const VkDescriptorImageInfo* bufInfo,
                                               uint32_t bufElCount );

VkFence createFence( VkDevice device, VkFenceCreateFlags flag );
}

#endif  // !VK_UTILS_H_
