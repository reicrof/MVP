#ifndef _VCOMMAND_POOL_H_
#define _VCOMMAND_POOL_H_

#include "vkUtils.h"
#include <vector>

class VCommandPool
{
  public:
   VCommandPool( VDeleter<VkDevice>& device,
                 uint32_t size );

   void init(VkCommandPoolCreateFlags flag, uint32_t queueFamilyIndex);
   VkCommandBuffer* alloc(VkCommandBufferUsageFlagBits flag);
   void free(VkCommandBuffer& cmdBuffer);

  private:
   VDeleter<VkDevice>& _device;
   VDeleter<VkCommandPool> _commandPool;
   std::vector<VkCommandBuffer> _commandBuffers;
   std::vector<bool> _freeCmdBuffers;
   const uint32_t _size;
   uint32_t _queue;
};

#endif  //_VCOMMAND_POOL_H_