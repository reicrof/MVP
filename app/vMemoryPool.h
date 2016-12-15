#ifndef VK_MEMORY_POOL_
#define VK_MEMORY_POOL_

#include "vkUtils.h"
#include "MemoryPool.h"
#include <vulkan/vulkan.h>
#include <inttypes.h>
#include <memory>

struct VMemAlloc
{
   VkDeviceMemory memory;
   uint64_t offset;
};

class VMemoryPool
{
  public:
   VMemoryPool( uint64_t size,
                const VkPhysicalDevice& physDevice,
                const VDeleter<VkDevice>& device,
                uint32_t memTypeMask,
                VkMemoryPropertyFlags type,
                uint64_t maxAllocCount = 200 );

   uint64_t alloc( uint64_t size, uint64_t alignment );
   void free( VMemAlloc& mem );
   operator VkDeviceMemory();

  private:
   const VDeleter<VkDevice>& _device;
   VDeleter<VkDeviceMemory> _memory{_device, vkFreeMemory};
   MemoryPool _pool;
   VkPhysicalDeviceMemoryProperties _memProperties;
   VkMemoryPropertyFlags _type;
};

class VMemoryManager
{
  public:
   VMemoryManager( const VkPhysicalDevice& physDevice, const VDeleter<VkDevice>& device );
   VMemAlloc alloc( const VkMemoryRequirements& requirements,
                    const VkMemoryPropertyFlags& properties );
   void free( VMemAlloc& alloc );

  private:
   struct PoolProperties
   {
      VkMemoryPropertyFlags memPropertyFlags;
      uint32_t memoryTypeBits;
   };

   std::vector<std::unique_ptr<VMemoryPool> > _pools;
   std::vector<PoolProperties> _poolsProperties;
   const VkPhysicalDevice& _physDevice;
   const VDeleter<VkDevice>& _device;
};

#endif  // VK_MEMORY_POOL_