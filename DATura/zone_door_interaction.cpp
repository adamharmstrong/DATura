#include "stdafx.h"
#include "zone_door_interaction.h"
#include "d3d_math.h"
#include "model_ff11.h"
#include <limits>

namespace ZoneDoorInteraction
{
namespace
{
void Transform(const float *p, const D3DMATRIX &m, float *out)
{
    for (int i = 0; i < 3; ++i)
        out[i] = p[0] * m.m[0][i] + p[1] * m.m[1][i] + p[2] * m.m[2][i] + m.m[3][i];
}
ZoneObjectTransform::DebugTransform Swing(const Door &door)
{
    ZoneObjectTransform::DebugTransform t = {};
    t.scale[0] = t.scale[1] = t.scale[2] = 1;
    t.rot[1] = door.angle;
    const auto rotation = D3DMath::BuildRotationY(door.angle);
    float rotated[3];
    Transform(door.hinge, rotation, rotated);
    for (int i = 0; i < 3; ++i)
        t.trans[i] = door.hinge[i] - rotated[i];
    return t;
}
float DistanceXZ(const float *a, const float *b)
{
    return std::hypot(a[0] - b[0], a[2] - b[2]);
}
bool RayTriangle(const float *origin, const float *direction, const float p[3][3], float &distance)
{
    float e1[3], e2[3], h[3], s[3], q[3];
    for (int i = 0; i < 3; ++i)
    {
        e1[i] = p[1][i] - p[0][i];
        e2[i] = p[2][i] - p[0][i];
        s[i] = origin[i] - p[0][i];
    }
    auto cross = [](const float *a, const float *b, float *c) {
        c[0] = a[1] * b[2] - a[2] * b[1];
        c[1] = a[2] * b[0] - a[0] * b[2];
        c[2] = a[0] * b[1] - a[1] * b[0];
    };
    auto dot = [](const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
    cross(direction, e2, h);
    float det = dot(e1, h);
    if (std::fabs(det) < 1e-7f)
        return false;
    float u = dot(s, h) / det;
    if (u < 0 || u > 1)
        return false;
    cross(s, e1, q);
    float v = dot(direction, q) / det;
    if (v < 0 || u + v > 1)
        return false;
    distance = dot(e2, q) / det;
    return distance > 0;
}
bool RayBounds(const float *origin, const float *direction, const float *lo, const float *hi, float limit)
{
    float nearDistance = 0, farDistance = limit;
    for (int i = 0; i < 3; ++i)
    {
        if (std::fabs(direction[i]) < 1e-8f)
        {
            if (origin[i] < lo[i] || origin[i] > hi[i])
                return false;
            continue;
        }
        float a = (lo[i] - origin[i]) / direction[i], b = (hi[i] - origin[i]) / direction[i];
        if (a > b)
            std::swap(a, b);
        nearDistance = (std::max)(nearDistance, a);
        farDistance = (std::min)(farDistance, b);
        if (nearDistance > farDistance)
            return false;
    }
    return true;
}
} // namespace

void State::Initialize(noesisModel_t *model, bool mirrorX)
{
    *this = {};
    if (!model)
        return;
    model->UpdateSubmeshBounds();
    for (const auto &object : gFF11LastMapObjects)
    {
        if (object.replacedByRoom || !std::string(object.objectName).starts_with("door_"))
            continue;
        Door door;
        door.name = object.displayName;
        float lo[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, hi[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        bool found = false;
        for (const auto &mesh : model->submeshes)
            if (mesh.objectName == door.name && mesh.hasBounds)
            {
                found = true;
                for (int i = 0; i < 3; ++i)
                {
                    lo[i] = (std::min)(lo[i], mesh.boundsMin[i]);
                    hi[i] = (std::max)(hi[i], mesh.boundsMax[i]);
                }
            }
        if (!found)
            continue;
        for (int i = 0; i < 3; ++i)
        {
            door.hinge[i] = object.trans[i];
            door.center[i] = (lo[i] + hi[i]) * 0.5f;
        }
        if (mirrorX)
            door.hinge[0] = -door.hinge[0];
        door.width = 2 * DistanceXZ(door.center, door.hinge);
        if (door.width < 0.2f || door.width > 12)
            continue;
        door.minY = lo[1];
        door.maxY = hi[1];
        door.collisionStart = object.visualCollisionStart;
        door.collisionCount = object.visualCollisionCount;
        doors.push_back(door);
    }
    // Retail double doors are separate placements with pivots at each hinge.
    // Pair only leaves whose free edges meet (or gates sharing a pivot).
    for (size_t i = 0; i < doors.size(); ++i)
        if (doors[i].partner < 0)
            for (size_t j = i + 1; j < doors.size(); ++j)
                if (doors[j].partner < 0)
                {
                    auto &a = doors[i];
                    auto &b = doors[j];
                    if (std::fabs(a.center[1] - b.center[1]) > 0.2f || std::fabs(a.width - b.width) > 0.2f)
                        continue;
                    float endA[3] = {2 * a.center[0] - a.hinge[0], 0, 2 * a.center[2] - a.hinge[2]};
                    float endB[3] = {2 * b.center[0] - b.hinge[0], 0, 2 * b.center[2] - b.hinge[2]};
                    if (DistanceXZ(endA, endB) < 0.2f || DistanceXZ(a.hinge, b.hinge) < 0.1f)
                    {
                        // Some gate leaves share an origin at the meeting edge;
                        // their physical hinges are at the opposite outer edges.
                        if (DistanceXZ(a.hinge, b.hinge) < 0.1f)
                        {
                            a.hinge[0] = endA[0];
                            a.hinge[2] = endA[2];
                            b.hinge[0] = endB[0];
                            b.hinge[2] = endB[2];
                        }
                        a.partner = (int)j;
                        b.partner = (int)i;
                        break;
                    }
                }
}
int State::FindDoor(const std::string &name) const
{
    for (size_t i = 0; i < doors.size(); ++i)
        if (doors[i].name == name)
            return (int)i;
    return -1;
}
float State::BindCollision(size_t sourceIndex, int runtimeIndex, const ZoneCollision::Triangle &triangle)
{
    for (size_t i = 0; i < doors.size(); ++i)
    {
        const auto &door = doors[i];
        bool matches =
            sourceIndex >= door.collisionStart && sourceIndex - door.collisionStart < door.collisionCount;
        if (!matches && std::fabs(triangle.normal[1]) < 0.25f)
        {
            // Native collision sometimes duplicates the visual door panel.
            // Require every vertex to lie inside this leaf, never cut a hole
            // out of a surrounding wall or floor triangle.
            float ux = (door.center[0] - door.hinge[0]) * 2 / door.width;
            float uz = (door.center[2] - door.hinge[2]) * 2 / door.width;
            matches = true;
            for (const auto &p : triangle.p)
            {
                float x = p[0] - door.hinge[0], z = p[2] - door.hinge[2];
                float along = x * ux + z * uz, across = x * uz - z * ux;
                if (along < -0.08f || along > door.width + 0.08f || std::fabs(across) > 0.12f ||
                    p[1] < door.minY - 0.05f || p[1] > door.maxY + 0.05f)
                    matches = false;
            }
        }
        if (matches)
        {
            collision.push_back({(int)i, runtimeIndex, triangle});
            return door.width * 2;
        }
    }
    return 0;
}
bool State::Click(int index, const float player[3])
{
    if (index < 0 || index >= (int)doors.size())
    {
        selected = -1;
        return false;
    }
    if (physics)
    {
        selected = index;
        return false;
    }
    if (selected != index && !(selected >= 0 && doors[selected].partner == index))
    {
        selected = index;
        return false;
    }
    selected = index;
    auto &door = doors[index];
    if (!player || DistanceXZ(door.center, player) > 6 || std::fabs(door.center[1] - player[1]) > 5)
        return false;
    for (int leaf : {index, door.partner})
        if (leaf >= 0)
        {
            auto &d = doors[leaf];
            d.openSeconds = 0;
            if (d.targetAngle != 0)
                continue;
            float dx = d.center[0] - d.hinge[0], dz = d.center[2] - d.hinge[2];
            float away = dz * (d.center[0] - player[0]) - dx * (d.center[2] - player[2]);
            // Reverse an interrupted close along the existing swing.
            d.targetAngle = (d.angle != 0 ? d.angle > 0 : away >= 0) ? 1.57079633f : -1.57079633f;
        }
    return true;
}
void State::Update(float dt, ZoneCollision::Mesh &mesh)
{
    if (physics || !std::isfinite(dt) || dt <= 0)
        return;
    std::vector<bool> changed(doors.size());
    for (size_t i = 0; i < doors.size(); ++i)
    {
        auto &door = doors[i];
        if (door.angle == door.targetAngle && door.targetAngle != 0)
        {
            door.openSeconds += dt;
            if (door.openSeconds >= 15.0f)
                door.targetAngle = 0;
        }
        if (door.angle == door.targetAngle)
            continue;
        float step = (std::min)(dt, 0.1f) * 3.14159265f;
        door.angle += std::clamp(door.targetAngle - door.angle, -step, step);
        transforms[door.name] = Swing(door);
        changed[i] = true;
    }
    ApplyCollision(mesh, changed);
}
void State::ApplyCollision(ZoneCollision::Mesh &mesh, const std::vector<bool> &changed)
{
    for (const auto &binding : collision)
        if (changed[binding.door] && binding.triangle < (int)mesh.Triangles().size())
        {
            if (doors[binding.door].angle == 0)
            {
                mesh.Triangles()[binding.triangle] = binding.closed;
                continue;
            }
            auto matrix = ZoneObjectTransform::BuildWorldMatrix(transforms.at(doors[binding.door].name));
            float points[3][3];
            for (int i = 0; i < 3; ++i)
                Transform(binding.closed.p[i], matrix, points[i]);
            ZoneCollision::Triangle triangle;
            if (ZoneCollision::BuildTriangle(&points[0][0], false, triangle))
                mesh.Triangles()[binding.triangle] = triangle;
        }
    for (size_t i = 0; i < doors.size(); ++i)
        if (changed[i] && doors[i].angle == 0)
            transforms.erase(doors[i].name);
}
void State::SetPhysics(bool enabled)
{
    if (physics == enabled)
        return;
    physics = enabled;
    for (auto &door : doors)
    {
        door.angularVelocity = 0;
        door.openSeconds = 0;
        door.targetAngle = door.angle == 0 ? 0 : std::copysign(1.57079633f, door.angle);
    }
}

void State::UpdatePhysics(float dt, ZoneCollision::Mesh &mesh, const float *player, float deltaX,
                          float deltaZ, float radius, float height)
{
    if (!physics || !std::isfinite(dt) || dt <= 0)
        return;
    // Bounded substeps keep contact and hinge integration stable at low FPS.
    dt = (std::min)(dt, 0.1f);
    const int steps = (std::max)(1, (int)std::ceil(dt * 120));
    const float h = dt / steps;
    constexpr float limit = 1.74532925f; // +/-100 degrees, with hinge stops.
    std::vector<bool> changed(doors.size());
    for (size_t i = 0; i < doors.size(); ++i)
    {
        auto &door = doors[i];
        const float initialAngle = door.angle;
        const float ux = (door.center[0] - door.hinge[0]) * 2 / door.width;
        const float uz = (door.center[2] - door.hinge[2]) * 2 / door.width;
        for (int step = 0; step < steps; ++step)
        {
            // Unit-mass panel: contact torque scales with lever arm and inertia.
            float acceleration = -0.9f * door.angle - 1.8f * door.angularVelocity;
            if (player && player[1] >= door.minY && player[1] - height <= door.maxY)
            {
                const float c = std::cos(door.angle), s = std::sin(door.angle);
                const float tx = ux * c + uz * s, tz = -ux * s + uz * c;
                const float fraction = (float)(step + 1) / steps;
                const float px = player[0] + deltaX * fraction - door.hinge[0];
                const float pz = player[2] + deltaZ * fraction - door.hinge[2];
                const float along = std::clamp(px * tx + pz * tz, 0.0f, door.width);
                const float ex = px - tx * along, ez = pz - tz * along;
                const float distance = std::hypot(ex, ez);
                if (distance < radius + 0.08f && along > 0.05f)
                {
                    // Keep the original side for swept contact across a thin panel.
                    const float originalSide =
                        (player[0] - door.hinge[0]) * tz - (player[2] - door.hinge[2]) * tx;
                    const float side =
                        std::fabs(originalSide) > 0.001f ? originalSide : -(deltaX * tz - deltaZ * tx);
                    const float force = -std::copysign(90.0f * (radius + 0.08f - distance), side);
                    acceleration += force * along / (door.width * door.width / 3);
                }
            }
            door.angularVelocity = std::clamp(door.angularVelocity + acceleration * h, -4.0f, 4.0f);
            door.angle += door.angularVelocity * h;
            if (std::fabs(door.angle) > limit)
            {
                door.angle = std::clamp(door.angle, -limit, limit);
                door.angularVelocity = 0;
            }
            if (std::fabs(door.angle) < 0.0001f && std::fabs(door.angularVelocity) < 0.0001f)
                door.angle = door.angularVelocity = 0;
        }
        if (door.angle != initialAngle)
        {
            transforms[door.name] = Swing(door);
            changed[i] = true;
        }
    }
    ApplyCollision(mesh, changed);
}

Hit State::Pick(const noesisModel_t *model, const ZoneObjectVisibility::RenderContext &visibility,
                const D3DMATRIX &view, const D3DMATRIX &projection, int width, int height, float x,
                float y) const
{
    Hit hit;
    if (!model || width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height)
        return hit;
    float camera[3] = {-(view._41 * view._11 + view._42 * view._12 + view._43 * view._13),
                       -(view._41 * view._21 + view._42 * view._22 + view._43 * view._23),
                       -(view._41 * view._31 + view._42 * view._32 + view._43 * view._33)};
    float vx = (2 * x / width - 1) / projection._11, vy = (1 - 2 * y / height) / projection._22;
    float direction[3] = {vx * view._11 + vy * view._12 + view._13, vx * view._21 + vy * view._22 + view._23,
                          vx * view._31 + vy * view._32 + view._33};
    float nearest = FLT_MAX;
    for (const auto &submesh : model->submeshes)
    {
        if (submesh.environmentObject ||
            !ZoneObjectVisibility::PassesRenderVisibility(model, submesh, true, visibility))
            continue;
        int door = FindDoor(submesh.objectName);
        if (submesh.softBlend && door < 0)
            continue;
        auto matrix = D3DMath::BuildIdentity();
        if (visibility.overrides)
        {
            auto it = visibility.overrides->find(submesh.objectName);
            if (it != visibility.overrides->end())
                matrix = ZoneObjectTransform::BuildWorldMatrix(it->second);
        }
        float lo[3] = {FLT_MAX, FLT_MAX, FLT_MAX}, hi[3] = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
        for (int corner = 0; corner < 8; ++corner)
        {
            float p[3], world[3];
            for (int i = 0; i < 3; ++i)
                p[i] = (corner & (1 << i)) ? submesh.boundsMax[i] : submesh.boundsMin[i];
            Transform(p, matrix, world);
            for (int i = 0; i < 3; ++i)
            {
                lo[i] = (std::min)(lo[i], world[i]);
                hi[i] = (std::max)(hi[i], world[i]);
            }
        }
        if (submesh.hasBounds && !RayBounds(camera, direction, lo, hi, nearest))
            continue;
        for (size_t i = 0; i + 2 < submesh.cpuIndices.size(); i += 3)
        {
            float p[3][3];
            for (int j = 0; j < 3; ++j)
                Transform(submesh.cpuVerts[submesh.cpuIndices[i + j]].pos, matrix, p[j]);
            float distance;
            if (RayTriangle(camera, direction, p, distance) && distance < nearest)
            {
                // Ray is parameterized in view-space Z, matching D3D depth.
                float depth = projection._33 + projection._43 / distance;
                if (depth < 0 || depth > 1)
                    continue;
                nearest = distance;
                hit = {door, depth};
            }
        }
    }
    return hit;
}
} // namespace ZoneDoorInteraction
