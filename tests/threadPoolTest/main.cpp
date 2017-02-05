#include <iostream>
#include <chrono>
#include <random>

#include "../../app/ThreadPool.h"


int main()
{
	constexpr size_t jobCount = 200000;
	std::vector< std::future<size_t> > res(jobCount);
	auto start = std::chrono::system_clock::now();
	{
		ThreadPool pool(std::thread::hardware_concurrency());

		std::mt19937 rng;
		rng.seed(123456);
		std::uniform_int_distribution<std::mt19937::result_type> ranNum(3000, 50000);

		for (int i = 0; i < jobCount; ++i)
		{
			res[i] = pool.addJob([&]() {
				volatile size_t sum = 0;
				for (size_t i = 0; i < ranNum(rng); ++i)
				{
					sum += i;
					auto a = std::acos(std::acos(std::cos(std::sqrt(sum))));
					auto b = std::sin(std::atan(std::asin(a)));
					sum += (a + b);
					sum /= 10;
				}
				return sum;
			});
		}
	}

	auto end = std::chrono::system_clock::now();
	int elapsed_seconds = std::chrono::duration_cast<std::chrono::milliseconds>
		(end - start).count();

	std::cout << "Time elapsed = " << elapsed_seconds << "ms\n";

	size_t finalRes = res[0].get();
	for (size_t i = 1; i < res.size(); ++i)
	{
		assert(res[i].get() == finalRes);
	}

	char a;
	std::cin >> a;
}