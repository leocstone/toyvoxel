#ifndef SDFSPHERE_H
#define SDFSPHERE_H
#include "sdf.h"

class SDFSphere
{
public:
    SDFSphere(const glm::vec3& _origin, float _radius);
    virtual ~SDFSphere();

    virtual float dist(const glm::vec3& point);

private:
    glm::vec3 origin;
    float radius;
};

#endif // SDFSPHERE_H
