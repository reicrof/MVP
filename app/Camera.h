#ifndef CAMERA_H_
#define CAMERA_H_

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace glm;

class Camera
{
  public:
   Camera( float fovInDeg, unsigned width, unsigned height, float near, float far );
   ~Camera() = default;

   const vec3& getPos() const;
   void setPos( const vec3& pos );
   const quat& getOrientation() const;
   void setOrientation( const quat& ori );

   vec3 getForward() const;
   vec3 getRight() const;
   const mat4& getView() const;
   const mat4& getProj() const;
   void setExtent( unsigned width, unsigned height );
   const unsigned getWidth() const;
   const unsigned getHeight() const;

  private:
  inline void updateViewMatrix();
   mat4 _view;
   mat4 _proj;
   vec3 _position;
   quat _orientation;
   float _fovInRad;
   unsigned _width;
   unsigned _height;
   float _near;
   float _far;
};

#endif  // CAMERA_H_
