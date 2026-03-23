#pragma once

#include <glm/vec3.hpp>

namespace voxel {

enum class VoxelType : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
    Sand,
    Water,
};

inline bool is_solid(VoxelType type) {
    return type != VoxelType::Air && type != VoxelType::Water;
}

inline bool is_opaque(VoxelType type) {
    return type != VoxelType::Air;
}

inline glm::vec3 voxel_color(VoxelType type) {
    switch (type) {
        case VoxelType::Grass: return {0.35f, 0.70f, 0.25f};
        case VoxelType::Dirt:  return {0.55f, 0.36f, 0.20f};
        case VoxelType::Stone: return {0.55f, 0.55f, 0.55f};
        case VoxelType::Sand:  return {0.85f, 0.78f, 0.55f};
        case VoxelType::Water: return {0.20f, 0.40f, 0.75f};
        default:               return {0.0f, 0.0f, 0.0f};
    }
}

} // namespace voxel
