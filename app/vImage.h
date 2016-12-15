#ifndef _VIMAGE_H_
#define _VIMAGE_H_

#include "vkUtils.h"
#include "vMemoryPool.h"

class VImage
{
  public:
   VImage( VDeleter<VkDevice>& device );
   void setMemory( const VMemAlloc& newMem );
   const VMemAlloc& getMemory() const;
   VMemAlloc& getMemory();
   bool isAllocated() const;
   VkImage* operator&();
   operator VkImage();

  private:
   VDeleter<VkImage> _image;
   VMemAlloc _memAlloc;
};

#endif  // _VIMAGE_H_