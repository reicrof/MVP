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
layout( binding = 2 ) uniform samplerCube irradianceSampler;
layout( binding = 3 ) uniform samplerCube radianceSampler;

layout( location = 0 ) out vec4 outColor;


const float PI = 3.14159265359;

float ggx( float rough, vec3 n, vec3 h )
{
   float alpha2 = ( rough * rough ) * ( rough * rough );
   float NdotH = clamp( dot( n, h ), 0.0, 1.0 );
   // float NdotH = dot( n, h );
   float ndothalpha = ( ( NdotH * NdotH ) * ( alpha2 - 1 ) ) + 1;
   float denom = PI * ( ndothalpha * ndothalpha );

   return alpha2 / denom;
}

float schlick( float roughness, vec3 n, vec3 v, vec3 l )
{
   float k = ( ( roughness + 1 ) * ( roughness + 1 ) ) / 8.0;
   float nDotV = clamp( dot( n, v ), 0.0, 1.0 );
   float nDotL = clamp( dot( n, l ), 0.0, 1.0 );
   float G1v = nDotV / ( nDotV * ( 1 - k ) + k );
   float G1l = nDotL / ( nDotL * ( 1 - k ) + k );

   return G1v * G1l;
}

float fresnel( float F0, vec3 v, vec3 h )
{
   float VdotH = clamp( dot( v, h ), 0.0, 1.0 );
   float exp = -5.55473 * VdotH * -6.98316 * VdotH;
   return F0 + ( 1.0 - F0 ) * pow( 2, exp );
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
      float D = ggx( roughness, N, H );
      // G = Geometric shadowing term (Microfacets shadowing)
      float G = schlick( roughness, N, V, L );
      // F = Fresnel factor (Reflectance depending on angle of incidence)
      float F = fresnel( metallic, V, H );

      // vec3 spec = D * F * G / ( 4.0 * dotNL * dotNV );
      float spec = D * F * G / ( 4.0 * dotNL * dotNV );

      color += spec * dotNL * lightColor;
   }

   return color;
}


// Environment BRDF approximation from
// https://www.unrealengine.com/blog/physically-based-shading-on-mobile
vec3 EnvBRDFApprox( vec3 SpecularColor, float Roughness, float NoV )
{
   vec4 c0 = vec4( -1, -0.0275, -0.572, 0.022 );
   vec4 c1 = vec4( 1, 0.0425, 1.04, -0.04 );
   vec4 r = Roughness * c0 + c1;
   float a004 = min( r.x * r.x, exp2( -9.28 * NoV ) ) * r.x + r.y;
   vec2 AB = vec2( -1.04, 1.04 ) * a004 + r.zw;
   return SpecularColor * AB.x + AB.y;
}

// From http://filmicgames.com/archives/75
vec3 Uncharted2Tonemap( vec3 x )
{
   float A = 0.15;
   float B = 0.50;
   float C = 0.10;
   float D = 0.20;
   float E = 0.02;
   float F = 0.30;
   return ( ( x * ( A * x + C * B ) + D * E ) / ( x * ( A * x + B ) + D * F ) ) - E / F;
}

void main()
{
   vec3 N = normalize( normal );
   vec3 V = normalize( camPos - inWorldPos );
   vec3 R = reflect( -V, N );

   // // Specular contribution
   // vec3 Lo = vec3( 0.0 );

   // vec3 L = normalize( lightPos.xyz - inWorldPos );
   // Lo += BRDF( L, V, N, roughMetal.y, roughMetal.x );

   // // Combine with ambient
   // vec3 color = inColor * 0.02;
   // color += Lo;

   // // Gamma correct
   // color = pow( color, vec3( 0.4545 ) );

   // outColor = vec4( color, 1.0 );

   float specular = 0.8;
   float exposure = 10.0;
   float gamma = 2.2;
   vec3 specularColor = mix( vec3( specular ), inColor, roughMetal.y );
   vec3 diffuseColor = inColor - inColor * roughMetal.y;

   // Cube map sampling
   ivec2 cubedim = textureSize( radianceSampler, 0 );
   int numMipLevels = int( log2( max( cubedim.s, cubedim.y ) ) );
   float mipLevel = numMipLevels - 1.0 + log2( roughMetal.x );
   vec3 radianceSample = pow( textureLod( radianceSampler, R, mipLevel ).rgb, vec3( 2.2f ) );
   vec3 irradianceSample = pow( texture( irradianceSampler, N ).rgb, vec3( 2.2f ) );

   vec3 reflection =
      EnvBRDFApprox( specularColor, pow( roughMetal.x, 1.0f ), clamp( dot( N, V ), 0.0, 1.0 ) );


   // Combine specular IBL and BRDF
   vec3 diffuse = diffuseColor * irradianceSample;
   vec3 spec = radianceSample * reflection;
   vec3 color = diffuse + spec;

   // Tone mapping
   color = Uncharted2Tonemap( color * exposure );
   color = color * ( 1.0f / Uncharted2Tonemap( vec3( 11.2f ) ) );
   // Gamma correction
   color = pow( color, vec3( 1.0f / gamma ) );

   outColor = vec4( color, 1.0 );
}
