#pragma once

struct noesisModel_t;
class noeRAPI_t;

namespace ZoneRoomLoader
{
// Appends authored room geometry using the zone's allocator/lifetime.
// Missing or invalid optional room files leave the main zone usable.
int Append(noesisModel_t* zone, noeRAPI_t* rapi, const char* zonePath);
}
