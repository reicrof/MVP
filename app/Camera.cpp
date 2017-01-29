#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

Camera::Camera( float fovInDeg, unsigned width, unsigned height, float near, float far )
    : _fovInRad( glm::radians( fovInDeg ) ),
      _width( width ),
      _height( height ),
      _near( near ),
      _far( far )
{
   _proj = glm::perspective( _fovInRad, width / static_cast<float>( height ), near, far );
   // Flip Y (because Vulkan)
   _proj[ 1 ][ 1 ] *= -1;

   _view = glm::lookAt( glm::vec3( 2.0f, 2.0f, 2.0f ), glm::vec3( 0.0f, 0.0f, 0.0f ),
                        glm::vec3( 0.0f, 0.0f, 1.0f ) );
}

const vec3& Camera::getPos() const
{
   return _position;
}

void Camera::setPos( const vec3& pos )
{
   _position = pos;
   updateViewMatrix();
}

const quat& Camera::getOrientation() const
{
   return _orientation;
}

void Camera::setOrientation( const quat& ori )
{
   _orientation = ori;
   updateViewMatrix();
}

vec3 Camera::getForward() const
{
   static const vec3 FORWARD( 0.0f, 0.0f, 1.0f );
   return _orientation * FORWARD;
}

vec3 Camera::getRight() const
{
   static const vec3 RIGHT( 1.0f, 0.0f, 0.0f );
   return _orientation * RIGHT;
}

const mat4& Camera::getView() const
{
   return _view;
}

const mat4& Camera::getProj() const
{
   return _proj;
}

const unsigned Camera::getWidth() const
{
   return _width;
}

void Camera::setExtent( unsigned width, unsigned height )
{
   _width = width;
   _height = height;
   _proj = glm::perspective( _fovInRad, width / static_cast<float>( height ), _near, _far );
   // Flip Y (because Vulkan)
   _proj[ 1 ][ 1 ] *= -1;
}

const unsigned Camera::getHeight() const
{
   return _height;
}

void Camera::updateViewMatrix()
{
   _view = translate( toMat4( glm::conjugate( _orientation ) ), -1.0f * _position );
}