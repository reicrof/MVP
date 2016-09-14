#include "swapChain.h"
#include "utils.h"
#include <limits>
#include <algorithm>
#include <assert.h>

namespace
{
	size_t selectDefaultSurfaceFormat( const std::vector< VkSurfaceFormatKHR >& formats )
	{
		for (size_t i = 0; i < formats.size(); ++i)
		{
			if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && formats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
			{
				return i;
			}
		}

		return 0;
	}

	size_t selectDefaultPresentMode(const std::vector< VkPresentModeKHR >& presentModes)
	{
		for (size_t i = 0; i < presentModes.size(); ++i)
		{
			if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR )
			{
				return i;
			}
		}

		return 0;	
	}

	VkExtent2D selectDefaultExtent( const VkExtent2D& curExt, const VkExtent2D& minExt, const VkExtent2D& maxExt)
	{
		if (curExt.width != std::numeric_limits<uint32_t>::max())
		{
			return curExt;
		}

		static const uint32_t WIDTH = 800;
		static const uint32_t HEIGHT = 600;
		VkExtent2D extent = { clamp(WIDTH, minExt.width, maxExt.width),
							  clamp(HEIGHT, minExt.height, maxExt.height) };
		return extent;
	}
}

SwapChain::SwapChain( const VkPhysicalDevice& physDevice, const VDeleter<VkDevice>& logicalDevice,
					  const VDeleter<VkSurfaceKHR>& surface, VkSharingMode sharingMode /*= VK_SHARING_MODE_EXCLUSIVE*/)
	: _handle{ logicalDevice, vkDestroySwapchainKHR }
{
	VkSurfaceCapabilitiesKHR capabilities = {};
	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice, surface, &capabilities);

	uint32_t surfaceFormatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &surfaceFormatCount, nullptr);
	_surfaceFormats.resize(surfaceFormatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice, surface, &surfaceFormatCount, _surfaceFormats.data());

	// If no preffered format, set a default one to be used.
	if (surfaceFormatCount == 1 && _surfaceFormats[0].format == VK_FORMAT_UNDEFINED)
	{
		_surfaceFormats[0] = { VK_FORMAT_B8G8R8A8_UNORM, VK_COLORSPACE_SRGB_NONLINEAR_KHR };
	}

	_selectedSurfaceFormat = selectDefaultSurfaceFormat(_surfaceFormats);

	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &presentModeCount, nullptr);
	_presentModes.resize(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(physDevice, surface, &presentModeCount, _presentModes.data());

	_selectedPresentMode = selectDefaultPresentMode(_presentModes);

	_curExtent = selectDefaultExtent(capabilities.currentExtent, capabilities.minImageExtent, capabilities.maxImageExtent);
	_minExtent = capabilities.minImageExtent;
	_maxExtent = capabilities.maxImageExtent;

	_imageCount = capabilities.minImageCount + 1;
	if (capabilities.maxImageCount > 0 && 
		_imageCount > capabilities.maxImageCount)
	{
		_imageCount = capabilities.maxImageCount;
	}

	// The actual creation of the swap chain
	VkSwapchainCreateInfoKHR createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = surface;
	createInfo.minImageCount = _imageCount;
	createInfo.imageFormat = _surfaceFormats[ _selectedSurfaceFormat ].format;
	createInfo.imageColorSpace = _surfaceFormats[_selectedSurfaceFormat].colorSpace;
	createInfo.presentMode = _presentModes[_selectedPresentMode];
	createInfo.imageExtent = _curExtent;
	createInfo.imageArrayLayers = 1; // Stereoscopic stuff
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

	assert(sharingMode == VK_SHARING_MODE_EXCLUSIVE);
	createInfo.imageSharingMode = sharingMode;
	createInfo.queueFamilyIndexCount = 0;
	createInfo.pQueueFamilyIndices = nullptr;

	createInfo.preTransform = capabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	VK_CALL(vkCreateSwapchainKHR(logicalDevice, &createInfo, nullptr, &_handle));
}