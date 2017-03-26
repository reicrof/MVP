#ifndef _VIMAGE_H_
#define _VIMAGE_H_

#include "vkUtils.h"
#include "vMemoryPool.h"
#include <string>
#include <memory>

#include <gli/gli.hpp>

class VImage
{
  public:
   VImage( VDeleter<VkDevice>& device );
   ~VImage();
   void setMemory( const VMemAlloc& newMem );
   const VMemAlloc& getMemory() const;
   VMemAlloc& getMemory();
   bool isAllocated() const;
   VkImage* operator&();
   operator VkImage();

   uint64_t _size;
   size_t _width;
   size_t _height;
   size_t _mips;
   VkFormat _format;

   static bool load2DTexture( const std::string& path,
                              VImage& img,
                              VkDevice device,
                              VkFormat format,
                              VkImageTiling tiling,
                              VkImageUsageFlags usage,
                              VMemoryManager& memoryManager,
                              VCommandPool& cmdPool,
                              VkQueue queue );
   static bool loadCubeTexture( const std::string& path,
                                VImage& img,
                                VkDevice device,
                                VkFormat format,
                                VkImageTiling tiling,
                                VkImageUsageFlags usage,
                                VMemoryManager& memoryManager,
                                VCommandPool& cmdPool,
                                VkQueue queue );



   VDeleter<VkImage> _image;
   VDeleter<VkImageView> _imageView;
   VDeleter<VkSampler> _sampler;

  private:
   VMemAlloc _memAlloc;
   std::unique_ptr<gli::texture> _gliTex;
};

#endif  // _VIMAGE_H_