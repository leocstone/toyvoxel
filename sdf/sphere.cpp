#include "sphere.h"

SDFSphere::SDFSphere(const glm::vec3& _origin, float _radius)
{
   origin = _origin;
   radius = _radius;
}

SDFSphere::~SDFSphere()
{
    //dtor
}

float SDFSphere::dist(const glm::vec3& point)
{
    return glm::length(point - origin);
}
