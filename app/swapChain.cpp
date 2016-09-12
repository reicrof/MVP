#include "swapChain.h"

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
}

SwapChain::SwapChain( const VkPhysicalDevice& physDevice, const VkSurfaceKHR& surface)
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
}