#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;
layout( location = 2 ) in vec3 normal;
layout( location = 3 ) in vec3 lightPos;

layout(binding = 1) uniform sampler2D texSampler;

layout(location = 0) out vec4 outColor;


float lambert(vec3 N, vec3 L)
{
  vec3 nrmN = normalize(N);
  vec3 nrmL = normalize(L);
  float result = dot(nrmN, nrmL);
  return max(result, 0.0);
}

void main() {
      vec3 result = vec3(1.0f,1.0f,1.0f) * lambert(normal, lightPos);
      outColor = vec4(result,1.0f);
    //outColor = texture(texSampler, fragTexCoord);
}