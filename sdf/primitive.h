#ifndef PRIMITIVE_H
#define PRIMITIVE_H
#include "sdf.h"

/* Simple sphere at origin */
class SDFSphere : public SDF
{
public:
    SDFSphere(float _radius) { radius = _radius; }
    ~SDFSphere() {}

    float dist(const glm::vec3& point) { return glm::length(point) - radius; }
private:
    float radius;
};

/* Cylinder from point a to b with given radius */
class SDFCylinder : public SDF
{
public:
    SDFCylinder(const glm::vec3& _a, const glm::vec3& _b, float _radius) {
        a = _a;
        b = _b;
        radius = _radius;
    }
    ~SDFCylinder() {};

    /* Copied from iq */
    float dist(const glm::vec3& point) {
        glm::vec3  ba = b - a;
        glm::vec3  pa = point - a;
        float baba = glm::dot(ba,ba);
        float paba = glm::dot(pa,ba);
        float x = glm::length(pa*baba-ba*paba) - radius*baba;
        float y = glm::abs(paba-baba*0.5)-baba*0.5;
        float x2 = x*x;
        float y2 = y*y*baba;
        float d = (glm::max(x,y)<0.0)?-glm::min(x2,y2):(((x>0.0)?x2:0.0)+((y>0.0)?y2:0.0));
        return glm::sign(d)*glm::sqrt(glm::abs(d))/baba;
    }
private:
    glm::vec3 a;
    glm::vec3 b;
    float radius;
};

#endif // PRIMITIVE_H
