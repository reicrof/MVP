#include <app/MemoryPool.h>
#include <app/ThreadPool.h>
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

static constexpr int POSSIBLE_ALIGNMENT[] = { 2, 4, 8, 16, 32, 64, 128, 512, 1024, 2048, 4096 };
static constexpr int ALIGNMENT_COUNT = sizeof(POSSIBLE_ALIGNMENT) / sizeof(POSSIBLE_ALIGNMENT[0]);

bool memoryPoolAllocateAll()
{
	constexpr uint64_t size = 1024;
	MemoryPool pool(size, 1024);

	uint64_t allocs[size];
	for (int i = 0; i < size/2; ++i)
	{
		allocs[i] = pool.alloc(1, 2);
	}

	return pool._debugIsConform();
}

bool memoryPoolAllocateAllAndDeallocateAll()
{
	constexpr uint64_t size = 1024;
	MemoryPool pool(size, 1024);

	uint64_t allocs[size];
	for (int i = 0; i < size / 2; ++i)
	{
		allocs[i] = pool.alloc(1, 2);
	}
	for (int i = 0; i < size / 2; ++i)
	{
		pool.free(allocs[i]);
	}
	for (int i = 0; i < size / 2; ++i)
	{
		allocs[i] = pool.alloc(1, 2);
	}
	for (int i = (size / 2) -1; i > -1 ; --i)
	{
		pool.free(allocs[i]);
	}

	return pool._debugIsConform();
}

bool memoryAllocateDeallocateHalfTheTime()
{
	constexpr uint64_t size = 1024;
	MemoryPool pool(size, 1024);

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
	MemoryPool pool(size, 1000);

	uint64_t allocs[1000];
	for (int i = 0; i < 1000; ++i)
	{
		uint64_t size = randNum(1, 515);
		uint64_t align = randNum(0, ALIGNMENT_COUNT-1);
		allocs[i] = pool.alloc(size, POSSIBLE_ALIGNMENT[align]);
	}

	return pool._debugIsConform();
}


bool memoryRandomAllocsRandomAlignRandomFree()
{
	constexpr size_t allocationCount = 2000;
	constexpr uint64_t size = 1024 * 1024 * 1024;
	MemoryPool pool(size, allocationCount);

	std::vector< uint64_t > allocs;
	allocs.reserve(allocationCount);
	for (int i = 0; i < allocationCount; ++i)
	{
		uint64_t size = randNum(1, 1024);
		uint64_t align = randNum(0, ALIGNMENT_COUNT-1);
		allocs.push_back( pool.alloc(size, POSSIBLE_ALIGNMENT[align]) );

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

bool memoryExactFit()
{
	constexpr uint64_t size = 1024;
	MemoryPool pool(size, 1024);

	size_t offset = pool.alloc(1024, 64);
	pool.free(offset);

	for (size_t i = 0; i < 1000; ++i)
	{
		size_t smallAlloc = randNum(1, size/2);
		size_t bigAllocAllignId = randNum(0, ALIGNMENT_COUNT - 4);
		size_t bigAllocAlign = POSSIBLE_ALIGNMENT[bigAllocAllignId];
		size_t alignLeft = (bigAllocAlign - (smallAlloc & (bigAllocAlign - 1))) & (bigAllocAlign-1);
		size_t alloc1 = pool.alloc(smallAlloc, 32);
		size_t alloc2 = pool.alloc(size - smallAlloc - alignLeft, bigAllocAlign);

		assert(pool.spaceLeft() == 0);
		pool.free(alloc2);
		pool.free(alloc1);
	}

	return pool._debugIsConform();
}

bool threadPoolTest()
{
	ThreadPool pool(std::thread::hardware_concurrency());
	for (int i = 0; i < 100; ++i)
	{
		pool.addJob([]() {
			std::this_thread::sleep_for(std::chrono::microseconds(300));
			std::cout << "thread done" << std::endl;
		});
	}

	return true;
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

	bool success = true;
	for (int i = 0; i < 5000; ++i)
	{
		success &= TEST(memoryPoolAllocateAll);
		success &= TEST(memoryPoolAllocateAllAndDeallocateAll);
		success &= TEST(memoryAllocateDeallocateHalfTheTime);
		success &= TEST(memoryRandomAllocsRandomAlign);
		success &= TEST(memoryRandomAllocsRandomAlignRandomFree);
		success &= TEST(memoryExactFit);
		success &= TEST(threadPoolTest);
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