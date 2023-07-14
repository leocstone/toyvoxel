#include "worldgenerator.h"

VoxelChunk* WorldGenerator::generateChunk()
{
    VoxelChunk* result = new VoxelChunk();

    for(int x = 0; x < CHUNK_WIDTH_METERS * VOXELS_PER_METER; x++) {
            for(int y = 0; y < CHUNK_WIDTH_METERS * VOXELS_PER_METER; y++) {
                for(int z = 0; z < CHUNK_HEIGHT_METERS * VOXELS_PER_METER; z++) {
                    constexpr double scale_factor = 32.0;
                    constexpr double scale_factor_surface = 32.0;
                    if (z >= 60.0 * VOXELS_PER_METER) {
                        double height = Perlin::perlin(double(x) / scale_factor_surface, double(y) / scale_factor_surface, double(60 * VOXELS_PER_METER) / scale_factor_surface) * 4.0 + 60.0 * VOXELS_PER_METER;
                        result->voxels[x][y][z] = (z <= height);
                    } else if (z >= 53.0 * VOXELS_PER_METER) {
                        result->voxels[x][y][z] = 1;
                    } else {
                        result->voxels[x][y][z] = (Perlin::perlin(double(x) / scale_factor, double(y) / scale_factor, double(z) / scale_factor) < 0.5);
                    }
                }
            }
        }

    return result;
}
