#ifndef VK_MEMORY_POOL_
#define VK_MEMORY_POOL_

#include "vkUtils.h"
#include "MemoryPool.h"
#include <vulkan/vulkan.h>
#include <inttypes.h>

template <typename MEMORY>
struct MemAlloc
{
	MEMORY memory;
	uint64_t offset;
};

using VMemAlloc = MemAlloc<VkDeviceMemory>;

template <uint32_t SIZE, unsigned MAX_ALLOC = 200>
class VMemoryPool
{
  public:
   VMemoryPool( const VkPhysicalDevice& physDevice,
                          const VDeleter<VkDevice>& device,
                          VkMemoryPropertyFlagBits type )
       : _physDevice( physDevice ), _device( device )
   {
      vkGetPhysicalDeviceMemoryProperties( physDevice, &_memProperties );
      VkMemoryAllocateInfo allocInfo = {};
      allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
      allocInfo.allocationSize = SIZE;
      allocInfo.memoryTypeIndex = findMemoryType( _memProperties, type );

      VK_CALL( vkAllocateMemory( _device, &allocInfo, nullptr, &_memory ) );
   }

   VMemAlloc alloc(uint64_t size, uint64_t alignment) { return VMemAlloc{ *_memory.get(), _pool.alloc(size, alignment) }; }
   void free(VMemAlloc& mem)
   {
	   assert(mem.memory == *_memory.get());
	   _pool.free(mem.offset);
#ifdef DEBUG
	   mem.memory = nullptr;
	   mem.offset = -1;
#endif // DEBUG

   }
   operator VkDeviceMemory() { return *_memory.get(); }
  private:
   uint32_t findMemoryType( VkPhysicalDeviceMemoryProperties memProperties,
                            VkMemoryPropertyFlagBits propertiesFlag );

   const VkPhysicalDevice& _physDevice;
   const VDeleter<VkDevice>& _device;
   VDeleter<VkDeviceMemory> _memory{_device, vkFreeMemory};
   MemoryPool<SIZE, MAX_ALLOC> _pool;
   VkPhysicalDeviceMemoryProperties _memProperties;
   VkMemoryPropertyFlagBits _type;
};

template <uint32_t SIZE, unsigned MAX_ALLOC>
uint32_t VMemoryPool<SIZE, MAX_ALLOC>::findMemoryType(
   VkPhysicalDeviceMemoryProperties memProperties,
   VkMemoryPropertyFlagBits propertiesFlag )
{
   uint32_t index = VK_MAX_MEMORY_TYPES;
   VkDeviceSize size = 0;

   for ( uint32_t i = 0; i < memProperties.memoryTypeCount; ++i )
   {
      VkMemoryHeap& heap = memProperties.memoryHeaps[ memProperties.memoryTypes[ i ].heapIndex ];
      if ( memProperties.memoryTypes[ i ].propertyFlags & propertiesFlag )
      {
         if ( heap.size > size )
         {
            size = heap.size;
            index = i;
         }
      }
   }

   if ( size == 0 || index == VK_MAX_MEMORY_TYPES )
   {
      assert( !"Cannot get memory from device" );
   }

   return index;
}

#endif  // VK_MEMORY_POOL_