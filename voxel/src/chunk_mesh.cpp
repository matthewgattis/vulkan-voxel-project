#include <voxel/chunk_mesh.hpp>

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

                    auto base = static_cast<uint32_t>(vertices_.size());
                    auto normal = normals[face];

                    // 4 vertices per face, color modulated by AO
                    switch (face) {
                        case 0: // +X
                            vertices_.push_back({{fx+1, fy,   fz  }, normal, color * bright[0]});
                            vertices_.push_back({{fx+1, fy+1, fz  }, normal, color * bright[1]});
                            vertices_.push_back({{fx+1, fy+1, fz+1}, normal, color * bright[2]});
                            vertices_.push_back({{fx+1, fy,   fz+1}, normal, color * bright[3]});
                            break;
                        case 1: // -X
                            vertices_.push_back({{fx, fy+1, fz  }, normal, color * bright[0]});
                            vertices_.push_back({{fx, fy,   fz  }, normal, color * bright[1]});
                            vertices_.push_back({{fx, fy,   fz+1}, normal, color * bright[2]});
                            vertices_.push_back({{fx, fy+1, fz+1}, normal, color * bright[3]});
                            break;
                        case 2: // +Y
                            vertices_.push_back({{fx+1, fy+1, fz  }, normal, color * bright[0]});
                            vertices_.push_back({{fx,   fy+1, fz  }, normal, color * bright[1]});
                            vertices_.push_back({{fx,   fy+1, fz+1}, normal, color * bright[2]});
                            vertices_.push_back({{fx+1, fy+1, fz+1}, normal, color * bright[3]});
                            break;
                        case 3: // -Y
                            vertices_.push_back({{fx,   fy, fz  }, normal, color * bright[0]});
                            vertices_.push_back({{fx+1, fy, fz  }, normal, color * bright[1]});
                            vertices_.push_back({{fx+1, fy, fz+1}, normal, color * bright[2]});
                            vertices_.push_back({{fx,   fy, fz+1}, normal, color * bright[3]});
                            break;
                        case 4: // +Z (top)
                            vertices_.push_back({{fx,   fy,   fz+1}, normal, color * bright[0]});
                            vertices_.push_back({{fx+1, fy,   fz+1}, normal, color * bright[1]});
                            vertices_.push_back({{fx+1, fy+1, fz+1}, normal, color * bright[2]});
                            vertices_.push_back({{fx,   fy+1, fz+1}, normal, color * bright[3]});
                            break;
                        case 5: // -Z (bottom)
                            vertices_.push_back({{fx,   fy+1, fz}, normal, color * bright[0]});
                            vertices_.push_back({{fx+1, fy+1, fz}, normal, color * bright[1]});
                            vertices_.push_back({{fx+1, fy,   fz}, normal, color * bright[2]});
                            vertices_.push_back({{fx,   fy,   fz}, normal, color * bright[3]});
                            break;
                    }

                    // Triangulation: flip diagonal based on AO to avoid artifacts.
                    // Standard: 0-3-2, 2-1-0.  Flipped: 1-0-3, 3-2-1.
                    if (ao[0] + ao[2] > ao[1] + ao[3]) {
                        indices_.push_back(base);
                        indices_.push_back(base + 3);
                        indices_.push_back(base + 2);
                        indices_.push_back(base + 2);
                        indices_.push_back(base + 1);
                        indices_.push_back(base);
                    } else {
                        indices_.push_back(base + 1);
                        indices_.push_back(base);
                        indices_.push_back(base + 3);
                        indices_.push_back(base + 3);
                        indices_.push_back(base + 2);
                        indices_.push_back(base + 1);
                    }
                }
            }
        }
    }
}

} // namespace voxel
