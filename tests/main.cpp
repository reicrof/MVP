#include <app/MemoryPool.h>
#include <memory>
#include <inttypes.h>
#include <assert.h>
#include <random>
#include <iostream>

std::mt19937 rng;
static auto randNum(int from, int to)
{
	std::uniform_int_distribution<std::mt19937::result_type> dist6(from, to);
	return dist6(rng);
}

bool memoryPoolAllocateAll()
{
	constexpr uint64_t size = 1024;
	MemoryPool<size, 1024> pool;

	uint64_t allocs[size];
	for (int i = 0; i < size-1; ++i)
	{
		allocs[i] = pool.alloc(1, 1);
	}

	return pool._debugIsConform();
}

bool memoryPoolAllocateAllAndDeallocateAll()
{
	constexpr uint64_t size = 1024;
	MemoryPool<size, 1024> pool;

	uint64_t allocs[size];
	for (int i = 0; i < size; ++i)
	{
		allocs[i] = pool.alloc(1, 1);
	}
	for (int i = 0; i < size; ++i)
	{
		pool.free(allocs[i]);
	}
	for (int i = 0; i < size; ++i)
	{
		allocs[i] = pool.alloc(1, 1);
	}
	for (int i = size-1; i > -1 ; --i)
	{
		pool.free(allocs[i]);
	}

	return pool._debugIsConform();
}

bool memoryAllocateDeallocateHalfTheTime()
{
	constexpr uint64_t size = 1024;
	MemoryPool<size, 1024> pool;

	uint64_t allocs[size];
	for (int i = 0; i < size; ++i)
	{
		allocs[i] = pool.alloc(1, 1);
		if (i % 2 == 0)
		{
			pool.free(allocs[i]);
		}
	}

	return pool._debugIsConform();
}


bool memoryRandomAllocsRandomAlign()
{
	constexpr uint64_t size = 1024 * 1024 * 1024;
	MemoryPool<size, 1000> pool;

	uint64_t allocs[1000];
	for (int i = 0; i < 1000; ++i)
	{
		uint64_t size = randNum(1, 515);
		uint64_t align = randNum(1, 128);
		allocs[i] = pool.alloc(size, align);
	}

	return pool._debugIsConform();
}


bool memoryRandomAllocsRandomAlignRandomFree()
{
	constexpr size_t allocationCount = 2000;
	constexpr uint64_t size = 1024 * 1024 * 1024;
	MemoryPool<size, allocationCount> pool;

	std::vector< uint64_t > allocs;
	allocs.reserve(allocationCount);
	for (int i = 0; i < allocationCount; ++i)
	{
		uint64_t size = randNum(1, 1024);
		uint64_t align = randNum(1, 512);
		allocs.push_back( pool.alloc(size, align) );

		auto numRan = randNum(1, (int)allocs.size());
		if (numRan == 0 || i % numRan)
		{
			auto allocToRemove = randNum(0, (int)allocs.size() - 1);
			pool.free(allocs[allocToRemove]);
			allocs.erase(allocs.begin() + allocToRemove);
		}
	}

	return pool._debugIsConform();
}

template<typename FUNC>
bool Test(FUNC f, const char* fctName)
{
	if (f())
	{
		printf("Test function %s passed.\n", fctName);
		return true;
	}
	else
	{
		printf("%s FAILED.\n", fctName);
		return false;
	}
}

#define TEST(x) Test( (x), #x );

int main()
{
	const auto seed = std::random_device()();
	std::cout << "using seed " << seed << std::endl;
	rng.seed(seed);

	bool success = true;
	for (int i = 0; i < 5000; ++i)
	{
		success &= TEST(memoryPoolAllocateAll);
		success &= TEST(memoryPoolAllocateAllAndDeallocateAll);
		success &= TEST(memoryAllocateDeallocateHalfTheTime);
		success &= TEST(memoryRandomAllocsRandomAlign);
		success &= TEST(memoryRandomAllocsRandomAlignRandomFree);
	}

	if (success)
	{
		std::cout << "All tests passed\n";
	}
	else
	{
		std::cout << "Some tests failed\n";
	}

	std::cout << "Press Enter to Continue";
	std::cin.ignore();
   return 0;
}