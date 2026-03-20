#pragma once

#include <steel/engine.hpp>
#include <glass/shader.hpp>
#include <glass/geometry.hpp>
#include <glass/material.hpp>
#include <glass/renderable.hpp>
#include <glass/renderer.hpp>
#include <glass/scene_node.hpp>
#include <glass/mesh.hpp>
#include <glass/vertex.hpp>

#include <array>
#include <span>
#include <vector>

namespace voxel {

class TriangleMesh : public glass::Mesh {
public:
    std::span<const glass::Vertex> vertices() const override { return vertices_; }
    std::span<const uint32_t> indices() const override { return indices_; }

private:
    static constexpr std::array<glass::Vertex, 3> vertices_ = {{
        {{ 0.0f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}},  // top    - red
        {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f}},   // right  - green
        {{-0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},   // left   - blue
    }};
    static constexpr std::array<uint32_t, 3> indices_ = {0, 1, 2};
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
    glass::Renderable renderable_;
    glass::Renderer renderer_;
    glass::SceneNode scene_;
};

} // namespace voxel
