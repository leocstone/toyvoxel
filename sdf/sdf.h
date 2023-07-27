#ifndef SDF_H
#define SDF_H
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class SDF
{
public:
    virtual float dist(const glm::vec3& point) = 0;
};

#endif // SDF_H
