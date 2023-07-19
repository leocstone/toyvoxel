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

static VoxelChunk* axes() {
    VoxelChunk* result = new VoxelChunk();
    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            for (int z = 0; z < CHUNK_HEIGHT_VOXELS; z++) {
                if (z == 0 || x == 0 || y == 0) {
                    result->setVoxel(x, y, z, -1);
                } else {
                    result->setVoxel(x, y, z, 0);
                }
            }
        }
    }

    return result;
}

static VoxelChunk* grassTest() {
    VoxelChunk* result = new VoxelChunk();
    constexpr double scale_factor = 32.0;
    for (int x = 0; x < CHUNK_WIDTH_VOXELS; x++) {
        for (int y = 0; y < CHUNK_WIDTH_VOXELS; y++) {
            for (int z = 0; z < CHUNK_HEIGHT_VOXELS; z++) {
                double height = CHUNK_HEIGHT_VOXELS - Perlin::octavePerlin(double(x) / scale_factor, double(y) / scale_factor, 55.0, 16, 0.5) * 1.5 * VOXELS_PER_METER - 2.0 * VOXELS_PER_METER;
                result->setVoxel(x, y, z, (z <= height) ? -3 : 0);
            }
        }
        std::cout << "\rGenerating grass voxels: " << x + 1 << " / " << CHUNK_WIDTH_VOXELS;
    }
    std::cout << " done." << std::endl;

    return result;
}

static VoxelChunk* forestTest() {
    VoxelChunk* result = new VoxelChunk();

    return result;
}

VoxelChunk* WorldGenerator::generateChunk() {
    VoxelChunk* result = grassTest();
    //computeDistances(result);
    return result;
}
