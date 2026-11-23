#pragma once
#include "zone_collision_geometry.h"
#include "zone_object_visibility.h"

namespace ZoneDoorInteraction
{
struct Door
{
    std::string name;
    float hinge[3] = {}, center[3] = {}, width = 0, minY = 0, maxY = 0;
    size_t collisionStart = 0, collisionCount = 0;
    int partner = -1;
    float angle = 0, targetAngle = 0;
    float openSeconds = 0;
    float angularVelocity = 0;
};
struct CollisionBinding
{
    int door, triangle;
    ZoneCollision::Triangle closed;
};
struct Hit
{
    int door = -1;
    float depth = 1;
};
class State
{
  public:
    std::vector<Door> doors;
    std::vector<CollisionBinding> collision;
    std::map<std::string, ZoneObjectTransform::DebugTransform> transforms;
    int selected = -1;
    bool physics = false;

    // Called after the zone's optional X mirror, before collision is indexed.
    void Initialize(noesisModel_t *model, bool mirrorX);
    int FindDoor(const std::string &name) const;
    // Registers authored visual collision and tightly matching native panels.
    // Returns extra spatial-index padding needed for a swinging panel.
    float BindCollision(size_t sourceIndex, int runtimeIndex, const ZoneCollision::Triangle &triangle);
    bool Click(int door, const float player[3]);
    void Update(float dt, ZoneCollision::Mesh &mesh);
    void SetPhysics(bool enabled);
    void UpdatePhysics(float dt, ZoneCollision::Mesh &mesh, const float *player = nullptr, float deltaX = 0,
                       float deltaZ = 0, float radius = 0.72f, float height = 3.2f);
    void ApplyCollision(ZoneCollision::Mesh &mesh, const std::vector<bool> &changed);
    Hit Pick(const noesisModel_t *model, const ZoneObjectVisibility::RenderContext &visibility,
             const D3DMATRIX &view, const D3DMATRIX &projection, int width, int height, float x,
             float y) const;
};
} // namespace ZoneDoorInteraction
