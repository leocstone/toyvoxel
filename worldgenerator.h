#ifndef WORLDGENERATOR_H
#define WORLDGENERATOR_H
#include <cstdint>
#include <algorithm>
#include "perlin.h"
#include "sdf/sdfchain.h"
#include "sdf/primitive.h"
#include "sdf/displacement.h"
#include "sdf/displacedsdf.h"

/*
Structure:
if < 0, voxel id is value * -1
if > 0, value is distance to nearest voxel
if 0, there is no voxel here and no distance information
*/
typedef int8_t Voxel;

enum VoxelID {
    Stone = 1,
    Dirt = 2,
    Grass = 3,
    Bark = 4,
    Wood = 5,
    Glass = 6
};

// A chunk is 16x16 meters XY
constexpr int CHUNK_WIDTH_METERS = 16;
// and 256 meters tall
constexpr int CHUNK_HEIGHT_METERS = 16;
// There are 8 voxels in 1 meter
constexpr int VOXELS_PER_METER = 16;

constexpr int CHUNK_WIDTH_VOXELS = CHUNK_WIDTH_METERS * VOXELS_PER_METER;
constexpr int CHUNK_HEIGHT_VOXELS = CHUNK_HEIGHT_METERS * VOXELS_PER_METER;

constexpr size_t CHUNK_SIZE_BYTES = sizeof(Voxel) * CHUNK_WIDTH_VOXELS * CHUNK_WIDTH_VOXELS * CHUNK_HEIGHT_VOXELS;

/* Number of chunks around the player to load */
const int DRAW_DISTANCE = 1;
const int LOADED_CHUNKS_AXIS = DRAW_DISTANCE * 2 + 1;
const int TOTAL_CHUNKS_LOADED = LOADED_CHUNKS_AXIS * LOADED_CHUNKS_AXIS;

struct LoadedChunks {
    int8_t voxels[LOADED_CHUNKS_AXIS * CHUNK_WIDTH_VOXELS][LOADED_CHUNKS_AXIS * CHUNK_WIDTH_VOXELS][CHUNK_HEIGHT_VOXELS];
    Voxel getVoxel(int chunkX, int chunkY, int x, int y, int z) {
        return voxels[chunkX * CHUNK_WIDTH_VOXELS + x][chunkY * CHUNK_WIDTH_VOXELS + y][z];
    }
    void setVoxel(int chunkX, int chunkY, int x, int y, int z, Voxel v) {
        voxels[chunkX * CHUNK_WIDTH_VOXELS + x][chunkY * CHUNK_WIDTH_VOXELS + y][z] = v;
    }
};

struct VoxelChunk {
    LoadedChunks* world;
    int chunkX;
    int chunkY;

    VoxelChunk(LoadedChunks* _world, int _chunkX, int _chunkY) {
        world = _world;
        chunkX = _chunkX;
        chunkY = _chunkY;
    }

    Voxel getVoxel(int x, int y, int z) {
        return world->getVoxel(chunkX, chunkY, x, y, z);
    }
    void setVoxel(int x, int y, int z, const Voxel& v) {
        world->setVoxel(chunkX, chunkY, x, y, z, v);
    }
};

struct VoxelFragment {
    Voxel* voxels;
    int sizeX;
    int sizeY;
    int sizeZ;

    VoxelFragment(int sx, int sy, int sz) {
        voxels = new Voxel[sx*sy*sz];
        sizeX = sx;
        sizeY = sy;
        sizeZ = sz;
    }
    ~VoxelFragment() {
        //delete[] voxels;
    }
    void freeVoxels() { delete[] voxels; }
    Voxel getVoxel(int x, int y, int z) {
        return voxels[x + y * sizeX + z * sizeX * sizeY];
    }
    void setVoxel(int x, int y, int z, const Voxel& v) {
        voxels[x + y * sizeX + z * sizeX * sizeY] = v;
    }
};

class WorldGenerator
{
public:
    WorldGenerator() {}

    static void generateChunk(VoxelChunk* result, int chunkX, int chunkY);

private:
};

#endif // WORLDGENERATOR_H
