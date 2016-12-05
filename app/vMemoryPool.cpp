// #include "vMemoryPool.h"

// namespace
// {
// uint32_t findMemoryType( VkPhysicalDeviceMemoryProperties memProperties,
//                          VkMemoryPropertyFlagBits propertiesFlag )
// {
//    uint32_t index = VK_MAX_MEMORY_TYPES;
//    VkDeviceSize size = 0;

//    for ( uint32_t i = 0; i < memProperties.memoryTypeCount; ++i )
//    {
//       VkMemoryHeap& heap = memProperties.memoryHeaps[ memProperties.memoryTypes[ i ].heapIndex ];
//       if ( memProperties.memoryTypes[ i ].propertyFlags & propertiesFlag )
//       {
//          if ( heap.size > size )
//          {
//             size = heap.size;
//             index = i;
//          }
//       }
//    }

//    if ( size == 0 || index == VK_MAX_MEMORY_TYPES )
//    {
//       assert( !"Cannot get memory from device" );
//    }

//    return index;
// }

// }  // End of anonymous namespace

// VMemoryPool::VMemoryPool( const VkPhysicalDevice& physDevice,
//                           const VDeleter<VkDevice>& device,
//                           const uint64_t allocSize,
//                           VkMemoryPropertyFlagBits type )
//     : _physDevice( physDevice ),
//       _device( device ),
//       _type( type ),
//       _allocatedChunks{},
//       _totalSize( allocSize ),
//       _allocatedSize( 0 )
// {
//    vkGetPhysicalDeviceMemoryProperties( physDevice, &_memProperties );
//    VkMemoryAllocateInfo allocInfo = {};
//    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
//    allocInfo.allocationSize = allocSize;
//    allocInfo.memoryTypeIndex = findMemoryType( _memProperties, type );

//    VK_CALL( vkAllocateMemory( _device, &allocInfo, nullptr, &_memory ) );

//    // Create first default free chunk
//    _allocatedChunks.reserve( 100 );
//    _allocatedChunks.emplace_back( AllocChunk::MAX_SIZE, 0 );
// }

// VMemAlloc VMemoryPool::allocateMemory( uint64_t size, uint64_t alignment )
// {
//    // Get the first free block that can contains the requested allocation
//    auto it = _allocatedChunks.begin();
//    uint64_t alignmentPadding = 0;
//    while ( !it->isFree || alignmentPadding + size > it->size )
//    {
//       assert( it != _allocatedChunks.end() );
//       ++it;
//       // Get next padding requirement for next chunk. Equivalent of : ceil( cur / align ) * align
//       alignmentPadding = ( ( it->offset + alignment - 1 ) / alignment * alignment ) - it->offset;
//    }

//    // The free block becomes the new allocated block. It is also splitted to
//    // create the new free block.
//    const uint64_t previousFreeSize = it->size;
//    if ( it->prev )
//    {
//       // We give the offset to the previous chunk
//       it->prev->size += alignmentPadding;
//       // We thus need to update current chunk offset
//       it->offset += alignmentPadding;
//    }

//    // Modify the new allocated chunk
//    it->isFree = false;
//    it->size = size;

//    // Add new free block after the allocated one.
//    _allocatedChunks.emplace_back( previousFreeSize - alignmentPadding - size, it->offset + size
//    );

//    // Update prev next
//    _allocatedChunks.back().prev = &( *it );
//    it->next = &_allocatedChunks.back();

//    return VMemAlloc{_memory, it->offset};
// }

// VMemoryPool::operator VkDeviceMemory()
// {
//    return *_memory.get();
// }
