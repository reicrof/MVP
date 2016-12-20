#include "vMemoryPool.h"
#include <assert.h>

namespace
{
uint32_t findMemoryType( uint32_t typeFilter,
                         VkPhysicalDeviceMemoryProperties memProperties,
                         VkMemoryPropertyFlags propertiesFlag )
{
   uint32_t index = VK_MAX_MEMORY_TYPES;
   VkDeviceSize size = 0;

   for ( uint32_t i = 0; i < memProperties.memoryTypeCount; ++i )
   {
      VkMemoryHeap& heap = memProperties.memoryHeaps[ memProperties.memoryTypes[ i ].heapIndex ];
      if ( ( typeFilter & ( 1 << i ) ) &&
           ( memProperties.memoryTypes[ i ].propertyFlags & propertiesFlag ) == propertiesFlag )
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
}

VMemoryPool::VMemoryPool( uint64_t size,
                          const VkPhysicalDevice& physDevice,
                          const VDeleter<VkDevice>& device,
                          uint32_t memTypeMask,
                          VkMemoryPropertyFlags type,
                          uint64_t maxAllocCount /*= 200*/ )
    : _device( device ), _pool( size, maxAllocCount ), _type( type )
{
   vkGetPhysicalDeviceMemoryProperties( physDevice, &_memProperties );
   VkMemoryAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocInfo.allocationSize = size;
   allocInfo.memoryTypeIndex = findMemoryType( memTypeMask, _memProperties, _type );

   VK_CALL( vkAllocateMemory( _device, &allocInfo, nullptr, &_memory ) );
}

uint64_t VMemoryPool::alloc( uint64_t size, uint64_t alignment )
{
   return _pool.alloc( size, alignment );
}

void VMemoryPool::free( VMemAlloc& mem )
{
   // assert(mem.memory == *_memory.get());
   _pool.free( mem.offset );
#ifdef DEBUG
   mem.memory = nullptr;
   mem.offset = -1;
#endif  // DEBUG
}

VMemoryPool::operator VkDeviceMemory()
{
   return *_memory.get();
}

////////////////////////////////////////

VMemoryManager::VMemoryManager( const VkPhysicalDevice& physDevice,
                                const VDeleter<VkDevice>& device )
    : _physDevice( physDevice ), _device( device )
{
}

VMemAlloc VMemoryManager::alloc( const VkMemoryRequirements& requirements,
                                 const VkMemoryPropertyFlags& properties )
{
   for ( size_t i = 0; i < _poolsProperties.size(); ++i )
   {
      if ( requirements.memoryTypeBits & _poolsProperties[ i ].memoryTypeBits &&
           ( properties & _poolsProperties[ i ].memPropertyFlags ) == properties )
      {
         return VMemAlloc{*_pools[ i ],
                          _pools[ i ]->alloc( requirements.size, requirements.alignment )};
      }
   }

   // No pool meets the requirement. Lets create one.
   _poolsProperties.push_back( PoolProperties{properties, requirements.memoryTypeBits} );
   _pools.push_back( std::make_unique<VMemoryPool>( requirements.size * 4, _physDevice, _device,
                                                    requirements.memoryTypeBits, properties ) );

   return VMemAlloc{*_pools.back(),
                    _pools.back()->alloc( requirements.size, requirements.alignment )};
}

void VMemoryManager::free( VMemAlloc& alloc )
{
   auto it = std::find_if( _pools.begin(), _pools.end(), [&]( const auto& pool ) {
      return static_cast<VkDeviceMemory>( *pool.get() ) == alloc.memory;
   } );

   assert( it != _pools.end() );

   ( *it )->free( alloc );
}
