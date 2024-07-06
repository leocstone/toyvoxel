#include "worldgenerator.h"
#include <iostream>
#include <random>
#include <cstring>

constexpr int max_search_radius = 64;

static std::random_device rd;
static std::mt19937 rng{rd()};

static int randomStoneMutation() {
    static std::uniform_int_distribution<int> uid(1,256);
    int rint = uid(rng);
    if (rint >= 250) {
        return rint % 3 + 4;
    }
    return 1;
}

static void boundsCheck(int x, int y, int z) {
    if (x < 0 || y < 0 || z < 0 || x >= CHUNK_WIDTH_VOXELS || y >= CHUNK_WIDTH_VOXELS || z >= CHUNK_HEIGHT_VOXELS) {
        throw std::runtime_error("Invalid array access");
    }
}

/* Return the distance in voxels (rounded down) to the nearest voxel to tx,ty,tz */
/* i.e. 0 if there is an adjacent voxel */
static int findClosestVoxel(int tx, int ty, int tz, VoxelChunk* chunkIn) {
    std::cout << "Finding closest voxel to " << tx << " " << ty << " " << tz << std::endl;
    for (int radius = 1; radius < max_search_radius; radius++) {
        /* Each loop counter is the offset from (tx,ty,tz) */
        /* +X Plane */
        int x = tx + radius;
        int y, z;
        if (x < CHUNK_WIDTH_VOXELS) {
            for (y = std::max(0, ty - radius); (y <= ty + radius) && (y < CHUNK_WIDTH_VOXELS); y++) {
                for (z = std::max(0, tz - radius); (z <= tz + radius) && (z < CHUNK_HEIGHT_VOXELS); z++) {
                    boundsCheck(x, y, z);
                    std::cout << "Checking " << x << ", " << y << ", " << z << std::endl;
                    if (chunkIn->getVoxel(x, y, z) < 0) {
                        std::cout << "Found closest voxel at " << x << ", " << y << ", " << z << ". radius = " << radius << std::endl;
                        return radius - 1;
                    }
                }
            }
        }
        /* -X Plane */
        x = tx - radius;
        if (x > 0) {
            for (y = std::max(0, ty - radius); (y <= ty + radius) && (y < CHUNK_WIDTH_VOXELS); y++) {
                for (z = std::max(0, tz - radius); (z <= tz + radius) && (z < CHUNK_HEIGHT_VOXELS); z++) {
                    boundsCheck(x, y, z);
                    std::cout << "Checking " << x << ", " << y << ", " << z << std::endl;
                    if (chunkIn->getVoxel(x, y, z) < 0) {
                        std::cout << "Found closest voxel at " << x << ", " << y << ", " << z << ". radius = " << radius << std::endl;
                        return radius - 1;
                    }
                }
            }
        }
        /* +Y Plane */
        y = ty + radius;
        if (y < CHUNK_WIDTH_VOXELS) {
            for (x = std::max(0, tx - radius + 1); (x < tx + radius - 1) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (z = std::max(0, tz - radius); (z < tz + radius) && (z < CHUNK_HEIGHT_VOXELS); z++) {
                    boundsCheck(x, y, z);
                    std::cout << "Checking " << x << ", " << y << ", " << z << std::endl;
                    if (chunkIn->getVoxel(x, y, z) < 0) {
                        std::cout << "Found closest voxel at " << x << ", " << y << ", " << z << ". radius = " << radius << std::endl;
                        return radius - 1;
                    }
                }
            }
        }
        /* -Y Plane */
        y = ty - radius;
        if (y > 0) {
            for (x = std::max(0, tx - radius + 1); (x < tx + radius - 1) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (z = std::max(0, tz - radius); (z < tz + radius) && (z < CHUNK_HEIGHT_VOXELS); z++) {
                    boundsCheck(x, y, z);
                    std::cout << "Checking " << x << ", " << y << ", " << z << std::endl;
                    if (chunkIn->getVoxel(x, y, z) < 0) {
                        std::cout << "Found closest voxel at " << x << ", " << y << ", " << z << ". radius = " << radius << std::endl;
                        return radius - 1;
                    }
                }
            }
        }
        /* +Z Plane */
        z = tz + radius;
        if (z < CHUNK_HEIGHT_VOXELS) {
            for (x = std::max(0, tx - radius + 1); (x < tx + radius - 1) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (y = std::max(0, ty - radius + 1); (y < ty + radius - 1) && (y < CHUNK_WIDTH_VOXELS); y++) {
                    boundsCheck(x, y, z);
                    std::cout << "Checking " << x << ", " << y << ", " << z << std::endl;
                    if (chunkIn->getVoxel(x, y, z) < 0) {
                        std::cout << "Found closest voxel at " << x << ", " << y << ", " << z << ". radius = " << radius << std::endl;
                        return radius - 1;
                    }
                }
            }
        }
        /* -Z Plane */
        z = tz - radius;
        if (z > 0) {
            for (x = std::max(0, tx - radius + 1); (x < tx + radius - 1) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (y = std::max(0, ty - radius + 1); (y < ty + radius - 1) && (y < CHUNK_WIDTH_VOXELS); y++) {
                    boundsCheck(x, y, z);
                    std::cout << "Checking " << x << ", " << y << ", " << z << std::endl;
                    if (chunkIn->getVoxel(x, y, z) < 0) {
                        std::cout << "Found closest voxel at " << x << ", " << y << ", " << z << ". radius = " << radius << std::endl;
                        return radius - 1;
                    }
                }
            }
        }
    }
    std::cout << "No voxel close enough to " << tx << ", " << ty << ", " << tz << std::endl;
    return 0; // Shouldn't happen unless entire map is empty or max search dist reached
}

