#include "vThread.h"

VThread::VThread()
{
	_thread = std::move( std::thread([this]()
	{
		while (true)
		{
			ThreadPool::Job job;
			{
				if (!_queue.getJob(job))
					return; // We are done
			}

			// We should have a job if we get here.
			assert(job);

			job();
		}
	}
	));
}

void VThread::init(VkPhysicalDevice physDevice, VkDevice device)
{
	_physicalDevice = physDevice;
	_resources._device = device;

	_resources._transferCommandPool.init(_resources._device, 5, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, _resources._transferQueueFamilly );
	_resources._graphicCommandPool.init(_resources._device, 5, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT, _resources._graphicQueueFamilly);
	_resources._memoryManager.init(_physicalDevice, _resources._device);
}