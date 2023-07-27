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
    virtual float combinedDist(float s1, float s2) {
        return glm::min(s1, s2);
    }
};

#endif // SDFCOMBINEOP_H
