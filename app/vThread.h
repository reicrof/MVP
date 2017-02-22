#ifndef VTHREAD_H_
#define VTHREAD_H_

#include "vMemoryPool.h"
#include "vCommandPool.h"
#include "ThreadPool.h"

#include <thread>
#include <mutex>
#include <queue>
#include <functional>
#include <condition_variable>
#include <future>
#include <memory>
#include <utility>
#include <vulkan/vulkan.h>

class VThread
{
public:
	struct VThreadResources
	{
		VkDevice _device;
		VCommandPool _transferCommandPool;
		VCommandPool _graphicCommandPool;
		VMemoryManager _memoryManager;
		VkQueue _graphicQueue;
		VkQueue _transferQueue;
		uint32_t _graphicQueueFamilly;
		uint32_t _transferQueueFamilly;
	};

	VThread();
	void init(VkPhysicalDevice physDevice, VkDevice device);

	template <class F, class... Args>
	auto addJob(F&& f, Args&&... args)
	{
		auto jobTask =
			std::make_shared<std::packaged_task<typename std::result_of<F(VThreadResources*, Args...)>::type()> >(
				std::bind(std::forward<F>(f), &_resources, std::forward<Args>(args)...));

		auto futureRes = jobTask->get_future();

		_queue.addJob(std::forward<decltype(jobTask)>(jobTask));
		return futureRes;
	}

	VThreadResources& getThreadResources() {
		return _resources;
	}

private:
	std::thread _thread;
	ThreadPool::JobQueue _queue;
	// Vulkan resources
	VkPhysicalDevice _physicalDevice;
	VThreadResources _resources;
	bool _isDone = false;
};

#endif  // VTHREAD_H_
