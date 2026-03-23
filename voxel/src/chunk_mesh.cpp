#include <voxel/chunk_mesh.hpp>

#include <cstring>
#include <unordered_map>

namespace voxel {

// Compute AO value for a vertex given two side neighbors and one corner neighbor.
// Returns 0 (darkest) to 3 (brightest).
static int vertex_ao(bool side1, bool side2, bool corner) {
    if (side1 && side2) return 0;
    return 3 - (static_cast<int>(side1) + static_cast<int>(side2) + static_cast<int>(corner));
}

// AO brightness mapping: 0 -> 0.25, 1 -> 0.5, 2 -> 0.75, 3 -> 1.0
static float ao_brightness(int ao) {
    static constexpr float table[] = {0.25f, 0.5f, 0.75f, 1.0f};
    return table[ao];
}

// For each face direction (6) and each vertex (4), the 3 neighbor offsets
// relative to the voxel position for AO: {side1, side2, corner}.
// Each offset is {dx, dy, dz}.
struct AONeighbors {
    int s1[3], s2[3], c[3];
};

// clang-format off
static constexpr AONeighbors ao_table[6][4] = {
    // Face 0: +X  (vertices: v0(x+1,y,z), v1(x+1,y+1,z), v2(x+1,y+1,z+1), v3(x+1,y,z+1))
    {{{1,0,-1}, {1,-1,0}, {1,-1,-1}},
     {{1,0,-1}, {1,+1,0}, {1,+1,-1}},
     {{1,0,+1}, {1,+1,0}, {1,+1,+1}},
     {{1,0,+1}, {1,-1,0}, {1,-1,+1}}},

    // Face 1: -X  (vertices: v0(x,y+1,z), v1(x,y,z), v2(x,y,z+1), v3(x,y+1,z+1))
    {{{-1,0,-1}, {-1,+1,0}, {-1,+1,-1}},
     {{-1,0,-1}, {-1,-1,0}, {-1,-1,-1}},
     {{-1,0,+1}, {-1,-1,0}, {-1,-1,+1}},
     {{-1,0,+1}, {-1,+1,0}, {-1,+1,+1}}},

    // Face 2: +Y  (vertices: v0(x+1,y+1,z), v1(x,y+1,z), v2(x,y+1,z+1), v3(x+1,y+1,z+1))
    {{{0,+1,-1}, {+1,+1,0}, {+1,+1,-1}},
     {{0,+1,-1}, {-1,+1,0}, {-1,+1,-1}},
     {{0,+1,+1}, {-1,+1,0}, {-1,+1,+1}},
     {{0,+1,+1}, {+1,+1,0}, {+1,+1,+1}}},

    // Face 3: -Y  (vertices: v0(x,y,z), v1(x+1,y,z), v2(x+1,y,z+1), v3(x,y,z+1))
    {{{0,-1,-1}, {-1,-1,0}, {-1,-1,-1}},
     {{0,-1,-1}, {+1,-1,0}, {+1,-1,-1}},
     {{0,-1,+1}, {+1,-1,0}, {+1,-1,+1}},
     {{0,-1,+1}, {-1,-1,0}, {-1,-1,+1}}},

    // Face 4: +Z  (vertices: v0(x,y,z+1), v1(x+1,y,z+1), v2(x+1,y+1,z+1), v3(x,y+1,z+1))
    {{{-1,0,+1}, {0,-1,+1}, {-1,-1,+1}},
     {{+1,0,+1}, {0,-1,+1}, {+1,-1,+1}},
     {{+1,0,+1}, {0,+1,+1}, {+1,+1,+1}},
     {{-1,0,+1}, {0,+1,+1}, {-1,+1,+1}}},

    // Face 5: -Z  (vertices: v0(x,y+1,z), v1(x+1,y+1,z), v2(x+1,y,z), v3(x,y,z))
    {{{-1,0,-1}, {0,+1,-1}, {-1,+1,-1}},
     {{+1,0,-1}, {0,+1,-1}, {+1,+1,-1}},
     {{+1,0,-1}, {0,-1,-1}, {+1,-1,-1}},
     {{-1,0,-1}, {0,-1,-1}, {-1,-1,-1}}},
};
// clang-format on

// ---------------------------------------------------------------------------
// Vertex welding: hash by raw bytes of (position, normal, color)
// ---------------------------------------------------------------------------

struct VertexHash {
    size_t operator()(const glass::Vertex& v) const {
        // Hash the raw bytes — Vertex is 36 bytes, tightly packed
        static_assert(sizeof(glass::Vertex) == 36);
        const auto* data = reinterpret_cast<const uint8_t*>(&v);
        // FNV-1a
        size_t hash = 14695981039346656037ULL;
        for (size_t i = 0; i < sizeof(glass::Vertex); ++i) {
            hash ^= data[i];
            hash *= 1099511628211ULL;
        }
        return hash;
    }
};

struct VertexEqual {
    bool operator()(const glass::Vertex& a, const glass::Vertex& b) const {
        return std::memcmp(&a, &b, sizeof(glass::Vertex)) == 0;
    }
};

// ---------------------------------------------------------------------------
// Mesh construction
// ---------------------------------------------------------------------------

ChunkMesh::ChunkMesh(const Chunk& chunk, const SolidQuery& is_solid_at) {
    // Direction offsets: +X, -X, +Y, -Y, +Z, -Z
    static constexpr int dx[] = { 1, -1,  0,  0,  0,  0};
    static constexpr int dy[] = { 0,  0,  1, -1,  0,  0};
    static constexpr int dz[] = { 0,  0,  0,  0,  1, -1};

    // Normals for each face direction
    static constexpr glm::vec3 normals[] = {
        { 1.0f,  0.0f,  0.0f},  // +X
        {-1.0f,  0.0f,  0.0f},  // -X
        { 0.0f,  1.0f,  0.0f},  // +Y
        { 0.0f, -1.0f,  0.0f},  // -Y
        { 0.0f,  0.0f,  1.0f},  // +Z
        { 0.0f,  0.0f, -1.0f},  // -Z
    };

    // World-space origin of this chunk
    int ox = chunk.cx() * CHUNK_SIZE;
    int oy = chunk.cy() * CHUNK_SIZE;
    int oz = chunk.cz() * CHUNK_SIZE;

    // Vertex deduplication map
    std::unordered_map<glass::Vertex, uint32_t, VertexHash, VertexEqual> vertex_map;

    auto emit_vertex = [&](const glass::Vertex& v) -> uint32_t {
        auto [it, inserted] = vertex_map.emplace(v, static_cast<uint32_t>(vertices_.size()));
        if (inserted) {
            vertices_.push_back(v);
        }
        return it->second;
    };

    for (int z = 0; z < CHUNK_SIZE; ++z) {
        for (int y = 0; y < CHUNK_SIZE; ++y) {
            for (int x = 0; x < CHUNK_SIZE; ++x) {
                auto type = chunk.get(x, y, z);
                if (!is_solid(type)) continue;

                auto color = voxel_color(type);
                auto fx = static_cast<float>(x);
                auto fy = static_cast<float>(y);
                auto fz = static_cast<float>(z);

                // World coords of this voxel
                int wx = ox + x;
                int wy = oy + y;
                int wz = oz + z;

                for (int face = 0; face < 6; ++face) {
                    // Check neighbor — use world-space query for cross-chunk culling
                    int nwx = wx + dx[face];
                    int nwy = wy + dy[face];
                    int nwz = wz + dz[face];

                    if (is_solid_at(nwx, nwy, nwz)) continue;

                    // Compute per-vertex AO
                    int ao[4];
                    for (int v = 0; v < 4; ++v) {
                        const auto& n = ao_table[face][v];
                        bool s1 = is_solid_at(wx + n.s1[0], wy + n.s1[1], wz + n.s1[2]);
                        bool s2 = is_solid_at(wx + n.s2[0], wy + n.s2[1], wz + n.s2[2]);
                        bool c  = is_solid_at(wx + n.c[0],  wy + n.c[1],  wz + n.c[2]);
                        ao[v] = vertex_ao(s1, s2, c);
                    }

                    float bright[4];
                    for (int v = 0; v < 4; ++v) {
                        bright[v] = ao_brightness(ao[v]);
                    }

                    auto normal = normals[face];

                    // Build 4 vertices, deduplicate via emit_vertex
                    uint32_t idx[4];
                    switch (face) {
                        case 0: // +X
                            idx[0] = emit_vertex({{fx+1, fy,   fz  }, normal, color * bright[0]});
                            idx[1] = emit_vertex({{fx+1, fy+1, fz  }, normal, color * bright[1]});
                            idx[2] = emit_vertex({{fx+1, fy+1, fz+1}, normal, color * bright[2]});
                            idx[3] = emit_vertex({{fx+1, fy,   fz+1}, normal, color * bright[3]});
                            break;
                        case 1: // -X
                            idx[0] = emit_vertex({{fx, fy+1, fz  }, normal, color * bright[0]});
                            idx[1] = emit_vertex({{fx, fy,   fz  }, normal, color * bright[1]});
                            idx[2] = emit_vertex({{fx, fy,   fz+1}, normal, color * bright[2]});
                            idx[3] = emit_vertex({{fx, fy+1, fz+1}, normal, color * bright[3]});
                            break;
                        case 2: // +Y
                            idx[0] = emit_vertex({{fx+1, fy+1, fz  }, normal, color * bright[0]});
                            idx[1] = emit_vertex({{fx,   fy+1, fz  }, normal, color * bright[1]});
                            idx[2] = emit_vertex({{fx,   fy+1, fz+1}, normal, color * bright[2]});
                            idx[3] = emit_vertex({{fx+1, fy+1, fz+1}, normal, color * bright[3]});
                            break;
                        case 3: // -Y
                            idx[0] = emit_vertex({{fx,   fy, fz  }, normal, color * bright[0]});
                            idx[1] = emit_vertex({{fx+1, fy, fz  }, normal, color * bright[1]});
                            idx[2] = emit_vertex({{fx+1, fy, fz+1}, normal, color * bright[2]});
                            idx[3] = emit_vertex({{fx,   fy, fz+1}, normal, color * bright[3]});
                            break;
                        case 4: // +Z (top)
                            idx[0] = emit_vertex({{fx,   fy,   fz+1}, normal, color * bright[0]});
                            idx[1] = emit_vertex({{fx+1, fy,   fz+1}, normal, color * bright[1]});
                            idx[2] = emit_vertex({{fx+1, fy+1, fz+1}, normal, color * bright[2]});
                            idx[3] = emit_vertex({{fx,   fy+1, fz+1}, normal, color * bright[3]});
                            break;
                        case 5: // -Z (bottom)
                            idx[0] = emit_vertex({{fx,   fy+1, fz}, normal, color * bright[0]});
                            idx[1] = emit_vertex({{fx+1, fy+1, fz}, normal, color * bright[1]});
                            idx[2] = emit_vertex({{fx+1, fy,   fz}, normal, color * bright[2]});
                            idx[3] = emit_vertex({{fx,   fy,   fz}, normal, color * bright[3]});
                            break;
                    }

                    // Triangulation: flip diagonal based on AO to avoid artifacts.
                    // Standard: 0-3-2, 2-1-0.  Flipped: 1-0-3, 3-2-1.
                    if (ao[0] + ao[2] > ao[1] + ao[3]) {
                        indices_.push_back(idx[0]);
                        indices_.push_back(idx[3]);
                        indices_.push_back(idx[2]);
                        indices_.push_back(idx[2]);
                        indices_.push_back(idx[1]);
                        indices_.push_back(idx[0]);
                    } else {
                        indices_.push_back(idx[1]);
                        indices_.push_back(idx[0]);
                        indices_.push_back(idx[3]);
                        indices_.push_back(idx[3]);
                        indices_.push_back(idx[2]);
                        indices_.push_back(idx[1]);
                    }
                }
            }
        }
    }
}

} // namespace voxel
