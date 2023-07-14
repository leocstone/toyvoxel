#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H
#include <cstdint>
#include "perlin.h"

/*
Structure:
bit 31: if 1, there is a voxel here. else, this is the number of voxels to the nearest voxel.
bits 30-0: voxel ID or distance to nearest voxel (in voxels)
*/
typedef int32_t Voxel;

// A chunk is 16x16 meters XY
constexpr int CHUNK_WIDTH_METERS = 32;
// and 256 meters tall
constexpr int CHUNK_HEIGHT_METERS = 256;
// There are 8 voxels in 1 meter
constexpr int VOXELS_PER_METER = 8;

constexpr int CHUNK_WIDTH_VOXELS = CHUNK_WIDTH_METERS * VOXELS_PER_METER;
constexpr int CHUNK_HEIGHT_VOXELS = CHUNK_HEIGHT_METERS * VOXELS_PER_METER;

struct VoxelChunk {
    alignas(4) Voxel voxels[CHUNK_WIDTH_METERS * VOXELS_PER_METER][CHUNK_WIDTH_METERS * VOXELS_PER_METER][CHUNK_HEIGHT_METERS * VOXELS_PER_METER];
};

class WorldGenerator
{
public:
    WorldGenerator() {}

    static VoxelChunk* generateChunk();

private:
};

#endif // WORLDGENERATOR_H
