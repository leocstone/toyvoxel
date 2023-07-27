#include "worldgenerator.h"
#include <iostream>
#include <random>

constexpr int max_search_radius = CHUNK_HEIGHT_VOXELS;

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
    //std::cout << "Finding closest voxel to " << tx << " " << ty << " " << tz << std::endl;
    for (int radius = 1; radius < max_search_radius; radius++) {
        /* Each loop counter is the offset from (tx,ty,tz) */
        /* +X Plane */
        int x = tx + radius;
        int y, z;
        if (x < CHUNK_WIDTH_VOXELS) {
            for (y = std::max(0, ty - radius); (y < ty + radius) && (y < CHUNK_WIDTH_VOXELS); y++) {
                for (z = std::max(0, tz - radius); (z < tz + radius) && (z < CHUNK_HEIGHT_VOXELS); z++) {
                    boundsCheck(x, y, z);
                    if (chunkIn->getVoxel(x, y, z) < 0)
                        return radius - 1;
                }
            }
        }
        /* -X Plane */
        x = tx - radius;
        if (x > 0) {
            for (y = std::max(0, ty - radius); (y < ty + radius) && (y < CHUNK_WIDTH_VOXELS); y++) {
                for (z = std::max(0, tz - radius); (z < tz + radius) && (z < CHUNK_HEIGHT_VOXELS); z++) {
                    boundsCheck(x, y, z);
                    if (chunkIn->getVoxel(x, y, z) < 0)
                        return radius - 1;
                }
            }
        }
        /* +Y Plane */
        y = ty + radius;
        if (y < CHUNK_WIDTH_VOXELS) {
            for (x = std::max(0, tx - radius + 1); (x < tx + radius - 1) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (z = std::max(0, tz - radius); (z < tz + radius) && (z < CHUNK_HEIGHT_VOXELS); z++) {
                    boundsCheck(x, y, z);
                    if (chunkIn->getVoxel(x, y, z) < 0)
                        return radius - 1;
                }
            }
        }
        /* -Y Plane */
        y = ty - radius;
        if (y > 0) {
            for (x = std::max(0, tx - radius + 1); (x < tx + radius - 1) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (z = std::max(0, tz - radius); (z < tz + radius) && (z < CHUNK_HEIGHT_VOXELS); z++) {
                    boundsCheck(x, y, z);
                    if (chunkIn->getVoxel(x, y, z) < 0)
                        return radius - 1;
                }
            }
        }
        /* +Z Plane */
        z = tz + radius;
        if (z < CHUNK_HEIGHT_VOXELS) {
            for (x = std::max(0, tx - radius + 1); (x < tx + radius - 1) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (y = std::max(0, ty - radius + 1); (y < ty + radius - 1) && (y < CHUNK_WIDTH_VOXELS); y++) {
                    boundsCheck(x, y, z);
                    if (chunkIn->getVoxel(x, y, z) < 0)
                        return radius - 1;
                }
            }
        }
        /* -Z Plane */
        z = tz - radius;
        if (z > 0) {
            for (x = std::max(0, tx - radius + 1); (x < tx + radius - 1) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (y = std::max(0, ty - radius + 1); (y < ty + radius - 1) && (y < CHUNK_WIDTH_VOXELS); y++) {
                    boundsCheck(x, y, z);
                    if (chunkIn->getVoxel(x, y, z) < 0)
                        return radius - 1;
                }
            }
        }
    }
    return 0; // Shouldn't happen unless entire map is empty or max search dist reached
}

/* Fill out all zero voxels with the distances to closest voxel */
static void computeDistances(VoxelChunk* chunkIn) {
    std::cout << "Computing distances. Total to compute: " << CHUNK_WIDTH_VOXELS << " ^2 * " << CHUNK_HEIGHT_VOXELS << std::endl;
    for (int x = 0; x < CHUNK_WIDTH_METERS * VOXELS_PER_METER; x++) {
        for (int y = 0; y < CHUNK_WIDTH_METERS * VOXELS_PER_METER; y++) {
            for (int z = 0; z < CHUNK_HEIGHT_METERS * VOXELS_PER_METER; z++) {
                boundsCheck(x, y, z);
                if (chunkIn->getVoxel(x, y, z) == 0) {
                    chunkIn->setVoxel(x, y, z, findClosestVoxel(x, y, z, chunkIn));
                }
            }
        }
        std::cout << "\r" << x + 1 << " / " << CHUNK_WIDTH_METERS * VOXELS_PER_METER;
        std::cout.flush();
    }
    std::cout << std::endl;
}