static int randomGrassBend(int b) {
    if (b >= 9) {
        return 1;
    } else if (b >= 7) {
        return -1;
    }
    return 0;
}

static void grassTest(VoxelChunk* result, double minHeight) {
    const float allStoneHeight = minHeight * 0.1f; // Everything below 10% of minHeight is all stone
    std::uniform_int_distribution<int> stoneChance(0, 500);
    constexpr double stone_scale_factor = 32.0;
    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            for (int z = 0; z < minHeight; z++) {
                /*
                if (z <= allStoneHeight) {
                    double stoneHeight = Perlin::perlin(x / stone_scale_factor, y / stone_scale_factor, 55);
                    if (z < (stoneHeight * allStoneHeight)) {
                        result->setVoxel(x, y, z, -Stone);
                        continue;
                    } else {
                        result->setVoxel(x, y, z, -Dirt);
                    }
                }
                double height = minHeight;
                float isStone = float(stoneChance(rng)) / 1000.0f;
                isStone *= isStone;
                float stoneChanceForZ = float(z) / float(CHUNK_HEIGHT_VOXELS);
                if (isStone > stoneChanceForZ) {
                    result->setVoxel(x, y, z, -Stone);
                } else {
                    result->setVoxel(x, y, z, -Dirt);
                }
                */
                result->setVoxel(x, y, z, -Dirt);
            }
        }
        //std::cout << "\rGenerating dirt voxels: " << x + 1 << " / " << CHUNK_WIDTH_VOXELS;
    }
    //std::cout << " done." << std::endl;

    // Grass
    constexpr int num_grass_blades = int(float(CHUNK_WIDTH_VOXELS * CHUNK_WIDTH_VOXELS) * 0.3);
    constexpr int max_height_voxels = 1;
    std::uniform_int_distribution<int> randomCoord(0, CHUNK_WIDTH_VOXELS - 1);
    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            result->setVoxel(x, y, minHeight - 1, -Grass);
        }
    }
    for (int i = 0; i < num_grass_blades; i++) {
        int randomX = randomCoord(rng);
        int randomY = randomCoord(rng);
        result->setVoxel(randomX, randomY, minHeight, -Grass);
    }
}

const glm::vec3 voxelCenter(0.5 / float(VOXELS_PER_METER), 0.5 / float(VOXELS_PER_METER), 0.5 / float(VOXELS_PER_METER));

