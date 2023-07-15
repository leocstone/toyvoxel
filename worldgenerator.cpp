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
            for (x = std::max(0, tx - radius); (x < tx + radius) && (x < CHUNK_WIDTH_VOXELS); x++) {
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
            for (x = std::max(0, tx - radius); (x < tx + radius) && (x < CHUNK_WIDTH_VOXELS); x++) {
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
            for (x = std::max(0, tx - radius); (x < tx + radius) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (y = std::max(0, ty - radius); (y < ty + radius) && (y < CHUNK_WIDTH_VOXELS); y++) {
                    boundsCheck(x, y, z);
                    if (chunkIn->getVoxel(x, y, z) < 0)
                        return radius - 1;
                }
            }
        }
        /* -Z Plane */
        z = tz - radius;
        if (z > 0) {
            for (x = std::max(0, tx - radius); (x < tx + radius) && (x < CHUNK_WIDTH_VOXELS); x++) {
                for (y = std::max(0, ty - radius); (y < ty + radius) && (y < CHUNK_WIDTH_VOXELS); y++) {
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
        std::cout << x + 1 << " / " << CHUNK_WIDTH_METERS * VOXELS_PER_METER << std::endl;
    }
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
    computeDistances(result);

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
    //computeDistances(result);

    return result;
}

static VoxelChunk* forestTest() {
    VoxelChunk* result = new VoxelChunk();

    return result;
}

VoxelChunk* WorldGenerator::generateChunk() {
    VoxelChunk* result = basicPerlinTest();

    return result;
}
