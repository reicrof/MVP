#include "MemoryPool.h"

MemoryPool::MemoryPool(uint64_t size, uint64_t maxAllocCount /*=200*/) : _poolSize(size), _freeSpace(size)
{
	_allocatedChunks.reserve(maxAllocCount);
	// Create first default free chunk
	_allocatedChunks.emplace_back(size, 0);
}

uint64_t MemoryPool::alloc(uint64_t size, uint64_t alignment)
{
	// Alignment needs to be a power of two.
	assert((alignment != 0) && !(alignment & (alignment - 1)));

	// Get the first free chunk that can contains the requested allocation
	const auto endIt = _allocatedChunks.cend();
	const uint64_t alignmentMask = alignment - 1;
	auto it =
		std::find_if(_allocatedChunks.begin(), _allocatedChunks.end(), [=](const auto& chunk) {
		// Get padding requirement for chunk. Equivalent of : ceil( cur / align ) * align
		// plus its size
		return chunk.isFree &&
			chunk.size >=
			(alignment - ((chunk.offset & alignmentMask)) & alignmentMask) + size;
	});
	assert(it != endIt); // Probably out of free space...
	const uint64_t alignmentPadding =
		(alignment - (it->offset & alignmentMask)) & alignmentMask;

	// The free chunk becomes the new allocated chunk. The remaining space is
	// splitted to create a new free chunk.
	const uint64_t previousFreeSize = it->size;
	if (it != _allocatedChunks.begin())
	{
		auto prevIt = it - 1;
		// We give the alignment offset to the previous chunk
		prevIt->size += alignmentPadding;
		// We thus need to update current chunk offset
		it->offset += alignmentPadding;

		// We gave the space to the previous chunk. If it was used,
		// we need to adjust the remaining free space.
		if (!prevIt->isFree)
		{
			_freeSpace -= alignmentPadding;
		}
	}

	// Modify the new allocated chunk
	it->isFree = false;
	it->size = size;

	// Append leftover memory the the next chunk or create a new free chunk if none exists.
	if (previousFreeSize > alignmentPadding + size)
	{
		const uint64_t leftOverMem = previousFreeSize - alignmentPadding - size;
		const auto itNextChunk = it + 1;
		if (itNextChunk != endIt && itNextChunk->isFree)
		{
			itNextChunk->size += leftOverMem;
			itNextChunk->offset -= leftOverMem;
		}
		else
		{
			// Vector will be reallocated. You should increase the starting capacity.
			assert(_allocatedChunks.capacity() >= _allocatedChunks.size());
			it = --_allocatedChunks.insert(it + 1, AllocChunk{ leftOverMem, it->offset + size });
		}
	}

	_freeSpace -= size;

#ifdef _DEBUG
	assert(_debugIsConform());
#endif  // DEBUG

	return it->offset;
}

void MemoryPool::free(uint64_t offset)
{
	// Find the chunk
	auto it =
		std::lower_bound(_allocatedChunks.begin(), _allocatedChunks.end(), offset,
			[](const AllocChunk& lhs, uint64_t val) { return lhs.offset < val; });

	// Unknown chunk or already freed chunk
	assert(it != _allocatedChunks.end() && !it->isFree);

	// Current chunk is now free.
	it->isFree = true;

	_freeSpace += it->size;

	// Try to merge the freed chunk with the one on the left and the right.
	// Merge right if not last chunk and if next chunk exists.
	const bool mergeRight =
		it != _allocatedChunks.end() && (it + 1) != _allocatedChunks.end() && (it + 1)->isFree;
	// Merge left if not first chunk and if previous chunk exists.
	const bool mergeLeft = it != _allocatedChunks.begin() &&
		(it - 1) != _allocatedChunks.begin() && (it - 1)->isFree;

	std::vector<AllocChunk>::iterator itToRemoves[2] = { it, it };
	if (mergeRight)
	{
		const auto nextIt = it + 1;
		nextIt->size += it->size;
		nextIt->offset -= it->size;
		it = nextIt;  // The current block is now the next one merged.
	}
	if (mergeLeft)
	{
		auto prevIt = it - 1;
		if (mergeRight)
			--prevIt;
		prevIt->size += it->size;
	}

	// Since we always merge to the left, we remove the chunk to
	// the right, according to the number of merge done.
	itToRemoves[1] += (mergeLeft + mergeRight);
	_allocatedChunks.erase(itToRemoves[0], itToRemoves[1]);

#ifdef _DEBUG
	assert(_debugIsConform());
#endif  // DEBUG
}

bool MemoryPool::_debugIsConform() const
{
	bool isConform = true;
	bool freeSpaceMatch = true;
	bool totalSizeMatch = true;
	uint64_t totalSize = 0;
	uint64_t freeSpace = 0;
	for (auto it = _allocatedChunks.begin(); it != _allocatedChunks.end(); ++it)
	{
		isConform &= it->offset == totalSize;
		totalSize += it->size;
		if (it->isFree)
		{
			freeSpace += it->size;
		}
		assert(isConform);
	}

	totalSizeMatch = totalSize == _poolSize;
	assert(totalSizeMatch);

	freeSpaceMatch = freeSpace == _freeSpace;
	assert(freeSpaceMatch);

	return isConform && totalSizeMatch && freeSpaceMatch;
}
