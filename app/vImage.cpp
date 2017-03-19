#include "vImage.h"
#include "vCommandPool.h"

VImage::VImage( VDeleter<VkDevice>& device ) :
	_image( device, vkDestroyImage ),
	_imageView( device, vkDestroyImageView ),
	_sampler( device, vkDestroySampler ),
	_memAlloc()
{
}

const VMemAlloc& VImage::getMemory() const
{
   return _memAlloc;
}

VMemAlloc& VImage::getMemory()
{
   return _memAlloc;
}

void VImage::setMemory( const VMemAlloc& newMem )
{
   _memAlloc = newMem;
}

bool VImage::isAllocated() const
{
   return _memAlloc.memory != VK_NULL_HANDLE;
}

VkImage* VImage::operator&()
{
   return &_image;
}

VImage::operator VkImage()
{
   return _image;
}

VImage::~VImage()
{
}

bool VImage::load2DTexture(const std::string& path, VImage& img, VkDevice device, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VMemoryManager& memoryManager, VCommandPool& cmdPool, VkQueue queue)
{
	img._gliTex.reset( new gli::texture2d(gli::load(path) ) );

	// Copy meta data ( could probably be removed later..)
	img._width = img._gliTex->extent().x;
	img._height = img._gliTex->extent().y;
	img._mips = img._gliTex->levels();
	img._format = format;
	img._size = img._gliTex->size();

	VDeleter<VkBuffer> stagingBuffer{ device, vkDestroyBuffer };
	VMemAlloc hostBuffer = VkUtils::createBuffer( device, memoryManager, 
		    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			img._size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, *stagingBuffer.get() );

	void* data;
	VK_CALL(vkMapMemory(device, hostBuffer.memory, hostBuffer.offset, img._size, 0, &data));
	memcpy(data, img._gliTex->data(), img._size);
	vkUnmapMemory(device, hostBuffer.memory);

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = img._width;
	imageInfo.extent.height = img._height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = img._mips;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = 0;  // Optional

	VK_CALL(vkCreateImage(device, &imageInfo, nullptr, &img._image));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, img._image, &memRequirements);

	img._memAlloc = memoryManager.alloc( memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT );

	VK_CALL(
		vkBindImageMemory(device, img._image, img._memAlloc.memory, img._memAlloc.offset ) );

	VkCommandBuffer commandBuffer = cmdPool.alloc(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// Transition to optimal destination
	VkUtils::transitionImgLayout(img, img._format, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);

	// Copy
	VkBufferImageCopy bufferCopyRegion = {};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = img._mips;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = img._width;
	bufferCopyRegion.imageExtent.height = img._height;
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	VkCommandBuffer cmdBuf = cmdPool.alloc(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
	vkCmdCopyBufferToImage(cmdBuf, stagingBuffer, img._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

	// Transition to optimal shader read
	VkUtils::transitionImgLayout(img, img._format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);

	VkUtils::endCmdBufferAndSubmit(cmdBuf, queue);

	// Create image view
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkImageViewCreateInfo viewInfo = {};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = img;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = format;
	viewInfo.subresourceRange = subresourceRange;

	VK_CALL(vkCreateImageView(device, &viewInfo, nullptr, &img._imageView));

	// Create sampler
	VkSamplerCreateInfo samplerInfo = {};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 0.0f;

	VK_CALL(vkCreateSampler(device, &samplerInfo, nullptr, &img._sampler));

	return true;
}

bool VImage::loadCubeTexture(const std::string& path, VImage& img, VkDevice device, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VMemoryManager& memoryManager, VCommandPool& cmdPool, VkQueue queue)
{
	img._gliTex.reset(new gli::texture_cube(gli::load(path)));

	// Copy meta data ( could probably be removed later..)
	img._width = img._gliTex->extent().x;
	img._height = img._gliTex->extent().y;
	img._mips = img._gliTex->levels();
	img._format = format;
	img._size = img._gliTex->size();

	VDeleter<VkBuffer> stagingBuffer{ device, vkDestroyBuffer };
	VMemAlloc hostBuffer = VkUtils::createBuffer(device, memoryManager,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		img._size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, *stagingBuffer.get());

	void* data;
	VK_CALL(vkMapMemory(device, hostBuffer.memory, hostBuffer.offset, img._size, 0, &data));
	memcpy(data, img._gliTex->data(), img._size);
	vkUnmapMemory(device, hostBuffer.memory);

	// Create cube map copy info
	std::vector<VkBufferImageCopy> bufferCopyRegions;
	size_t offset = 0;

	const gli::texture_cube& cubeTex = static_cast< gli::texture_cube >( *img._gliTex );
	for (uint32_t face = 0; face < 6; ++face)
	{
		for (uint32_t level = 0; level < img._mips; ++level)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = level;
			bufferCopyRegion.imageSubresource.baseArrayLayer = face;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(cubeTex[face][level].extent().x);
			bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(cubeTex[face][level].extent().y);
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);

			// Increase offset into staging buffer for next level / face
			offset += cubeTex[face][level].size();
		}
	}

	VkImageCreateInfo imageInfo = {};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = img._width;
	imageInfo.extent.height = img._height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = img._mips;
	// Cube has 6 faces
	imageInfo.arrayLayers = 6;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageInfo.usage = usage;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	VK_CALL(vkCreateImage(device, &imageInfo, nullptr, &img._image));

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(device, img._image, &memRequirements);

	img._memAlloc = memoryManager.alloc(memRequirements, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	VK_CALL(
		vkBindImageMemory(device, img._image, img._memAlloc.memory, img._memAlloc.offset));

	VkCommandBuffer commandBuffer = cmdPool.alloc(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// Transition to optimal destination
	VkUtils::transitionImgLayout(img, img._format, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, commandBuffer);

	// Copy
	VkBufferImageCopy bufferCopyRegion = {};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = img._mips;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = img._width;
	bufferCopyRegion.imageExtent.height = img._height;
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, img._image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, static_cast<uint32_t>(bufferCopyRegions.size()), bufferCopyRegions.data());

	// Transition to optimal shader read
	VkUtils::transitionImgLayout(img, img._format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, commandBuffer);

	VkUtils::endCmdBufferAndSubmit(commandBuffer, queue);

	// Create image view
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.baseArrayLayer = 0;
	subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
	subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewCreateInfo.format = img._format;
	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	viewCreateInfo.subresourceRange.layerCount = 6;
	viewCreateInfo.subresourceRange.levelCount = img._mips;
	viewCreateInfo.image = img;

	VK_CALL(vkCreateImageView(device, &viewCreateInfo, nullptr, &img._imageView));

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
	samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.maxAnisotropy = 8;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = (float)img._mips;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

	VK_CALL(vkCreateSampler(device, &samplerCreateInfo, nullptr, &img._sampler));

	vkDeviceWaitIdle(device);

	return true;
}