/*
Returns a voxel fragment contained within an AABB from origin to dimensions
*/
static VoxelFragment* proceduralTree(const glm::vec3& dimensions) {
    VoxelFragment* result = new VoxelFragment(VOXELS_PER_METER * glm::ceil(dimensions.x),
                                              VOXELS_PER_METER * glm::ceil(dimensions.y),
                                              VOXELS_PER_METER * glm::ceil(dimensions.z));

    std::uniform_real_distribution<float> randomTrunkHeight(0.7f, 0.75f); // Trunk is 70-75% of available vertical space

    // Constants
    const glm::vec3 center(dimensions.x / 2.0f, dimensions.y / 2.0f, 0.0);
    const float initialTreeRadius = glm::min(dimensions.x, dimensions.y) / 4.0;
    const float trunkHeight = dimensions.z * randomTrunkHeight(rng);
    const float max_branch_length = 3.0f;

    // SDF chain representing tree
    SDFChain treeChain;
    // Base of tree (roots)
    SDFLink roots;
    SDFSphere rootSphere(initialTreeRadius);
    roots.s = (SDF*)&rootSphere;
    SDFTransformOp t;
    t.addScale(glm::vec3(0.95, 1.0, 0.2));
    t.addTranslation(glm::vec3(center));
    roots.t = t;
    roots.c = nullptr; // Not used for first SDF in chain
    treeChain.addLink(roots);
    // Trunk
    //SDFSmoothUnion smooth(0.8f);
    SDFUnion smooth;
    glm::vec3 trunkEnd = center;
    trunkEnd.z += trunkHeight;
    SDFCappedCone trunkCylinder(center, trunkEnd, initialTreeRadius * 0.9, initialTreeRadius * 0.9 * (1.0 - trunkHeight * 0.01));
    SDFTransformOp trunkT;
    // rotate trunk a little so it isn't perfectly vertical
    std::uniform_real_distribution<float> randomRotation(-0.03f, 0.03f);
    trunkT.addRotation(randomRotation(rng), glm::vec3(1, 0, 0));
    trunkT.addRotation(randomRotation(rng), glm::vec3(0, 1, 0));

    // Make the trunk less cylindrical
    SDFSineDisplacement barkRoughness(glm::vec3(20.0, 20.0, 4.0), 0.01);

    // Combine these into a displaced SDF
    //DisplacedSDF roughTrunk(&trunkCylinder, &barkRoughness);
    SDFLink trunkLink;
    trunkLink.c = &smooth;
    trunkLink.s = &trunkCylinder;
    trunkLink.t = trunkT;

    treeChain.addLink(trunkLink);

    // Add L1 branches
    constexpr int min_branches = 10;
    constexpr int max_branches = 10;
    float lastBranchHeight = 0.0f;
    std::uniform_int_distribution<int> randomNumBranches(min_branches, max_branches);
    std::uniform_real_distribution<float> randomThickness(0.3f, 0.4f); // Branches are 30-40% as thick as last level
    std::uniform_real_distribution<float> randomBranchHeight(0.6f, 0.8f); // Branches are 60-80% up the remaining length of the trunk
    std::uniform_real_distribution<float> randomBranchDirection(0.0f, 2.0f * glm::pi<float>()); // Branch origin is some point along the outside of the trunk
    std::uniform_real_distribution<float> randomBranchLength(0.8f, 1.0f);
    int numBranches = randomNumBranches(rng);
    SDFCurvedXYCone* branchCones = new SDFCurvedXYCone[numBranches];
    for (int branch = 0; branch < numBranches; branch++) {
        float l1Thickness = randomThickness(rng);
        float curBranchHeight = randomBranchHeight(rng);

        glm::vec3 curBranchStart = center;
        curBranchStart.z += trunkHeight * curBranchHeight;
        float curBranchTheta = randomBranchDirection(rng);
        glm::vec3 curBranchDirection(glm::cos(curBranchTheta), glm::sin(curBranchTheta), 0);
        glm::vec3 vectorToExterior = curBranchDirection;
        vectorToExterior *= initialTreeRadius * 1.0;
        curBranchStart += vectorToExterior;
        curBranchStart = trunkT.transformPoint(curBranchStart);

        float curBranchLength = randomBranchLength(rng) * max_branch_length;
        glm::vec3 curBranchEnd = curBranchStart + curBranchLength * curBranchDirection;

        SDFLink curBranch;
        branchCones[branch] = SDFCurvedXYCone(curBranchLength, l1Thickness, l1Thickness * 0.9, 2.0, 1.0);
        curBranch.s = (SDF*)&branchCones[branch];
        curBranch.t = SDFTransformOp();
        curBranch.t.addTranslation(curBranchStart);
        curBranch.t.addRotation(curBranchTheta, glm::vec3(0, 0, 1));
        curBranch.c = &smooth;
        treeChain.addLink(curBranch);
    }

    for (int x = 0; x < result->sizeX; x++) {
        for (int y = 0; y < result->sizeY; y++) {
            for (int z = 0; z < result->sizeZ; z++) {
                glm::vec3 curPoint(float(x) / float(VOXELS_PER_METER) + voxelCenter.x,
                                   float(y) / float(VOXELS_PER_METER) + voxelCenter.y,
                                   float(z) / float(VOXELS_PER_METER) + voxelCenter.z);
                float distSample = treeChain.dist(curPoint);
                if (distSample < 0.0f) {
                    result->setVoxel(x, y, z, -4);
                } else {
                    result->setVoxel(x, y, z, 0);
                }
            }
        }
    }
    delete[] branchCones;

    return result;
}

