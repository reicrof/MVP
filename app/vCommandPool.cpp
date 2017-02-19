#include "vCommandPool.h"

VCommandPool::VCommandPool() : _device( VK_NULL_HANDLE ), _nextFreeIdx( 0 ), _queueFamilly( -1 )
{
}

void VCommandPool::init( VkDevice device,
                         uint32_t size,
                         VkCommandPoolCreateFlags flag,
                         uint32_t queueFamilyIndex )
{
   _device = device;
   _commandBuffers.resize( size );
   _queueFamilly = queueFamilyIndex;
   // Create the VkPool first
   VkCommandPoolCreateInfo poolInfo = {};
   poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   poolInfo.queueFamilyIndex = _queueFamilly;
   poolInfo.flags = flag;

   VK_CALL( vkCreateCommandPool( _device, &poolInfo, nullptr, &_commandPool ) );

   // Allocate requested count of commands upfront
   VkCommandBufferAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandPool = _commandPool;
   allocInfo.commandBufferCount = size;

   vkAllocateCommandBuffers( _device, &allocInfo, _commandBuffers.data() );
}

VkCommandBuffer VCommandPool::alloc( VkCommandBufferUsageFlagBits flag )
{
   static VkCommandBufferBeginInfo beginInfo = {};
   beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   beginInfo.flags = flag;

   if ( _nextFreeIdx >= _commandBuffers.size() )
   {
      expandCommandBuffers( _commandBuffers.size() );
   }
   const size_t cmdBuffIdx = _nextFreeIdx++;

   vkBeginCommandBuffer( _commandBuffers[ cmdBuffIdx ], &beginInfo );
   return _commandBuffers[ cmdBuffIdx ];
}

void VCommandPool::free( VkCommandBuffer& cmdBuffer, VkCommandPoolResetFlags flag /*= 0*/ )
{
   for ( size_t i = 0; i < _commandBuffers.size(); ++i )
   {
      if ( _commandBuffers[ i ] == cmdBuffer )
      {
         std::swap( _commandBuffers[ i ], _commandBuffers[ --_nextFreeIdx ] );
         vkResetCommandBuffer( cmdBuffer, flag );
         return;
      }
   }
}

void VCommandPool::freeAll( VkCommandPoolResetFlags flag /*= 0*/ )
{
   vkResetCommandPool( _device, _commandPool, flag );
   _nextFreeIdx = 0;
}

void VCommandPool::expandCommandBuffers( size_t size )
{
   VkCommandBufferAllocateInfo allocInfo = {};
   allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   allocInfo.commandPool = _commandPool;
   allocInfo.commandBufferCount = static_cast<uint32_t>( size );

   const size_t oldSize = _commandBuffers.size();
   _commandBuffers.resize( oldSize + size );
   vkAllocateCommandBuffers( _device, &allocInfo, &_commandBuffers[ oldSize ] );
}

VCommandPool::~VCommandPool()
{
   vkFreeCommandBuffers( _device, _commandPool, static_cast<uint32_t>( _commandBuffers.size() ),
                         _commandBuffers.data() );
   _commandBuffers.clear();
   vkDestroyCommandPool( _device, _commandPool, nullptr );
}
