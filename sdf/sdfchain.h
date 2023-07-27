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

class SDFChain : SDF
{
public:
    SDFChain();
    virtual ~SDFChain();

    virtual float dist(const glm::vec3& point);
    void addLink(const SDFLink& l);

protected:
    std::vector<SDFLink> chain;
};

#endif // SDFCHAIN_H