static VoxelChunk* basicPerlinTest() {
    VoxelChunk* result = new VoxelChunk();

    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            for (int z = 0; z < CHUNK_HEIGHT_VOXELS; z++) {
                constexpr double scale_factor = 32.0;
                constexpr double scale_factor_surface = 32.0;
                if (z >= 60.0 * VOXELS_PER_METER) {
                    double height = Perlin::perlin(double(x) / scale_factor_surface, double(y) / scale_factor_surface, double(60 * VOXELS_PER_METER) / scale_factor_surface) * 4.0 + 60.0 * VOXELS_PER_METER;
                    result->setVoxel(x, y, z, (z <= height) ? -3 : 0);
                } else if (z >= 53.0 * VOXELS_PER_METER) {
                    result->setVoxel(x, y, z, -2);
                } else {
                    int voxel = (Perlin::perlin(double(x) / scale_factor, double(y) / scale_factor, double(z) / scale_factor) < 0.5) ? randomStoneMutation() * -1 : 0;
                    result->setVoxel(x, y, z, voxel);
                }
            }
        }
    }

    return result;
}

static VoxelChunk* erosionTest() {
    VoxelChunk* result = new VoxelChunk();

    /* Simple 2d perlin as heightmap */
    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            for (int z = 0; z < CHUNK_HEIGHT_VOXELS; z++) {
                constexpr double scale_factor = 32.0;
                double height = Perlin::perlin(double(x) / scale_factor, double(y) / scale_factor, 55.0) * VOXELS_PER_METER * 14.0;
                int voxel = (z <= height) ? -3 : 0;
                result->setVoxel(x, y, z, voxel);
            }
        }
    }

    /* Erode */
    constexpr int EROSION_ITERATIONS = CHUNK_WIDTH_VOXELS * CHUNK_WIDTH_VOXELS * 64;
    std::uniform_int_distribution<int> randomPosition(0, CHUNK_WIDTH_VOXELS);
    std::uniform_int_distribution<int> randomDirection(0, 3);
    std::cout << "Erosion simulation starting." << std::endl;
    for (int iteration = 0; iteration < EROSION_ITERATIONS; iteration++) {
        int curX = randomPosition(rng);
        int curY = randomPosition(rng);

        // Droplet starts at top of world
        int height = CHUNK_HEIGHT_VOXELS - 1;
        int carriedVoxel = 0;
        /* Pick up carried voxel */
        while (height > 0 && (result->getVoxel(curX, curY, height) == 0)) {
            height--;
        }
        carriedVoxel = result->getVoxel(curX, curY, height);
        result->setVoxel(curX, curY, height, 0);
        if (carriedVoxel == 0)
            continue;
        // Trace the path of the droplet from impact point
        float energy = 1.0f; // Droplet starts at terminal velocity (1)
        while (energy > 0.0f) {
            /* First, check for falling down (no energy) */
            while (height > 0 && (result->getVoxel(curX, curY, height - 1) == 0)) {
                height--;
            }

            /* Check to move sideways and use energy */
            /* 1: +x 2: -x 3: +y 4: -y */
            int curRandDirections[4][2] = {{1, 0},{-1, 0},{0, 1},{0, -1}};
            /* Shuffle the directions so we are guaranteed to get one of each in any given 4 samples */
            constexpr int swaps = 4;
            for (int s = 0; s < swaps; s++) {
                int idx1 = randomDirection(rng);
                int idx2 = randomDirection(rng);
                int tmp[2] = {curRandDirections[idx2][0], curRandDirections[idx2][1]};
                curRandDirections[idx2][0] = curRandDirections[idx1][0];
                curRandDirections[idx2][1] = curRandDirections[idx1][1];
                curRandDirections[idx1][0] = tmp[0];
                curRandDirections[idx1][1] = tmp[1];
            }
            bool madeMove = false;
            for (int i = 0; i < 4 && (!madeMove); i++) {
                int sampledX = curX + curRandDirections[i][0];
                int sampledY = curY + curRandDirections[i][1];
                if ((sampledX >= 0 && sampledX < CHUNK_WIDTH_VOXELS) && (sampledY >= 0 && sampledY < CHUNK_WIDTH_VOXELS)) {
                    if (result->getVoxel(sampledX, sampledY, height) == 0) {
                        curX = sampledX;
                        curY = sampledY;
                        energy -= 0.1f;
                        madeMove = true;
                    }
                }
            }
            if (!madeMove)
                break;
        }
        result->setVoxel(curX, curY, height, carriedVoxel);
    }
    std::cout << std::endl;

    return result;
}

static int randomGrassBend(int b) {
    if (b >= 9) {
        return 1;
    } else if (b >= 7) {
        return -1;
    }
    return 0;
}

