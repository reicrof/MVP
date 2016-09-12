#ifndef SWAP_CHAIN_H_
#define SWAP_CHAIN_H_
#include <vector>
#include <vulkan\vulkan.h>

class SwapChain
{
public:
	SwapChain( const VkPhysicalDevice& physDevice, const VkSurfaceKHR& surface );
	~SwapChain() = default;

	std::vector< VkSurfaceFormatKHR > _surfaceFormats;
	size_t _selectedSurfaceFormat = 0;

	std::vector< VkPresentModeKHR > _presentModes;
	size_t _selectedPresentMode = 0;
};

#endif // SWAP_CHAIN_H_
