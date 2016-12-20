#include "vImage.h"

VImage::VImage( VDeleter<VkDevice>& device ) : _image( device, vkDestroyImage ), _memAlloc()
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