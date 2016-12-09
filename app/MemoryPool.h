#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include <vector>
#include <inttypes.h>
#include <limits>
#include <algorithm>
#include <assert.h>

template <uint64_t SIZE, unsigned MAX_ALLOC>
class MemoryPool
{
  public:
   static constexpr uint64_t INVALID_OFFSET = std::numeric_limits<uint64_t>::max();

   MemoryPool() : _freeSpace( SIZE )
   {
      _allocatedChunks.reserve( MAX_ALLOC );
      // Create first default free chunk
      _allocatedChunks.emplace_back( SIZE, 0 );
   }

   uint64_t alloc( uint64_t size, uint64_t alignment )
   {
      // Get the first free chunk that can contains the requested allocation
      auto it = _allocatedChunks.begin();
      const auto endIt = _allocatedChunks.cend();
      const auto minChunkSize = ( alignment - 1 ) + size;
      while ( !it->isFree || minChunkSize > it->size )
      {
         ++it;
         assert( it != endIt );
      }

      // Get padding requirement for chunk. Equivalent of : ceil( cur / align ) * align
      uint64_t alignmentPadding =
         ( ( it->offset + alignment - 1 ) / alignment * alignment ) - it->offset;

      // The free chunk becomes the new allocated chunk. The remaining space is
      // splitted to create a new free chunk.
      const uint64_t previousFreeSize = it->size;
      if ( it != _allocatedChunks.begin() )
      {
         auto prevIt = it - 1;
         // We give the alignment offset to the previous chunk
         prevIt->size += alignmentPadding;
         // We thus need to update current chunk offset
         it->offset += alignmentPadding;

         // We gave the space to the previous chunk. If it was used,
         // we need to adjust the remaining free space.
         if ( !prevIt->isFree )
         {
            _freeSpace -= alignmentPadding;
         }
      }

      // Modify the new allocated chunk
      it->isFree = false;
      it->size = size;

      // Append leftover memory the the next chunk or create a new free chunk if none exists.
      const uint64_t leftOverMem = previousFreeSize - alignmentPadding - size;
      if ( leftOverMem > 0 )
      {
         const auto itNextChunk = it + 1;
         if ( itNextChunk != endIt && itNextChunk->isFree )
         {
            itNextChunk->size += leftOverMem;
            itNextChunk->offset -= leftOverMem;
         }
         else
         {
            // Vector will be reallocated. You should increase the starting capacity.
            assert( _allocatedChunks.capacity() >= _allocatedChunks.size() );
            it = --_allocatedChunks.insert( it + 1, AllocChunk{leftOverMem, it->offset + size} );
         }
      }

      _freeSpace -= size;

#ifdef _DEBUG
      assert( _debugIsConform() );
#endif  // DEBUG

      return it->offset;
   }

   void free( uint64_t offset )
   {
      // Find the chunk
      auto it =
         std::lower_bound( _allocatedChunks.begin(), _allocatedChunks.end(), offset,
                           []( const AllocChunk& lhs, uint64_t val ) { return lhs.offset < val; } );

      // Unknown chunk or already freed chunk
      assert( it != _allocatedChunks.end() && !it->isFree );

      // Current chunk is now free.
      it->isFree = true;

      _freeSpace += it->size;

      // Try to merge the freed chunk with the one on the left and the right.
      // Merge right if not last chunk and if next chunk exists.
      const bool mergeRight =
         it != _allocatedChunks.end() && ( it + 1 ) != _allocatedChunks.end() && ( it + 1 )->isFree;
      // Merge left if not first chunk and if previous chunk exists.
      const bool mergeLeft = it != _allocatedChunks.begin() &&
                             ( it - 1 ) != _allocatedChunks.begin() && ( it - 1 )->isFree;

      typename std::vector<AllocChunk>::iterator itToRemoves[ 2 ] = {it, it};
      if ( mergeRight )
      {
         const auto nextIt = it + 1;
         nextIt->size += it->size;
         nextIt->offset -= it->size;
         it = nextIt;  // The current block is now the next one merged.
      }
      if ( mergeLeft )
      {
         auto prevIt = it - 1;
         if ( mergeRight )
            --prevIt;
         prevIt->size += it->size;
      }

      // Since we always merge to the left, we remove the chunk to
      // the right, according to the number of merge done.
      itToRemoves[ 1 ] += ( mergeLeft + mergeRight );
      _allocatedChunks.erase( itToRemoves[ 0 ], itToRemoves[ 1 ] );

#ifdef _DEBUG
      assert( _debugIsConform() );
#endif  // DEBUG
   }



   bool _debugIsConform() const
   {
      bool isConform = true;
      bool freeSpaceMatch = true;
      bool totalSizeMatch = true;
      uint64_t totalSize = 0;
      uint64_t freeSpace = 0;
      for ( auto it = _allocatedChunks.begin(); it != _allocatedChunks.end(); ++it )
      {
         isConform &= it->offset == totalSize;
         totalSize += it->size;
         if ( it->isFree )
         {
            freeSpace += it->size;
         }
         assert( isConform );
      }

      totalSizeMatch = totalSize == SIZE;
      assert( totalSizeMatch );

      freeSpaceMatch = freeSpace == _freeSpace;
      assert( freeSpaceMatch );

      return isConform && totalSizeMatch && freeSpaceMatch;
   }


  private:
   struct AllocChunk
   {
      static constexpr uint64_t MAX_SIZE = 1ull << 62;
      AllocChunk( uint64_t allocSize, uint64_t allocOffset )
          : isFree( true ), size( allocSize ), offset( allocOffset )
      {
         assert( allocSize <= MAX_SIZE );  // Something is probably wrong...
      }

      uint64_t isFree : 1;
      uint64_t size : 63;
      uint64_t offset;
   };

   std::vector<AllocChunk> _allocatedChunks;
   uint64_t _freeSpace;
};

#endif  // MEMORY_POOL_H_