#pragma once

#include <steel/engine.hpp>
#include <glass/camera.hpp>
#include <glass/shader.hpp>
#include <glass/geometry.hpp>
#include <glass/material.hpp>
#include <glass/renderer.hpp>
#include <glass/world.hpp>
#include <glass/components.hpp>
#include <glass/mesh.hpp>
#include <glass/vertex.hpp>

#include <array>
#include <span>
#include <vector>

namespace voxel {

class CubeMesh : public glass::Mesh {
public:
    std::span<const glass::Vertex> vertices() const override { return vertices_; }
    std::span<const uint32_t> indices() const override { return indices_; }

private:
    // 24 vertices (4 per face, unique normals per face)
    // Each face gets a distinct color
    static constexpr std::array<glass::Vertex, 24> vertices_ = {{
        // Front face (z = +0.5) - red
        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.8f, 0.2f, 0.2f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.8f, 0.2f, 0.2f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.8f, 0.2f, 0.2f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {0.8f, 0.2f, 0.2f}},

        // Back face (z = -0.5) - green
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.2f, 0.8f, 0.2f}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.2f, 0.8f, 0.2f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.2f, 0.8f, 0.2f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.2f, 0.8f, 0.2f}},

        // Top face (y = +0.5) - blue
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.2f, 0.2f, 0.8f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.2f, 0.2f, 0.8f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.2f, 0.2f, 0.8f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.2f, 0.2f, 0.8f}},

        // Bottom face (y = -0.5) - yellow
        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.8f, 0.8f, 0.2f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {0.8f, 0.8f, 0.2f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.8f, 0.8f, 0.2f}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {0.8f, 0.8f, 0.2f}},

        // Right face (x = +0.5) - cyan
        {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.2f, 0.8f, 0.8f}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {0.2f, 0.8f, 0.8f}},
        {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {0.2f, 0.8f, 0.8f}},
        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {0.2f, 0.8f, 0.8f}},

        // Left face (x = -0.5) - magenta
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.8f, 0.2f, 0.8f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.8f, 0.2f, 0.8f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.8f, 0.2f, 0.8f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.8f, 0.2f, 0.8f}},
    }};

    // 36 indices (2 triangles per face, 6 faces, CCW winding)
    static constexpr std::array<uint32_t, 36> indices_ = {
         0,  3,  2,   2,  1,  0,  // front
         4,  7,  6,   6,  5,  4,  // back
         8, 11, 10,  10,  9,  8,  // top
        12, 15, 14,  14, 13, 12,  // bottom
        16, 19, 18,  18, 17, 16,  // right
        20, 23, 22,  22, 21, 20,  // left
    };
};

class Application {
public:
    Application();
    ~Application();

    Application(const Application&) = delete;
    auto operator=(const Application&) -> Application& = delete;
    Application(Application&&) = delete;
    auto operator=(Application&&) -> Application& = delete;

    void run();

private:
    steel::Engine engine_;
    glass::Geometry geometry_;
    glass::Material material_;
    glass::Camera camera_;
    glass::Renderer renderer_;
    glass::World world_;
};

} // namespace voxel
