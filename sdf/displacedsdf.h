#ifndef DISPLACEDSDF_H
#define DISPLACEDSDF_H
#include "combineop.h"

class DisplacedSDF : public SDF
{
public:
    DisplacedSDF(SDF* _surface, SDF* _displacement) {
        surface = _surface;
        displacement = _displacement;
    }
    virtual ~DisplacedSDF() {}

    float dist(const glm::vec3& point) {
        return surface->dist(point) + displacement->dist(point);
    }
private:
    SDF* surface;
    SDF* displacement;
};

#endif // DISPLACEDSDF_H