static VoxelChunk* grassTest(double minHeight) {
    VoxelChunk* result = new VoxelChunk();
    constexpr double scale_factor = 32.0;
    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            for (int z = 0; z < CHUNK_HEIGHT_VOXELS; z++) {
                double height = 3.0;//Perlin::octavePerlin(double(x) / scale_factor, double(y) / scale_factor, 55.0, 16, 0.5) * 3.0 + minHeight;
                result->setVoxel(x, y, z, (z <= height) ? -2 : 0);
            }
        }
        //std::cout << "\rGenerating dirt voxels: " << x + 1 << " / " << CHUNK_WIDTH_VOXELS;
    }
    //std::cout << " done." << std::endl;

    // Grass
    constexpr int num_grass_blades = int(float(CHUNK_WIDTH_VOXELS * CHUNK_WIDTH_VOXELS) * 0.9);
    constexpr int max_height_voxels = 8;
    std::uniform_int_distribution<int> randomCoord(0, CHUNK_WIDTH_VOXELS - 1);
    std::uniform_int_distribution<int> randomHeight(1, max_height_voxels);
    std::uniform_int_distribution<int> randomBend(0, 10);
    for (int i = 0; i < num_grass_blades; i++) {
        int randomX = randomCoord(rng);
        int randomY = randomCoord(rng);
        int totalBendX = 0;
        int totalBendY = 0;
        int bendDirX = randomGrassBend(randomBend(rng));
        int bendDirY = randomGrassBend(randomBend(rng));
        int randomBladeHeight = randomHeight(rng);
        result->setVoxel(randomX, randomY, 3, -3);
        for (int h = 1; h < randomBladeHeight; h++) {
            int bendX = randomBend(rng) - h;
            if (bendX >= 8 && totalBendX == 0 && totalBendY == 0) {
                totalBendX += bendDirX;
            }
            int bendY = randomBend(rng) - h;
            if (bendY >= 8 && totalBendX == 0 && totalBendY == 0) {
                totalBendY += bendDirY;
            }
            int placedX = std::clamp(randomX + totalBendX, 0, CHUNK_WIDTH_VOXELS - 1);
            int placedY = std::clamp(randomY + totalBendY, 0, CHUNK_WIDTH_VOXELS - 1);
            result->setVoxel(placedX, placedY, 3 + h, -3);
        }
    }

    return result;
}

/*
Returns a voxel fragment contained within an AABB from origin to dimensions
*/
static VoxelFragment* proceduralTree(const glm::vec3& dimensions) {
    VoxelFragment* result = new VoxelFragment(VOXELS_PER_METER * glm::ceil(dimensions.x),
                                              VOXELS_PER_METER * glm::ceil(dimensions.y),
                                              VOXELS_PER_METER * glm::ceil(dimensions.z));

    // Constants
    const glm::vec3 center(dimensions.x / 2.0f, dimensions.y / 2.0f, 0.0);
    const glm::vec3 voxelCenter(0.5 / float(VOXELS_PER_METER), 0.5 / float(VOXELS_PER_METER), 0.5 / float(VOXELS_PER_METER));
    const float initialTreeRadius = glm::min(dimensions.x, dimensions.y) / 4.0;

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
    SDFLink trunk;
    SDFSmoothUnion smooth(0.8f);
    glm::vec3 trunkEnd = center;
    trunkEnd.z += dimensions.z / 2.0;
    SDFCylinder trunkCylinder(center, trunkEnd, initialTreeRadius * 0.9);
    SDFTransformOp trunkT;
    trunkT.addRotation(0.05, glm::vec3(1, 0, 0));
    trunkT.addRotation(0.07, glm::vec3(0, 1, 0));
    trunk.c = &smooth;
    trunk.s = &trunkCylinder;
    trunk.t = trunkT;
    treeChain.addLink(trunk);
    // Make the trunk less cylindrical
    SDFLink trunkDisplacement;
    SDFDisplace displace;
    SDFSineDisplacement barkRoughness(glm::vec3(1.0, 1.0, 0.2), 0.05);
    trunkDisplacement.c = &displace;
    trunkDisplacement.s = (SDF*)&barkRoughness;
    trunkDisplacement.t = t;
    treeChain.addLink(trunkDisplacement);


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

    return result;
}

/*
Copy voxels from src into dst at offset
*/
static void blitVoxels(VoxelChunk* dst, VoxelFragment* src, int sx, int sy, int sz) {
    for (int x = 0; (x + sx < CHUNK_WIDTH_VOXELS) && (x < src->sizeX); x++) {
        for (int y = 0; (y + sy < CHUNK_WIDTH_VOXELS) && (y < src->sizeY); y++) {
            for (int z = 0; (z + sz < CHUNK_HEIGHT_VOXELS) && (z < src->sizeZ); z++) {
                int dstIndex = (sx + x) + (sy + y) * CHUNK_WIDTH_VOXELS + (sz + z) * CHUNK_WIDTH_VOXELS * CHUNK_WIDTH_VOXELS;
                int srcIndex = x + y * src->sizeX + z * src->sizeX * src->sizeY;
                if (src->voxels[srcIndex] < 0) {
                    dst->voxels[dstIndex] = src->voxels[srcIndex];
                }
            }
        }
    }
}

static VoxelChunk* forestTest() {
    double grassHeight = 3;
    VoxelChunk* dst = grassTest(grassHeight);
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
    const glm::vec3 treeDimensions(3, 3, 10);
    VoxelFragment* src = proceduralTree(treeDimensions);
    blitVoxels(dst, src, CHUNK_WIDTH_VOXELS / 2 - (treeDimensions.x / 2) * VOXELS_PER_METER, CHUNK_WIDTH_VOXELS / 2 - (treeDimensions.y / 2) * VOXELS_PER_METER, int(grassHeight));
    delete[] src->voxels;

    return dst;
}

VoxelChunk* WorldGenerator::generateChunk() {
    VoxelChunk* result = forestTest();
    //computeDistances(result);
    return result;
}
