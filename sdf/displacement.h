/*
This file defines displacement SDF functions to be used with a combine op - not actual SDFs
*/

#ifndef SDFDISPLACEMENT_H
#define SDFDISPLACEMENT_H
#include "sdf.h"

class SDFSineDisplacement : public SDF
{
public:
    SDFSineDisplacement(const glm::vec3& _scale, float _amount) {
        scale = _scale;
        amount = _amount;
    }
    ~SDFSineDisplacement() {}

    float dist(const glm::vec3& point) {
        return amount * (glm::sin(point.x * scale.x) +
               glm::sin(point.y * scale.y) +
               glm::sin(point.z * scale.z));
    }

private:
    glm::vec3 scale;
    float amount;
};

#endif // SDFDISPLACEMENT_H
