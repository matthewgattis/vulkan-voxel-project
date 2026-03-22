#pragma once

#include <glm/glm.hpp>

namespace glass {

class Camera {
public:
    Camera(float fov_degrees, float aspect_ratio, float near_plane, float far_plane);

    void set_aspect_ratio(float aspect_ratio);

    float fov_degrees() const { return glm::degrees(fov_radians_); }
    float aspect_ratio() const { return aspect_ratio_; }
    float near_plane() const { return near_plane_; }
    float far_plane() const { return far_plane_; }
    const glm::mat4& projection() const { return projection_; }

private:
    void update_projection();

    float fov_radians_;
    float aspect_ratio_;
    float near_plane_;
    float far_plane_;

    glm::mat4 projection_{1.0f};
};

} // namespace glass