/*
Copy voxels from src into dst at offset
*/
static void blitVoxels(VoxelChunk* dst, VoxelFragment* src, int sx, int sy, int sz) {
    for (int x = 0; (x + sx < CHUNK_WIDTH_VOXELS) && (x < src->sizeX); x++) {
        for (int y = 0; (y + sy < CHUNK_WIDTH_VOXELS) && (y < src->sizeY); y++) {
            for (int z = 0; (z + sz < CHUNK_HEIGHT_VOXELS) && (z < src->sizeZ); z++) {
                //int dstIndex = (sx + x) + (sy + y) * CHUNK_WIDTH_VOXELS + (sz + z) * CHUNK_WIDTH_VOXELS * CHUNK_WIDTH_VOXELS;
                int srcIndex = x + y * src->sizeX + z * src->sizeX * src->sizeY;
                if (src->voxels[srcIndex] < 0) {
                    dst->setVoxel(sx + x, sy + y, sz + z, src->voxels[srcIndex]);
                }
            }
        }
    }
}

static void forestTest(VoxelChunk* dst) {
    double grassHeight = 128;
    grassTest(dst, grassHeight);
    /*
    for (int x = 0; x < VOXELS_PER_METER; x++) {
        for (int y = 0; y < VOXELS_PER_METER; y++) {
            for (int z = 0; z < VOXELS_PER_METER; z++) {
                if ((x * x + y * y) < 64) {
                    result->setVoxel(x, y, z, -4);
                } else {
                    result->setVoxel(x, y, z, 0);
                }
                result->setVoxel(x, y, z, -4);
            }
        }
    }
    */
    const glm::vec3 treeDimensions(5, 5, 20);
    VoxelFragment* src = proceduralTree(treeDimensions);
    std::uniform_int_distribution<int> randomTreeX(0, CHUNK_WIDTH_VOXELS - src->sizeX - 1);
    int randomX = randomTreeX(rng);
    int randomY = randomTreeX(rng);
    blitVoxels(dst, src, randomX, randomY, int(grassHeight));
    delete[] src->voxels;
}

static int findClosestVoxelSafe(int tx, int ty, int tz, VoxelChunk* chunkIn) {
    for (int radius = 1; radius < max_search_radius; radius++) {
        for (int x = tx - radius; x < tx + radius + 1; x++) {
            if (x < 0 || x >= CHUNK_WIDTH_VOXELS)
                continue;
            for (int y = ty - radius; y < ty + radius + 1; y++) {
                if (y < 0 || y >= CHUNK_WIDTH_VOXELS)
                    continue;
                for (int z = tz - radius; z < tz + radius + 1; z++) {
                    if (z < 0 || z >= CHUNK_WIDTH_VOXELS)
                        continue;
                    boundsCheck(x, y, z);
                    if (chunkIn->getVoxel(x, y, z) < 0) {
                        /*
                        std::cout << "Found closest voxel for " << tx << ", " << ty << ", " << tz
                                                      << " at " << x << ", " << y << ", " << z
                                                      << ". radius = " << radius <<
                                                      ", value = " << int(chunkIn->voxels[x][y][z]) << std::endl;
                        */
                        assert(x == 0 || x == CHUNK_WIDTH_VOXELS-1 || y == 0 || y == CHUNK_WIDTH_VOXELS-1 || z == 0 || z == CHUNK_HEIGHT_VOXELS-1);
                        return radius - 1;
                    }
                }
            }
        }
    }
    std::cout << "No voxel close enough to " << tx << ", " << ty << ", " << tz << std::endl;
    return 0;
}

