#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H
#include <cstdint>
#include <algorithm>
#include "perlin.h"

/*
Structure:
if < 0, voxel id is value * -1
if > 0, value is distance to nearest voxel
if 0, there is no voxel here and no distance information
*/
typedef int32_t Voxel;

// A chunk is 16x16 meters XY
constexpr int CHUNK_WIDTH_METERS = 48;
// and 256 meters tall
constexpr int CHUNK_HEIGHT_METERS = 8;
// There are 8 voxels in 1 meter
constexpr int VOXELS_PER_METER = 8;

constexpr int CHUNK_WIDTH_VOXELS = CHUNK_WIDTH_METERS * VOXELS_PER_METER;
constexpr int CHUNK_HEIGHT_VOXELS = CHUNK_HEIGHT_METERS * VOXELS_PER_METER;

struct VoxelChunk {
    Voxel voxels[CHUNK_WIDTH_VOXELS * CHUNK_WIDTH_VOXELS * CHUNK_HEIGHT_VOXELS];
    Voxel getVoxel(int x, int y, int z) {
        return voxels[x + y * CHUNK_WIDTH_VOXELS + z * CHUNK_WIDTH_VOXELS * CHUNK_WIDTH_VOXELS];
    }
    void setVoxel(int x, int y, int z, const Voxel& v) {
        voxels[x + y * CHUNK_WIDTH_VOXELS + z * CHUNK_WIDTH_VOXELS * CHUNK_WIDTH_VOXELS] = v;
    }
};

class WorldGenerator
{
public:
    WorldGenerator() {}

    static VoxelChunk* generateChunk();

private:
};

#endif // WORLDGENERATOR_H
