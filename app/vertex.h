#ifndef VERTEX_H_
#define VERTEX_H_

#include <vulkan/vulkan.h>
#include "../thirdParties/glm/glm.hpp"

#include <array>

struct Vertex
{
   glm::vec3 pos;
   glm::vec3 color;
   glm::vec2 texCoord;

   static VkVertexInputBindingDescription getBindingDescription()
   {
      VkVertexInputBindingDescription bindingDescription = {};
      bindingDescription.binding = 0;
      bindingDescription.stride = sizeof( Vertex );
      bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
      return bindingDescription;
   }

   static auto getAttributeDescriptions()
   {
      // Vertex
      std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
      attributeDescriptions[ 0 ].binding = 0;
      attributeDescriptions[ 0 ].location = 0;
      attributeDescriptions[ 0 ].format = VK_FORMAT_R32G32B32_SFLOAT;
      attributeDescriptions[ 0 ].offset = offsetof( Vertex, pos );

      // Color
      attributeDescriptions[ 1 ].binding = 0;
      attributeDescriptions[ 1 ].location = 1;
      attributeDescriptions[ 1 ].format = VK_FORMAT_R32G32B32_SFLOAT;
      attributeDescriptions[ 1 ].offset = offsetof( Vertex, color );

      // Texture Coords
      attributeDescriptions[ 2 ].binding = 0;
      attributeDescriptions[ 2 ].location = 2;
      attributeDescriptions[ 2 ].format = VK_FORMAT_R32G32_SFLOAT;
      attributeDescriptions[ 2 ].offset = offsetof( Vertex, texCoord );

      return attributeDescriptions;
   }
};

#endif  // VERTEX_H_