/* Fill out all zero voxels with the distances to closest voxel */
static void computeDistances(VoxelChunk* chunkIn) {
    std::cout << "Computing distances. Total to compute: " << CHUNK_WIDTH_VOXELS << " ^2 * " << CHUNK_HEIGHT_VOXELS << std::endl;
    for (int x = 0; x < CHUNK_WIDTH_METERS * VOXELS_PER_METER; x++) {
        for (int y = 0; y < CHUNK_WIDTH_METERS * VOXELS_PER_METER; y++) {
            for (int z = 0; z < CHUNK_HEIGHT_METERS * VOXELS_PER_METER; z++) {
                //boundsCheck(x, y, z);
                if (chunkIn->getVoxel(x, y, z) == 0) {
                    chunkIn->setVoxel(x, y, z, findClosestVoxelSafe(x, y, z, chunkIn));
                }
            }
        }
        //std::cout << "\r" << x + 1 << " / " << CHUNK_WIDTH_METERS * VOXELS_PER_METER;
        //std::cout.flush();
    }
    //std::cout << std::endl;
}

/*
Literally just 1 voxel for the floor
*/
void generatePavement(VoxelChunk* result, int chunkX, int chunkY) {
    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            result->setVoxel(x, y, 0, -Stone);
        }
    }
}

