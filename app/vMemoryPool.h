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
                const VkDevice& device,
                uint32_t memTypeMask,
                VkMemoryPropertyFlags type,
                uint64_t maxAllocCount = 200 );

   uint64_t alloc( uint64_t size, uint64_t alignment );
   void free( VMemAlloc& mem );
   uint64_t spaceLeft() const;
   uint64_t totalSize() const;
   operator VkDeviceMemory();

   std::string _debugPrint( int totalLength, char empty, char used ) const;

  private:
   const VkDevice _device;
   VDeleter<VkDeviceMemory> _memory{_device, vkFreeMemory};
   MemoryPool _pool;
   VkPhysicalDeviceMemoryProperties _memProperties;
   VkMemoryPropertyFlags _type;
};

class VMemoryManager
{
  public:
   VMemoryManager();
   VMemoryManager( const VkPhysicalDevice& physDevice, const VDeleter<VkDevice>& device );
   void init( const VkPhysicalDevice& physDevice, const VkDevice& device );
   VMemAlloc alloc( const VkMemoryRequirements& requirements,
                    const VkMemoryPropertyFlags& properties );
   void free( VMemAlloc& alloc );

   void _debugPrint() const;

  private:
   struct PoolsType
   {
      VkMemoryPropertyFlags _properties;
      uint32_t _memTypeBits;
      PoolsType( const VkMemoryRequirements& requirements, const VkMemoryPropertyFlags& properties )
          : _properties( properties ), _memTypeBits( requirements.memoryTypeBits )
      {
      }
      bool operator==( const PoolsType& rhs )
      {
         return _memTypeBits & rhs._memTypeBits && ( _properties & rhs._properties ) == _properties;
      }
   };

   std::vector<std::vector<std::unique_ptr<VMemoryPool> > > _pools;
   std::vector<PoolsType> _poolsProperties;
   VkPhysicalDevice _physDevice;
   VkDevice _device;
};

#endif  // VK_MEMORY_POOL_