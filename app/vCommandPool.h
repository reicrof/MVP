#ifndef _VCOMMAND_POOL_H_
#define _VCOMMAND_POOL_H_

#include "vkUtils.h"
#include <vector>

class VCommandPool
{
  public:
   VCommandPool();
   ~VCommandPool();

   void init( VkDevice device,
              uint32_t size,
              VkCommandPoolCreateFlags flag,
              uint32_t queueFamilyIndex );
   VkCommandBuffer alloc( VkCommandBufferUsageFlagBits flag );
   void free( VkCommandBuffer& cmdBuffer, VkCommandPoolResetFlags flag = 0 );
   void freeAll( VkCommandPoolResetFlags flag = 0 );

  private:
   void expandCommandBuffers( size_t size );

   VkDevice _device;
   VkCommandPool _commandPool;
   std::vector<VkCommandBuffer> _commandBuffers;
   size_t _nextFreeIdx;
   uint32_t _queueFamilly;
};

#endif  //_VCOMMAND_POOL_H_