void generateBuilding(VoxelChunk* result, int chunkX, int chunkY) {
    double grassHeight = 1;
    //grassTest(result, grassHeight);
    generatePavement(result, chunkX, chunkY);
    /*
    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            for (int z = 33; z < CHUNK_HEIGHT_VOXELS; z++) {
                result->setVoxel(x, y, z, std::min(z - 33, 127));
            }
        }
    }
    */
    std::vector<Voxel> materials;
    // Add building
    std::uniform_int_distribution<int> randomShackOffset(1, CHUNK_WIDTH_VOXELS - 7 * VOXELS_PER_METER);
    const float wallThickness = 0.2f;
    const float shackHeight = 3.0f;
    const float baseHeight = 0.3f;
    const float shackWidthX = 5.0f;
    const float shackWidthY = 7.0f;
    const float doorWidth = 1.1f;
    const float doorHeight = 2.0f;
    const float doorMargin = 1.0f;
    float offsetX = float(randomShackOffset(rng)) / float(VOXELS_PER_METER);
    float offsetY = float(randomShackOffset(rng)) / float(VOXELS_PER_METER);
    SDFChain buildingChain;
    SDFLink baseLink;
    SDFAABB baseAABB(glm::vec3(shackWidthX, shackWidthY, baseHeight));
    SDFTransformOp baseT;
    //baseT.addTranslation(glm::vec3(CHUNK_WIDTH_METERS / 2.0f - 2.5f, CHUNK_WIDTH_METERS / 2.0f - 3.5f, grassHeight / VOXELS_PER_METER));
    baseT.addTranslation(glm::vec3(shackWidthX / 2.0f, shackWidthY / 2.0f, baseHeight / 2.0f + grassHeight / VOXELS_PER_METER));
    baseLink.s = &baseAABB;
    baseLink.t = baseT;
    baseLink.c = nullptr;
    buildingChain.addLink(baseLink);
    materials.push_back(Stone);

    /* Walls */
    SDFLink wallsLink;
    SDFChain wallsChain;
    SDFUnion combine;
    wallsLink.t = SDFTransformOp();
    wallsLink.c = &combine;
    wallsLink.s = &wallsChain;

    /* -x */
    SDFLink wall1Link;
    SDFAABB wall1AABB(glm::vec3(shackWidthX, wallThickness, shackHeight));
    SDFTransformOp wall1T;
    wall1T.addTranslation(glm::vec3(shackWidthX / 2.0f, wallThickness / 2.0f, shackHeight / 2.0f + grassHeight / VOXELS_PER_METER + baseHeight));
    wall1Link.s = &wall1AABB;
    wall1Link.t = wall1T;
    wall1Link.c = &combine;
    wallsChain.addLink(wall1Link);

    /* +x */
    SDFLink wall3Link;
    SDFAABB wall3AABB(glm::vec3(shackWidthX, wallThickness, shackHeight));
    SDFTransformOp wall3T;
    wall3T.addTranslation(glm::vec3(shackWidthX / 2.0f, shackWidthY - wallThickness / 2.0f, shackHeight / 2.0f + grassHeight / VOXELS_PER_METER + baseHeight));
    wall3Link.s = &wall3AABB;
    wall3Link.t = wall3T;
    wall3Link.c = &combine;
    wallsChain.addLink(wall3Link);

    /* -y */
    SDFLink wall2Link;
    SDFAABB wall2AABB(glm::vec3(wallThickness, shackWidthY, shackHeight));
    SDFTransformOp wall2T;
    wall2T.addTranslation(glm::vec3(wallThickness / 2.0f, shackWidthY / 2.0f, shackHeight / 2.0f + grassHeight / VOXELS_PER_METER + baseHeight));
    wall2Link.s = &wall2AABB;
    wall2Link.t = wall2T;
    wall2Link.c = &combine;
    wallsChain.addLink(wall2Link);

    /* +y */
    SDFLink wall4Link;
    SDFAABB wall4AABB(glm::vec3(wallThickness, shackWidthY, shackHeight));
    SDFTransformOp wall4T;
    wall4T.addTranslation(glm::vec3(shackWidthX - wallThickness / 2.0f, shackWidthY / 2.0f, shackHeight / 2.0f + grassHeight / VOXELS_PER_METER + baseHeight));
    wall4Link.s = &wall4AABB;
    wall4Link.t = wall4T;
    wall4Link.c = &combine;
    wallsChain.addLink(wall4Link);

    /* Doorway */
    SDFSubtract subtraction;
    std::uniform_int_distribution<int> randomDoorOffset(0, 100);
    const float minDoorOffset = doorMargin + doorWidth / 2.0f;
    const float maxDoorOffset = shackWidthX - doorWidth - doorMargin;
    float doorOffset = std::min(minDoorOffset, (float(randomDoorOffset(rng)) / 100.0f) * maxDoorOffset);
    SDFLink doorwayLink;
    SDFAABB doorwayAABB(glm::vec3(doorWidth, wallThickness, doorHeight));
    SDFTransformOp doorwayT;
    doorwayT.addTranslation(glm::vec3(doorWidth / 2.0f + doorOffset, shackWidthY - wallThickness / 2.0f, doorHeight / 2.0f + grassHeight / VOXELS_PER_METER + baseHeight));
    doorwayLink.s = &doorwayAABB;
    doorwayLink.t = doorwayT;
    doorwayLink.c = &subtraction;
    wallsChain.addLink(doorwayLink);

    buildingChain.addLink(wallsLink);
    materials.push_back(Wood);

    VoxelFragment shackFragment(shackWidthX * VOXELS_PER_METER, shackWidthY * VOXELS_PER_METER, shackHeight * VOXELS_PER_METER);

    for (int x = 0; x < shackFragment.sizeX; x++) {
        for (int y = 0; y < shackFragment.sizeY; y++) {
            for (int z = 0; z < shackFragment.sizeZ; z++) {
                glm::vec3 curPoint(float(x) / float(VOXELS_PER_METER) + voxelCenter.x,
                                   float(y) / float(VOXELS_PER_METER) + voxelCenter.y,
                                   float(z) / float(VOXELS_PER_METER) + voxelCenter.z);
                DistResult distSample = buildingChain.minDist(curPoint);
                if (distSample.distance <= 0.0f) {
                    shackFragment.setVoxel(x, y, z, -materials[distSample.minIndex]);
                } else {
                    shackFragment.setVoxel(x, y, z, 0);
                }
            }
        }
    }
    blitVoxels(result, &shackFragment, offsetX * VOXELS_PER_METER, offsetY * VOXELS_PER_METER, 0);
    shackFragment.freeVoxels();
}

void WorldGenerator::generateChunk(VoxelChunk* result, int chunkX, int chunkY) {
    forestTest(result);
}
