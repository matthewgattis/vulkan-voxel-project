#pragma once

#include <voxel/chunk.hpp>

#include <glass/geometry.hpp>
#include <glass/material.hpp>
#include <glass/world.hpp>
#include <steel/engine.hpp>

#include <memory>
#include <vector>

namespace voxel {

class Terrain {
public:
    Terrain(steel::Engine& engine, glass::World& world, const glass::Material& material);

private:
    static constexpr int GRID_X = 4;
    static constexpr int GRID_Y = 4;
    static constexpr float NOISE_SCALE = 0.05f;
    static constexpr float HEIGHT_BASE = 16.0f;
    static constexpr float HEIGHT_AMP = 12.0f;

    static float terrain_height(float wx, float wy);
    static bool is_solid_at(int wx, int wy, int wz);
    static void fill_chunk(Chunk& chunk);

    std::vector<std::unique_ptr<glass::Geometry>> geometries_;
};

} // namespace voxel
