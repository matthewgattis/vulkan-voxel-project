#include <glass/camera.hpp>

#include <glm/gtc/matrix_transform.hpp>

namespace glass {

Camera::Camera(float fov_degrees, float aspect_ratio, float near_plane, float far_plane)
    : fov_radians_(glm::radians(fov_degrees))
    , aspect_ratio_(aspect_ratio)
    , near_plane_(near_plane)
    , far_plane_(far_plane) {
    update_view();
    update_projection();
}

void Camera::set_position(const glm::vec3& position) {
    position_ = position;
    update_view();
}

void Camera::look_at(const glm::vec3& target) {
    target_ = target;
    update_view();
}

void Camera::set_aspect_ratio(float aspect_ratio) {
    aspect_ratio_ = aspect_ratio;
    update_projection();
}

void Camera::update_view() {
    view_ = glm::lookAt(position_, target_, up_);
}

void Camera::update_projection() {
    projection_ = glm::perspective(fov_radians_, aspect_ratio_, near_plane_, far_plane_);
    projection_[1][1] *= -1.0f; // Vulkan Y-axis is inverted vs OpenGL
}

} // namespace glass
