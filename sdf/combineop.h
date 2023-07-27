#ifndef SDFCOMBINEOP_H
#define SDFCOMBINEOP_H
#include "sdf.h"

class SDFCombineOp
{
public:
    virtual float combinedDist(float s1, float s2) = 0;
};

class SDFUnion : SDFCombineOp
{
public:
    float combinedDist(float s1, float s2) {
        return glm::min(s1, s2);
    }
};

/*
Copied from iq
Smoothed union of two SDFS
*/
class SDFSmoothUnion : public SDFCombineOp
{
public:
    SDFSmoothUnion(float _smoothAmount) {
        smoothAmount = _smoothAmount;
    }
    ~SDFSmoothUnion() {}

    float combinedDist(float s1, float s2) {
        float h = glm::clamp( 0.5 + 0.5*(s2-s1)/smoothAmount, 0.0, 1.0 );
        return glm::mix( s2, s1, h ) - smoothAmount*h*(1.0-h);
    }
private:
    float smoothAmount;
};

/*
For displacements
*/
class SDFDisplace : public SDFCombineOp
{
public:
    SDFDisplace() {}
    ~SDFDisplace() {}

    float combinedDist(float s1, float s2) { return s1 + s2; }
};

#endif // SDFCOMBINEOP_H
