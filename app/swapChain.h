#ifndef SWAP_CHAIN_H_
#define SWAP_CHAIN_H_
#include <vector>
#include <vulkan\vulkan.h>

class SwapChain
{
public:
	SwapChain( const VkPhysicalDevice& physDevice, const VkDevice& logicalDevice,
			   const VkSurfaceKHR& surface, VkSharingMode sharingMode = VK_SHARING_MODE_EXCLUSIVE );
	~SwapChain();

	VkSwapchainKHR _handle;
	VkExtent2D _curExtent;
	VkExtent2D _minExtent;
	VkExtent2D _maxExtent;

	std::vector< VkSurfaceFormatKHR > _surfaceFormats;
	size_t _selectedSurfaceFormat = 0;

	std::vector< VkPresentModeKHR > _presentModes;
	size_t _selectedPresentMode = 0;

	uint32_t _imageCount;
private:
	const VkDevice& _deviceUsedForCreation;
};

#endif // SWAP_CHAIN_H_
