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

static VoxelChunk* grassTest(double minHeight) {
    VoxelChunk* result = new VoxelChunk();
    constexpr double scale_factor = 32.0;
    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            for (int z = 0; z < CHUNK_HEIGHT_VOXELS; z++) {
                double height = Perlin::octavePerlin(double(x) / scale_factor, double(y) / scale_factor, 55.0, 16, 0.5) * 3.0 + minHeight;
                result->setVoxel(x, y, z, (z <= height) ? -3 : 0);
            }
        }
        std::cout << "\rGenerating grass voxels: " << x + 1 << " / " << CHUNK_WIDTH_VOXELS;
    }
    std::cout << " done." << std::endl;

    return result;
}

static float distanceFromCylinder(const glm::vec3& origin, float radius, const glm::vec3& point) {
    // Disregard Z
    glm::vec3 originXY = origin;
    originXY.z = 0.0;
    glm::vec3 pointXY = point;
    pointXY.z = 0.0;
    glm::vec3 s = glm::vec3(pointXY - originXY);
    float result = glm::length(s) - radius;
    //std::cout << "point-origin: (" << s.x << ", " << s.y << ", " << s.z << ") length=" << glm::length(s) << ". Distance from (" << point.x << ", " << point.y << ", " << point.z << "): " << result << std::endl;
    return result;
}

static float distanceFromSphere(const glm::vec3& origin, float radius, const glm::vec3& point) {
    return glm::length(point - origin) - radius;
}

/* Copied from iq */
static float sdCappedCylinder( glm::vec3 p, glm::vec3 a, glm::vec3 b, float r )
{
  glm::vec3  ba = b - a;
  glm::vec3  pa = p - a;
  float baba = glm::dot(ba,ba);
  float paba = glm::dot(pa,ba);
  float x = glm::length(pa*baba-ba*paba) - r*baba;
  float y = glm::abs(paba-baba*0.5)-baba*0.5;
  float x2 = x*x;
  float y2 = y*y*baba;
  float d = (glm::max(x,y)<0.0)?-glm::min(x2,y2):(((x>0.0)?x2:0.0)+((y>0.0)?y2:0.0));
  return glm::sign(d)*glm::sqrt(glm::abs(d))/baba;
}

static float horizontalCylinder(const glm::vec3& origin, float radius, const glm::vec3& point) {
    return 0;
}

static float curvedSharpCylinder(const glm::vec3& a, const glm::vec3& b, float radius, const glm::vec3& point,
                                 float curveFactor, const glm::vec3& curveDirection, float sharpness) {
    glm::vec3 transformedPoint = point;

    glm::vec3 b_a = b - a;
    glm::vec3 p_a = point - a;
    float distanceTraveledAlongCylinder = 0.0f;

    return sdCappedCylinder(transformedPoint, a, b, radius);
}

/*
Copied from iq
Smoothed union of two SDFS
d1 - distance to SDF 1
d2 - distance to SDF 2
k  - smoothing amount
*/
static float opSmoothUnion( float d1, float d2, float k ) {
    float h = glm::clamp( 0.5 + 0.5*(d2-d1)/k, 0.0, 1.0 );
    return glm::mix( d2, d1, h ) - k*h*(1.0-h);
}

/*
Tree
*/
static VoxelFragment* tree() {
    const float initialTrunkRadius = 1.0f * VOXELS_PER_METER;
    const int height = 10;
    float radius = initialTrunkRadius;

    VoxelFragment* result = new VoxelFragment(VOXELS_PER_METER * 4, VOXELS_PER_METER * 4, VOXELS_PER_METER * height);

    glm::vec3 origin(result->sizeX / 2.0f, result->sizeY / 2.0f, 0.0f);
    glm::vec3 halfVoxel(0.5f, 0.5f, 0.5f);
    float curRotation = 0.02f;
    //std::cout << "Cylinder is at (" << origin.x << ", " << origin.y << ", " << origin.z << ") with radius " << initialTrunkRadius << std::endl;
    for (int x = 0; x < result->sizeX; x++) {
        for (int y = 0; y < result->sizeY; y++) {
            for (int z = 0; z < result->sizeZ; z++) {
                float curRadius = initialTrunkRadius;
                if (z > 100) {
                    curRadius = initialTrunkRadius * 0.9;
                }
                glm::vec4 curPoint(glm::vec3(x, y, z) + halfVoxel, 1);
                glm::mat4 r = glm::mat4(1.0);
                r = glm::rotate(r, curRotation, glm::vec3(0, 1, 0));
                curPoint = r * curPoint;
                float distSample = opSmoothUnion(opSmoothUnion(distanceFromSphere(glm::vec3(origin.x, origin.y - initialTrunkRadius + 1.0, 50.0), 3.0, curPoint),
                                                               distanceFromSphere(glm::vec3(origin.x, origin.y, 0), initialTrunkRadius * 1.1, curPoint), 0.8),
                                                 opSmoothUnion(distanceFromCylinder(origin, curRadius, curPoint),
                                                               distanceFromSphere(glm::vec3(origin.x, origin.y, 100.0), initialTrunkRadius * 1.02, curPoint), 0.8), 0.8);
                if (distSample < 0.0f && distSample > -1.8f) {
                    result->setVoxel(x, y, z, -4);
                } else if (distSample < -1.8f) {
                    result->setVoxel(x, y, z, -5);
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

    VoxelFragment* src = tree();
    blitVoxels(dst, src, CHUNK_WIDTH_VOXELS / 2, CHUNK_WIDTH_VOXELS / 2, int(grassHeight));
    delete[] src->voxels;

    return dst;
}

VoxelChunk* WorldGenerator::generateChunk() {
    VoxelChunk* result = forestTest();
    //computeDistances(result);
    return result;
}
