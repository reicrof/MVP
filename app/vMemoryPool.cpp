#include "vMemoryPool.h"

namespace
{
uint32_t findMemoryType( VkPhysicalDeviceMemoryProperties memProperties,
                         VkMemoryPropertyFlagBits propertiesFlag )
{
   uint32_t index = VK_MAX_MEMORY_TYPES;
   VkDeviceSize size = 0;

   for ( uint32_t i = 0; i < memProperties.memoryTypeCount; ++i )
   {
      VkMemoryHeap& heap = memProperties.memoryHeaps[ memProperties.memoryTypes[ i ].heapIndex ];
      if ( memProperties.memoryTypes[ i ].propertyFlags & propertiesFlag )
      {
		  if (heap.size > size)
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

}  // End of anonymous namespace

VMemoryPool::VMemoryPool( const VkPhysicalDevice& physDevice,
                          const VDeleter<VkDevice>& device,
                          const uint32_t allocSize,
                          VkMemoryPropertyFlagBits type )
    : _physDevice( physDevice ),
      _device( device ),
      _type( type ),
      _totalSize(allocSize),
	_allocatedSize( 0 ),
	_allocationCount( 0 )
{
   vkGetPhysicalDeviceMemoryProperties( physDevice, &_memProperties );
   VkMemoryAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = allocSize;
   allocInfo.memoryTypeIndex = findMemoryType( _memProperties, type );

   VK_CALL( vkAllocateMemory( _device, &allocInfo, nullptr, &_memory ) );
}

VMemAlloc VMemoryPool::allocateMemory( size_t size, size_t alignment )
{
   // Get the first aligned position. Equivalent of : ceil( cur / align ) * align
   const size_t allocPos = ( _allocatedSize + alignment - 1 ) / alignment * alignment;
   size_t offset = INVALID_OFFSET;
   if ( allocPos < _totalSize )
   {
      offset = allocPos;
	  _allocatedSize = allocPos + size;
	  ++_allocationCount;
   }
   return VMemAlloc{ _memory, offset };
}

VMemoryPool::operator VkDeviceMemory ()
{
   return *_memory.get();
}
