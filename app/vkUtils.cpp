#include "vkUtils.h"
#include "vMemoryPool.h"
#include "vCommandPool.h"

namespace
{
	VkAccessFlags vkImageLayoutToAccessFlags(VkImageLayout layout)
	{
		switch (layout)
		{
		case VK_IMAGE_LAYOUT_PREINITIALIZED:
			return VK_ACCESS_HOST_WRITE_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
			return VK_ACCESS_TRANSFER_READ_BIT;
		case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
			return VK_ACCESS_TRANSFER_WRITE_BIT;
		case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
			return VK_ACCESS_SHADER_READ_BIT;
		case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
			return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		case VK_IMAGE_LAYOUT_UNDEFINED:
			return 0;
		default:
			assert(!"Invalid img laout");
			return 0;
		}
	}
}

void VkUtils::endCmdBufferAndSubmit(VkCommandBuffer cmdBuf, VkQueue & queue, uint32_t waitSemCount, VkSemaphore * waitSem, uint32_t signalSemCount, VkSemaphore * signalSem, VkFence fenceToSignal)
{
	vkEndCommandBuffer(cmdBuf);

	VkSubmitInfo submitInfo = {
		VK_STRUCTURE_TYPE_SUBMIT_INFO,
		nullptr,
		waitSemCount,
		waitSem,
		nullptr,
		1,
		&cmdBuf,
		signalSemCount,
		signalSem,
	};

	vkQueueSubmit(queue, 1, &submitInfo, fenceToSignal);
}

VkCommandBuffer VkUtils::copyBuffer(VkCommandBuffer cmdBuffer, VkBuffer source, VkBuffer dest, VkDeviceSize size)
{
	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(cmdBuffer, source, dest, 1, &copyRegion);
	
	return cmdBuffer;
}

VMemAlloc VkUtils::createBuffer(VkDevice device,
	VMemoryManager& memoryManager,
	VkMemoryPropertyFlags memProperty,
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkBuffer& buffer)
{
	VkBufferCreateInfo bufferInfo = {};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VK_CALL(vkCreateBuffer(device, &bufferInfo, nullptr, &buffer));

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

	const VMemAlloc alloc = memoryManager.alloc(memRequirements, memProperty);

	vkBindBufferMemory(device, buffer, alloc.memory, alloc.offset);

	return alloc;
}

VkCommandBuffer VkUtils::transitionImgLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkCommandBuffer cmdBuffer)
{
	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	// Queue family ownership
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

	barrier.image = image;

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	barrier.subresourceRange = subresourceRange;
	
	barrier.srcAccessMask = vkImageLayoutToAccessFlags(oldLayout);
	barrier.dstAccessMask = vkImageLayoutToAccessFlags(newLayout);

	vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1,
		&barrier);
	
	return cmdBuffer;
}
