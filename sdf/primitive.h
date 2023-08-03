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
    SDFCylinder() {}
    SDFCylinder(const glm::vec3& _a, const glm::vec3& _b, float _radius) {
        a = _a;
        b = _b;
        radius = _radius;
    }
    ~SDFCylinder() {}

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

class SDFCappedCone : public SDF
{
public:
    SDFCappedCone() {}
    SDFCappedCone(const glm::vec3& _a, const glm::vec3& _b, float _ra, float _rb) {
        a = _a;
        b = _b;
        ra = _ra;
        rb = _rb;
    }
    ~SDFCappedCone() {}

    /* Copied from iq */
    float dist(const glm::vec3& point)
    {
      float rba  = rb-ra;
      float baba = glm::dot(b-a,b-a);
      float papa = glm::dot(point-a,point-a);
      float paba = glm::dot(point-a,b-a)/baba;
      float x = glm::sqrt( papa - paba*paba*baba );
      float cax = glm::max(0.0f,x-((paba<0.5)?ra:rb));
      float cay = glm::abs(paba-0.5)-0.5;
      float k = rba*rba + baba;
      float f = glm::clamp( (rba*(x-ra)+paba*baba)/k, 0.0f, 1.0f );
      float cbx = x-ra - f*rba;
      float cby = paba - f;
      float s = (cbx<0.0 && cay<0.0) ? -1.0 : 1.0;
      return s*glm::sqrt( glm::min(cax*cax + cay*cay*baba,
                         cbx*cbx + cby*cby*baba) );
    }
private:
    glm::vec3 a;
    glm::vec3 b;
    float ra;
    float rb;
};

/*
Cone along positive X axis that has a built-in transformation to curve upwards
curveAmount - rotation amount at end of cone, where 1.0 = 90 degree rotation from start
*/
class SDFCurvedXYCone : public SDF {
public:
    SDFCurvedXYCone() {}
    SDFCurvedXYCone(float _length, float _ra, float _rb, float _curveAmount, float _curvePower) {
        length = _length;
        ra = _ra;
        rb = _rb;
        curveAmount = _curveAmount;
        curvePower = _curvePower;
    }
    ~SDFCurvedXYCone() {}

    float dist(const glm::vec3& point) {
        if (point.x < 0.0 || point.x > length) {
            return glm::abs(point.x);
        }
        //glm::vec3 lv(0, point.y, point.z);
        //return glm::length(lv) - ra;
        glm::mat4 curTransform(1.0);
        float distanceAlongCone = (point.x / length);
        float curRadius = ra;//ra + (rb - ra) * distanceAlongCone;
        //float curveFunction = glm::pow(distanceAlongCone, curvePower); // sample of curve function - dependent on length
        float rotationAmount = (glm::radians(30.0) * curveAmount) * distanceAlongCone;
        curTransform = glm::rotate(curTransform, rotationAmount, glm::vec3(0, 1, 0));
        glm::vec4 transformedPoint4 = glm::vec4(point, 1.0);
        transformedPoint4 = curTransform * transformedPoint4;
        glm::vec3 transformedPoint(transformedPoint4.x, transformedPoint4.y, transformedPoint4.z);
        transformedPoint.x = 0.0;
        return glm::length(transformedPoint) - curRadius;
    }
private:
    float length;
    float ra;
    float rb;
    float curveAmount;
    float curvePower;
};

#endif // PRIMITIVE_H
