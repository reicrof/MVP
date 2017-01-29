#ifndef CAMERA_H_
#define CAMERA_H_

#include "glmIncludes.h"

class Camera
{
  public:
   Camera( float fovInDeg, unsigned width, unsigned height, float near, float far );
   ~Camera() = default;

   const glm::vec3& getPos() const;
   void setPos( const glm::vec3& pos );
   const glm::quat& getOrientation() const;
   void setOrientation( const glm::quat& ori );

   glm::vec3 getForward() const;
   glm::vec3 getRight() const;
   const glm::mat4& getView() const;
   const glm::mat4& getProj() const;
   void setExtent( unsigned width, unsigned height );
   const unsigned getWidth() const;
   const unsigned getHeight() const;

  private:
   inline void updateViewMatrix();
   glm::mat4 _view;
   glm::mat4 _proj;
   glm::vec3 _position;
   glm::quat _orientation;
   float _fovInRad;
   unsigned _width;
   unsigned _height;
   float _near;
   float _far;
};

#endif  // CAMERA_H_
