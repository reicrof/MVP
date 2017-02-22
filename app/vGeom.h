#ifndef _VGEOM_H_H
#define _VGEOM_H_H

#include "vkUtils.h"
#include <vulkan/vulkan.h>

class VGeom
{
public:
	VkBuffer _vertexBuffer;
	VkBuffer _indexBuffer;

   uint32_t _verticesCount = 0;
   uint32_t _indexCount = 0;
};

#endif  // _VGEOM_H_H