#include "vCommandPool.h"

VCommandPool::VCommandPool( VDeleter<VkDevice>& device, uint32_t size )
    : _device( device ),
      _commandPool( device, vkDestroyCommandPool ),
      _freeCmdBuffers( size, true ),
      _size( size )
{
   _commandBuffers.resize( size );
}

void VCommandPool::init( VkCommandPoolCreateFlags flag, uint32_t queueFamilyIndex )
{
   _queue = queueFamilyIndex;
   // Create the VkPool first
   VkCommandPoolCreateInfo poolInfo = {};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.queueFamilyIndex = _queue;
   poolInfo.flags = flag;

   VK_CALL( vkCreateCommandPool( _device, &poolInfo, nullptr, &_commandPool ) );

   // Allocate requested count of commands upfront
   VkCommandBufferAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandPool = _commandPool;
   allocInfo.commandBufferCount = _size;

   vkAllocateCommandBuffers( _device, &allocInfo, _commandBuffers.data() );
}

VkCommandBuffer* VCommandPool::alloc( VkCommandBufferUsageFlagBits flag )
{
   static VkCommandBufferBeginInfo beginInfo = {};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = flag;

   for ( size_t i = 0; i < _size; ++i )
   {
      if ( _freeCmdBuffers[ i ] )
      {
         _freeCmdBuffers[ i ] = false;

         vkBeginCommandBuffer( _commandBuffers[ i ], &beginInfo );
         return &_commandBuffers[ i ];
      }
   }

   assert( false );
   return nullptr;
}

void VCommandPool::free( VkCommandBuffer& cmdBuffer )
{
   for ( size_t i = 0; i < _size; ++i )
   {
      if ( _commandBuffers[ i ] == cmdBuffer )
      {
         _freeCmdBuffers[ i ] = true;
         vkResetCommandBuffer( cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT );
         return;
      }
   }
}
