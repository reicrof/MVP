#ifndef VK_MEMORY_POOL_
#define VK_MEMORY_POOL_

#include <vulkan/vulkan.h>
#include "vkUtils.h"

#include <utility>
#include <limits>

struct VMemAlloc
{
	VkDeviceMemory memory;
	size_t offset;
};

class VMemoryPool
{
  public:
   VMemoryPool( const VkPhysicalDevice& physDevice,
                const VDeleter<VkDevice>& device,
                const uint32_t bufferSize,
                VkMemoryPropertyFlagBits type );

   static constexpr size_t INVALID_OFFSET = std::numeric_limits<size_t>::max();
   VMemAlloc allocateMemory( size_t size, size_t alignment );
   operator VkDeviceMemory();

  private:
   const VkPhysicalDevice& _physDevice;
   const VDeleter<VkDevice>& _device;
   VDeleter<VkDeviceMemory> _memory{_device, vkFreeMemory};
   VkPhysicalDeviceMemoryProperties _memProperties;
   VkMemoryPropertyFlagBits _type;

   const size_t _totalSize;
   size_t _allocatedSize;
   int _allocationCount;
};

#endif  // VK_MEMORY_POOL_