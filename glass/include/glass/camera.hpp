#pragma once

#include <glm/glm.hpp>

namespace glass {

class Camera {
public:
    Camera(float fov_degrees, float aspect_ratio, float near_plane, float far_plane);

    void set_position(const glm::vec3& position);
    void look_at(const glm::vec3& target);
    void set_aspect_ratio(float aspect_ratio);

    const glm::vec3& position() const { return position_; }
    const glm::mat4& view() const { return view_; }
    const glm::mat4& projection() const { return projection_; }
    glm::mat4 view_projection() const { return projection_ * view_; }

private:
    void update_view();
    void update_projection();

    glm::vec3 position_{0.0f, 0.0f, 0.0f};
    glm::vec3 target_{0.0f, 0.0f, -1.0f};
    glm::vec3 up_{0.0f, 1.0f, 0.0f};

    float fov_radians_;
    float aspect_ratio_;
    float near_plane_;
    float far_plane_;

    glm::mat4 view_{1.0f};
    glm::mat4 projection_{1.0f};
};

} // namespace glass
