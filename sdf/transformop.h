#ifndef SDFTRANSFORMOP_H
#define SDFTRANSFORMOP_H
#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

class SDFTransformOp
{
public:
    SDFTransformOp();
    virtual ~SDFTransformOp();

    glm::vec3 operator()(const glm::vec3& point) const {
        glm::vec4 tv(point, 1);
        tv = glm::inverse(transformMat) * tv;
        return glm::vec3(tv.x, tv.y, tv.z);
    }

    void addTranslation(const glm::vec3& t) {
        transformMat = glm::translate(transformMat, t);
    }

    void addRotation(float angle, const glm::vec3& axis) {
        transformMat = glm::rotate(transformMat, angle, axis);
    }

    void addScale(const glm::vec3& scale) {
        transformMat = glm::scale(transformMat, scale);
    }

    glm::vec3 transformPoint(const glm::vec3& point) const {
        glm::vec4 tv(point, 1);
        tv = transformMat * tv;
        return glm::vec3(tv.x, tv.y, tv.z);
    }

protected:
    glm::mat4 transformMat;
};

#endif // SDFTRANSFORMOP_H
