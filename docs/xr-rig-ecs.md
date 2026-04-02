# XR Rig as ECS Entities

## Current approach

The XR view matrices are computed directly in `XrSystem::wait_and_begin_frame()`. The camera entity provides body position and yaw; the XR head pose is layered on top via matrix composition. There are no ECS entities for the head or eyes.

```
camera_entity (Transform + Velocity + CameraComponent)
    position → body position (WASD)
    yaw      → body yaw (mouse horizontal)
    ↓
XrSystem::xr_pose_to_view_matrix()
    body_pos + body_yaw + xr_to_engine * eye_pose → view matrix
```

## Future: XR rig entity hierarchy

Replace the ad-hoc matrix composition with an entity hierarchy:

```
body_entity      Transform + Velocity          CameraController (WASD + mouse yaw)
  └─ head_entity   Transform                   XrSystem writes head pose each frame
       ├─ eye_L     Transform + CameraComponent XrSystem writes per-eye offset + FOV
       └─ eye_R     Transform + CameraComponent
       (future)
       ├─ hand_L    Transform                   XrSystem writes controller pose
       └─ hand_R    Transform
```

Each entity's world transform is the product of its ancestors' transforms. The renderer just renders from any entity with a CameraComponent.

## Prerequisite: transform hierarchy

The engine currently has no parent-child relationship between entities. Each entity's `Transform` is a standalone world matrix. A transform hierarchy would provide:

- `Parent` component linking a child to its parent entity
- System that walks the hierarchy and computes world transforms from local transforms
- Dirty-flag propagation so unchanged subtrees skip recomputation

Without this, the XR rig would still manually compose `body * head * eye` each frame, which is what we do now.

## Benefits

- **Unified rendering** -- the renderer doesn't need separate XR/desktop code paths; it renders from any CameraComponent entity
- **Controller support** -- hand entities slot in naturally as children of the head or body
- **Attached objects** -- headlamps, HUD elements, held items attach to head/hand entities
- **Scene graph reuse** -- vehicles, turrets, or any mounted camera become the same pattern

## Considerations

- Transform hierarchy adds per-frame cost (walk + multiply). For a small hierarchy (5-10 nodes for an XR rig) this is negligible.
- The renderer still needs to know which cameras to render and to which targets (XR swapchains vs desktop). A "RenderTarget" component or a camera priority/tag system could handle this.
- Multi-camera rendering (N cameras to N targets in one frame) requires the UBO-per-view pattern already implemented for XR stereo.
