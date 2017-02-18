#ifndef MEMORY_POOL_H_
#define MEMORY_POOL_H_

#include <vector>
#include <inttypes.h>
#include <limits>
#include <algorithm>
#include <assert.h>
#include <string>

class MemoryPool
{
  public:
   static constexpr uint64_t INVALID_OFFSET = std::numeric_limits<uint64_t>::max();

   MemoryPool( uint64_t size, uint64_t maxAllocCount = 200 );
   uint64_t alloc( uint64_t size, uint64_t alignment );
   void free( uint64_t offset );

   bool _debugIsConform() const;
   std::string _debugPrint( int length, char emptyChar, char usedChar ) const;

   uint64_t spaceLeft() const { return _freeSpace; }
   uint64_t totalPoolSize() const { return _poolSize; }
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
   const uint64_t _poolSize;
   uint64_t _freeSpace;
};

#endif  // MEMORY_POOL_H_