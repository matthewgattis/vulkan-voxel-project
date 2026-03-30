#include <voxel/chunk_manager.hpp>

#include <glass/components.hpp>

#include <glm/gtc/matrix_transform.hpp>

#include <spdlog/spdlog.h>

#include <cmath>

namespace voxel {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

ChunkManager::ChunkManager(steel::Engine& engine, glass::World& world,
                           const glass::Material& material,
                           const TerrainGenerator& generator)
    : engine_{engine}
    , world_{world}
    , material_{material}
    , generator_{generator}
{
    uint32_t hw = std::thread::hardware_concurrency();
    uint32_t count = std::min(WORKER_COUNT, std::max(1u, hw > 1 ? hw - 1 : 1u));
    workers_.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        workers_.emplace_back([this](std::stop_token stop) { worker_loop(stop); });
    }
    spdlog::info("ChunkManager: {} worker threads", count);
}

ChunkManager::~ChunkManager() {
    // workers_ is destroyed first (declared last).
    // Each jthread destructor calls request_stop() + join().
    // The stop_callback in each worker notifies the CV, waking the worker to exit.
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

void ChunkManager::worker_loop(std::stop_token stop) {
    // Wake this thread when stop is requested
    std::stop_callback on_stop(stop, [this] { request_cv_.notify_all(); });

    while (!stop.stop_requested()) {
        ColumnRequest request;
        {
            std::unique_lock lock(request_mutex_);
            request_cv_.wait(lock, [&] {
                return !request_queue_.empty() || stop.stop_requested();
            });
            if (stop.stop_requested()) break;
            request = request_queue_.top();
            request_queue_.pop();
        }

        // Check if still within range
        int cx = cam_cx_.load(std::memory_order_relaxed);
        int cy = cam_cy_.load(std::memory_order_relaxed);
        if (std::abs(request.key.cx - cx) > LOAD_RADIUS ||
            std::abs(request.key.cy - cy) > LOAD_RADIUS) {
            // Push empty result so main thread clears in_flight_
            std::lock_guard lock(result_mutex_);
            result_queue_.push({request.key, {}});
            continue;
        }

        // Generate heightmap for this column, then only mesh surface slices
        ColumnResult result{request.key, {}};
        TerrainColumn column{request.key.cx, request.key.cy, generator_};

        auto opaque_query = [&](int wx, int wy, int wz) {
            return generator_.is_opaque_at(wx, wy, wz);
        };
        auto solid_query = [&](int wx, int wy, int wz) {
            return generator_.is_solid_at(wx, wy, wz);
        };

        for (int cz = column.min_slice(); cz <= column.max_slice() && !stop.stop_requested(); ++cz) {
            Chunk chunk{request.key.cx, request.key.cy, cz};
            column.fill_chunk(chunk);

            ChunkMesh mesh{chunk, opaque_query, solid_query};
            if (!mesh.empty()) {
                result.slices.push_back({cz, std::move(mesh)});
            }
        }

        {
            std::lock_guard lock(result_mutex_);
            result_queue_.push(std::move(result));
        }
    }
}

// ---------------------------------------------------------------------------
// Frustum culling
// ---------------------------------------------------------------------------

ChunkManager::Frustum ChunkManager::extract_frustum(const glm::mat4& vp) {
    Frustum planes;
    // Left, Right, Bottom, Top, Near, Far
    for (int i = 0; i < 4; ++i) {
        planes[0][i] = vp[i][3] + vp[i][0]; // left
        planes[1][i] = vp[i][3] - vp[i][0]; // right
        planes[2][i] = vp[i][3] + vp[i][1]; // bottom
        planes[3][i] = vp[i][3] - vp[i][1]; // top
        planes[4][i] = vp[i][3] + vp[i][2]; // near
        planes[5][i] = vp[i][3] - vp[i][2]; // far
    }
    // Normalize
    for (auto& p : planes) {
        float len = glm::length(glm::vec3(p));
        p /= len;
    }
    return planes;
}

bool ChunkManager::column_in_frustum(const Frustum& frustum, int cx, int cy) {
    // AABB for the column: XY from chunk coords, Z spans full possible height
    float min_x = static_cast<float>(cx * CHUNK_SIZE);
    float min_y = static_cast<float>(cy * CHUNK_SIZE);
    float min_z = TerrainGenerator::HEIGHT_AMP;
    float max_x = min_x + static_cast<float>(CHUNK_SIZE);
    float max_y = min_y + static_cast<float>(CHUNK_SIZE);
    float max_z = TerrainGenerator::HEIGHT_AMP;

    for (const auto& plane : frustum) {
        // Find the corner most in the direction of the plane normal
        float px = (plane.x > 0.0f) ? max_x : min_x;
        float py = (plane.y > 0.0f) ? max_y : min_y;
        float pz = (plane.z > 0.0f) ? max_z : min_z;

        if (plane.x * px + plane.y * py + plane.z * pz + plane.w < 0.0f) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Column unload
// ---------------------------------------------------------------------------

void ChunkManager::unload_column(const ChunkColumnKey& key) {
    auto it = columns_.find(key);
    if (it == columns_.end()) return;

    for (auto& slice : it->second.slices) {
        world_.destroy(slice.entity);
    }

    columns_.erase(it);
}

// ---------------------------------------------------------------------------
// Per-frame update (main thread)
// ---------------------------------------------------------------------------

void ChunkManager::update(const glm::vec3& camera_pos, const glm::mat4& view_projection) {
    int cam_cx = static_cast<int>(std::floor(camera_pos.x / static_cast<float>(CHUNK_SIZE)));
    int cam_cy = static_cast<int>(std::floor(camera_pos.y / static_cast<float>(CHUNK_SIZE)));

    // 1. Consume completed results from workers
    {
        std::lock_guard lock(result_mutex_);
        while (!result_queue_.empty()) {
            auto result = std::move(result_queue_.front());
            result_queue_.pop();

            in_flight_.erase(result.key);

            // Skip if out of range or empty
            if (std::abs(result.key.cx - cam_cx) > LOAD_RADIUS ||
                std::abs(result.key.cy - cam_cy) > LOAD_RADIUS) {
                continue;
            }
            if (result.slices.empty()) continue;

            // Upload to GPU and create ECS entities
            LoadedColumn column;
            for (auto& slice : result.slices) {
                auto geom = std::make_unique<glass::Geometry>(
                    glass::Geometry::create(engine_, slice.mesh));

                auto entity = world_.create();
                glm::mat4 transform = glm::translate(
                    glm::mat4{1.0f},
                    glm::vec3{
                        static_cast<float>(result.key.cx * CHUNK_SIZE),
                        static_cast<float>(result.key.cy * CHUNK_SIZE),
                        static_cast<float>(slice.cz * CHUNK_SIZE)
                    });
                world_.add<glass::Transform>(entity, glass::Transform{transform});
                world_.add<glass::GeometryComponent>(entity,
                    glass::GeometryComponent{std::move(geom)});
                world_.add<glass::MaterialComponent>(entity,
                    glass::MaterialComponent{&material_});

                column.slices.push_back({entity});
            }
            columns_.emplace(result.key, std::move(column));
        }
    }

    // 2. Unload out-of-range columns
    std::vector<ChunkColumnKey> to_unload;
    for (const auto& [key, _] : columns_) {
        if (std::abs(key.cx - cam_cx) > LOAD_RADIUS ||
            std::abs(key.cy - cam_cy) > LOAD_RADIUS) {
            to_unload.push_back(key);
        }
    }
    for (const auto& key : to_unload) {
        unload_column(key);
    }

    // 3. Update shared camera position for workers
    cam_cx_.store(cam_cx, std::memory_order_relaxed);
    cam_cy_.store(cam_cy, std::memory_order_relaxed);

    // 4. Queue columns that need loading (in-frustum first, then closest)
    {
        auto frustum = extract_frustum(view_projection);
        std::lock_guard lock(request_mutex_);
        for (int cx = cam_cx - LOAD_RADIUS; cx <= cam_cx + LOAD_RADIUS; ++cx) {
            for (int cy = cam_cy - LOAD_RADIUS; cy <= cam_cy + LOAD_RADIUS; ++cy) {
                ChunkColumnKey key{cx, cy};
                if (columns_.contains(key) || in_flight_.contains(key)) continue;

                float dx = static_cast<float>(cx - cam_cx);
                float dy = static_cast<float>(cy - cam_cy);
                bool visible = column_in_frustum(frustum, cx, cy);
                request_queue_.push({key, dx * dx + dy * dy, visible});
                in_flight_.insert(key);
            }
        }
    }
    request_cv_.notify_all();
}

} // namespace voxel
