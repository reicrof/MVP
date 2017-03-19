#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( binding = 0 ) uniform UniformBufferObject
{
   mat4 model;
   mat4 view;
   mat4 proj;
   vec3 color;
   float rough;
   float metal;
}
ubo;

layout( location = 0 ) in vec3 inPosition;
layout( location = 1 ) in vec3 inNormals;
layout( location = 2 ) in vec2 inTexCoord;

layout( location = 0 ) out vec3 fragColor;
layout( location = 1 ) out vec2 fragTexCoord;
layout( location = 2 ) out vec3 outNormal;
layout( location = 3 ) out vec3 lightPos;
layout( location = 4 ) out vec2 roughMetal;
layout( location = 5 ) out vec3 outColor;
layout( location = 6 ) out vec3 outCamPos;
layout( location = 7 ) out vec3 outWorldPos;

out gl_PerVertex
{
   vec4 gl_Position;
};

void main()
{
   gl_Position = ubo.proj * ubo.view * ubo.model * vec4( inPosition, 1.0 );
   fragTexCoord = inTexCoord;
   lightPos = vec3(5.0f, 5.0f, 5.0f);
   roughMetal = vec2( ubo.rough, ubo.metal );
   outColor = ubo.color;
   outCamPos = vec3( ubo.view[3] );
   outWorldPos = vec3( ubo.model * vec4( inPosition, 1.0 ) );
   outNormal = vec3( ubo.model * vec4( inNormals, 1.0 ) );;
}
