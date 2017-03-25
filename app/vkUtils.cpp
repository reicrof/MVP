#include "vkUtils.h"
#include "vMemoryPool.h"
#include "vCommandPool.h"
#include "vImage.h"

#include <fstream>

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

void VkUtils::createImage(
    VkDevice device,
    VMemoryManager& memoryManager,
    uint32_t width,
    uint32_t height,
    uint32_t mips,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VkMemoryPropertyFlags memProperty,
    VImage& img)
{
    VkImageCreateInfo imageInfo = {};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mips;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0;  // Optional

    VK_CALL(vkCreateImage(device, &imageInfo, nullptr, &img));

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, img, &memRequirements);

    // Free the image memory if it was already allocated
    if (img.isAllocated())
    {
        memoryManager.free(img.getMemory());
    }

    img.setMemory(memoryManager.alloc(memRequirements, memProperty));

    VK_CALL(
        vkBindImageMemory(device, img, img.getMemory().memory, img.getMemory().offset));
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

VkShaderModule VkUtils::createShaderModule(VkDevice device, const std::string& path)
{
	std::ifstream shaderFile{ path, std::ios::binary };
	std::vector<char> shaderSource(std::istreambuf_iterator<char>{shaderFile},
		std::istreambuf_iterator<char>{});

    VkShaderModule shaderModule = VK_NULL_HANDLE;
	if (shaderSource.empty())
	{
		std::cerr << "Cannot find shader : " << path << std::endl;
		return shaderModule;
	}

	VkShaderModuleCreateInfo shaderModuleCreateInfo = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, nullptr, 0, shaderSource.size(),
		reinterpret_cast<const uint32_t*>(shaderSource.data()) };

	VK_CALL(vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule) );

    return shaderModule;
}

VkDescriptorSetLayout VkUtils::createDescriptorSetLayout(VkDevice device, const std::vector<VkDescriptorSetLayoutBinding>& bindings)
{
    VkDescriptorSetLayoutCreateInfo layoutInfo = {};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VkDescriptorSetLayout descSetLayout = VK_NULL_HANDLE;
    VK_CALL(vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descSetLayout));
    return descSetLayout;
}

VkWriteDescriptorSet VkUtils::createWriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t dstArrayEl, const VkDescriptorBufferInfo* bufInfo, uint32_t bufElCount)
{
    VkWriteDescriptorSet descriptorWrites = {};
    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = dstSet;
    descriptorWrites.dstBinding = dstBinding;
    descriptorWrites.dstArrayElement = dstArrayEl;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites.descriptorCount = bufElCount;
    descriptorWrites.pBufferInfo = bufInfo;

    return descriptorWrites;
}

VkWriteDescriptorSet VkUtils::createWriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, uint32_t dstArrayEl, const VkDescriptorImageInfo * imgInfos, uint32_t imgInfosCount)
{
    VkWriteDescriptorSet descriptorWrites = {};
    descriptorWrites.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites.dstSet = dstSet;
    descriptorWrites.dstBinding = dstBinding;
    descriptorWrites.dstArrayElement = dstArrayEl;
    descriptorWrites.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites.descriptorCount = imgInfosCount;
    descriptorWrites.pImageInfo = imgInfos;

    return descriptorWrites;
}
