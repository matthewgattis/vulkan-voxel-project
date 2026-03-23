#pragma once

#include <voxel/chunk.hpp>
#include <voxel/chunk_mesh.hpp>
#include <voxel/terrain_generator.hpp>

#include <glass/material.hpp>
#include <glass/world.hpp>
#include <steel/engine.hpp>

#include <glm/vec3.hpp>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace voxel {

struct ChunkColumnKey {
    int cx, cy;
    bool operator==(const ChunkColumnKey&) const = default;
};

struct ChunkColumnKeyHash {
    size_t operator()(const ChunkColumnKey& k) const {
        auto h1 = std::hash<int>{}(k.cx);
        auto h2 = std::hash<int>{}(k.cy);
        return h1 ^ (h2 * 2654435761u);
    }
};

class ChunkManager {
public:
    ChunkManager(steel::Engine& engine, glass::World& world,
                 const glass::Material& material, const TerrainGenerator& generator);
    ~ChunkManager();

    void update(const glm::vec3& camera_pos);

private:
    static constexpr int LOAD_RADIUS = 32;
    static constexpr uint32_t WORKER_COUNT = 4;

    // --- Loaded column data ---

    struct SliceEntry {
        glass::Entity entity;
    };

    struct LoadedColumn {
        std::vector<SliceEntry> slices;
    };

    void unload_column(const ChunkColumnKey& key);

    // --- Worker thread ---

    void worker_loop(std::stop_token stop);

    // Request: queued for workers (min-heap by distance)
    struct ColumnRequest {
        ChunkColumnKey key;
        float distance_sq;
        bool operator>(const ColumnRequest& o) const { return distance_sq > o.distance_sq; }
    };

    // Result: worker → main thread (CPU-side mesh data)
    struct ColumnResult {
        ChunkColumnKey key;
        struct SliceData {
            int cz;
            ChunkMesh mesh;
        };
        std::vector<SliceData> slices;
    };

    // --- Members ---

    steel::Engine& engine_;
    glass::World& world_;
    const glass::Material& material_;
    const TerrainGenerator& generator_;

    // Loaded columns (main thread only)
    std::unordered_map<ChunkColumnKey, LoadedColumn, ChunkColumnKeyHash> columns_;
    std::unordered_set<ChunkColumnKey, ChunkColumnKeyHash> in_flight_;

    // Request queue (shared, mutex-protected)
    std::priority_queue<ColumnRequest, std::vector<ColumnRequest>, std::greater<>> request_queue_;
    std::mutex request_mutex_;
    std::condition_variable request_cv_;

    // Result queue (shared, mutex-protected)
    std::queue<ColumnResult> result_queue_;
    std::mutex result_mutex_;

    // Camera position for worker relevance checks
    std::atomic<int> cam_cx_{0};
    std::atomic<int> cam_cy_{0};

    // Worker threads (must be declared last — destroyed first)
    std::vector<std::jthread> workers_;
};

} // namespace voxel
