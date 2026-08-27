#pragma once

#include "noesis_rapi.h"
#include "zone_collision_geometry.h"

namespace FFXICreationGrounding
{
    // Finds a walkable point whose ground surface intersects the model's
    // existing foot altitude. Only X/Z translation is returned; Y is untouched.
    bool FindHorizontalPlacement(const noesisModel_t* model,
                                 const ZoneCollision::Mesh& collisionMesh,
                                 float outTranslation[3]);
}
