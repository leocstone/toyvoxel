#include "sdfchain.h"

SDFChain::SDFChain() : chain() {}

SDFChain::~SDFChain() {}

float SDFChain::dist(const glm::vec3& point) {
    size_t numLinks = chain.size();
    float* distances = new float[numLinks];
    for (size_t i = 0; i < numLinks; i++) {
        distances[i] = chain[i].s->dist(chain[i].t(point));
    }
    float curDist = distances[0];
    for (size_t i = 1; i < numLinks; i++) {
        curDist = chain[i].c->combinedDist(curDist, distances[i]);
    }
    delete[] distances;
    return curDist;
}

DistResult SDFChain::minDist(const glm::vec3& point) {
    size_t numLinks = chain.size();
    float* distances = new float[numLinks];
    for (size_t i = 0; i < numLinks; i++) {
        distances[i] = chain[i].s->dist(chain[i].t(point));
    }
    float curDist = distances[0];
    int curMin = 0;
    for (size_t i = 1; i < numLinks; i++) {
        if (curDist > distances[i]) {
            curDist = distances[i];
            curMin = i;
        }
    }
    delete[] distances;
    return {curDist, curMin};
}

void SDFChain::addLink(const SDFLink& l)
{
    chain.push_back(l);
}
