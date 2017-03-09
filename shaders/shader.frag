#version 450
#extension GL_ARB_separate_shader_objects : enable

layout( location = 0 ) in vec3 fragColor;
layout( location = 1 ) in vec2 fragTexCoord;
layout( location = 2 ) in vec3 normal;
layout( location = 3 ) in vec3 lightPos;
layout( location = 4 ) in vec2 roughMetal;
layout( location = 5 ) in vec3 inColor;
layout( location = 6 ) in vec3 camPos;
layout( location = 7 ) in vec3 inWorldPos;

layout( binding = 1 ) uniform sampler2D texSampler;

layout( location = 0 ) out vec4 outColor;


const float PI = 3.14159265359;

//#define ROUGHNESS_PATTERN 1

// Normal Distribution function --------------------------------------
float D_GGX( float dotNH, float roughness )
{
   float alpha = roughness * roughness;
   float alpha2 = alpha * alpha;
   float denom = dotNH * dotNH * ( alpha2 - 1.0 ) + 1.0;
   return ( alpha2 ) / ( PI * denom * denom );
}

// Geometric Shadowing function --------------------------------------
float G_SchlickmithGGX( float dotNL, float dotNV, float roughness )
{
   float r = ( roughness + 1.0 );
   float k = ( r * r ) / 8.0;
   float GL = dotNL / ( dotNL * ( 1.0 - k ) + k );
   float GV = dotNV / ( dotNV * ( 1.0 - k ) + k );
   return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick( float cosTheta, float metallic )
{
   vec3 F0 = mix( vec3( 0.04 ), inColor, metallic );  // * material.specular
   vec3 F = F0 + ( 1.0 - F0 ) * pow( 1.0 - cosTheta, 5.0 );
   return F;
}

// Specular BRDF composition --------------------------------------------

vec3 BRDF( vec3 L, vec3 V, vec3 N, float metallic, float roughness )
{
   // Precalculate vectors and dot products
   vec3 H = normalize( V + L );
   float dotNV = clamp( dot( N, V ), 0.0, 1.0 );
   float dotNL = clamp( dot( N, L ), 0.0, 1.0 );
   float dotLH = clamp( dot( L, H ), 0.0, 1.0 );
   float dotNH = clamp( dot( N, H ), 0.0, 1.0 );

   // Light color fixed
   vec3 lightColor = vec3( 1.0 );

   vec3 color = vec3( 0.0 );

   if ( dotNL > 0.0 )
   {
      float rroughness = max( 0.05, roughness );
      // D = Normal distribution (Distribution of the microfacets)
      float D = D_GGX( dotNH, roughness );
      // G = Geometric shadowing term (Microfacets shadowing)
      float G = G_SchlickmithGGX( dotNL, dotNV, roughness );
      // F = Fresnel factor (Reflectance depending on angle of incidence)
      vec3 F = F_Schlick( dotNV, metallic );

      vec3 spec = D * F * G / ( 4.0 * dotNL * dotNV );

      color += spec * dotNL * lightColor;
   }

   return color;
}



void main()
{
   vec3 N = normalize( normal );
   vec3 V = normalize( camPos - inWorldPos );

   // Specular contribution
   vec3 Lo = vec3( 0.0 );

   vec3 L = normalize( lightPos.xyz - inWorldPos );
   Lo += BRDF( L, V, N, roughMetal.y, roughMetal.x );

   // Combine with ambient
   vec3 color = inColor * 0.02;
   color += Lo;

   // Gamma correct
   color = pow( color, vec3( 0.4545 ) );

   outColor = vec4( color, 1.0 );
}
