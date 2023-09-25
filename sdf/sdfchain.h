#ifndef SDFCHAIN_H
#define SDFCHAIN_H
#include "transformop.h"
#include "combineop.h"
#include <vector>

struct SDFLink {
    SDF* s;
    SDFTransformOp t;
    SDFCombineOp* c;
};

struct DistResult {
    float distance;
    int minIndex;
};

class SDFChain : public SDF
{
public:
    SDFChain();
    virtual ~SDFChain();

    virtual float dist(const glm::vec3& point);
    DistResult minDist(const glm::vec3& point);
    void addLink(const SDFLink& l);

protected:
    std::vector<SDFLink> chain;
};

#endif // SDFCHAIN_H
