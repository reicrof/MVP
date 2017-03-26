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
                          const VkDevice& device,
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
   _pool.free( mem.offset );
   mem.memory = nullptr;
   mem.offset = -1;
}

uint64_t VMemoryPool::spaceLeft() const
{
   return _pool.spaceLeft();
}

uint64_t VMemoryPool::totalSize() const
{
   return _pool.totalPoolSize();
}

std::string VMemoryPool::_debugPrint( int totalLength, char empty, char used ) const
{
   return _pool._debugPrint( totalLength, empty, used );
}

VMemoryPool::operator VkDeviceMemory()
{
   return *_memory.get();
}

////////////////////////////////////////

VMemoryManager::VMemoryManager() : _physDevice( VK_NULL_HANDLE ), _device( VK_NULL_HANDLE )
{
}

VMemoryManager::VMemoryManager( const VkPhysicalDevice& physDevice,
                                const VDeleter<VkDevice>& device )
    : _physDevice( physDevice ), _device( device )
{
}

void VMemoryManager::init( const VkPhysicalDevice& physDevice, const VkDevice& device )
{
   _physDevice = physDevice;
   _device = device;
}

VMemAlloc VMemoryManager::alloc( const VkMemoryRequirements& requirements,
                                 const VkMemoryPropertyFlags& properties )
{
   PoolsType curType{requirements, properties};
   std::vector<std::unique_ptr<VMemoryPool> >* validPools = nullptr;
   for ( size_t i = 0; i < _poolsProperties.size(); ++i )
   {
      if ( curType == _poolsProperties[ i ] )
      {
         // We have found the list of pools that are valid with requested alloc
         validPools = &_pools[ i ];
         uint64_t allocOffset = MemoryPool::INVALID_OFFSET;
         for ( size_t j = 0; j < validPools->size(); ++j )
         {
            allocOffset = ( *validPools )[ j ]->alloc( requirements.size, requirements.alignment );
            if ( allocOffset != MemoryPool::INVALID_OFFSET )
            {
               return VMemAlloc{*( *validPools )[ j ], allocOffset};
            }
         }
      }
   }

   // No pool meets the requirement. Lets create one.

   // If no pools satisfies the type required, create new list of pools
   VMemoryPool* pool = nullptr;
   if ( !validPools )
   {
      // Creates a new list of pool, with a first pool containing enough space to allocate 4 times
      // the requested amount.
      _pools.emplace_back();
      _pools.back().emplace_back( std::make_unique<VMemoryPool>(
         requirements.size * 4, _physDevice, _device, requirements.memoryTypeBits, properties ) );

      // Add the new pool properties to the list property.
      _poolsProperties.emplace_back( requirements, properties );
      pool = _pools.back().back().get();
   }
   else
   {
      // There is already a list of pools satisfying the required types. We need more memory of this
      // type.
      // Lets create a new pool, doubling the size of the previous pool.
      const uint64_t newSize =
         std::max( validPools->back()->totalSize() * 2, requirements.size * 4 );
      validPools->emplace_back( std::unique_ptr<VMemoryPool>{std::make_unique<VMemoryPool>(
         newSize, _physDevice, _device, requirements.memoryTypeBits, properties )} );
      pool = validPools->back().get();
   }

   assert( pool );

   return VMemAlloc{*pool, pool->alloc( requirements.size, requirements.alignment )};
}

void VMemoryManager::free( VMemAlloc& alloc )
{
   for ( auto& poolList : _pools )
   {
      for ( auto& pool : poolList )
      {
         if ( *pool.get() == alloc.memory )
         {
            pool->free( alloc );
            return;
         }
      }
   }

   assert( false && "Alloc not found" );
}

#include <iostream>
void VMemoryManager::_debugPrint() const
{
   using namespace std;
   for ( size_t i = 0; i < _poolsProperties.size(); ++i )
   {
      cout << "Properties : " << _poolsProperties[ i ]._properties
           << " | Mem Type bits : " << _poolsProperties[ i ]._memTypeBits << endl;
      for ( const auto& pool : _pools[ i ] )
      {
         cout << pool->_debugPrint( 80, ' ', '=' ) << '\n';
      }
   }
}
