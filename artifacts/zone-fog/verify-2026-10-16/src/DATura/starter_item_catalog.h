#pragma once

#include "inventory.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>

namespace StarterItemCatalog
{
// Registers the deliberately bounded collection used by DATura's first playable
// low-level slice. Returns false if the target catalog already contains an ID.
bool RegisterAll(InventorySystem::ItemCatalog& catalog);
inline constexpr std::size_t kItemCount = 59;

using KeyItemId = std::uint16_t;

struct KeyItemDefinition
{
    KeyItemId id = 0;
    std::string name;
};

class KeyItemCatalog
{
public:
    bool Register(KeyItemDefinition definition);
    const KeyItemDefinition* Find(KeyItemId id) const;
    std::size_t Size() const { return definitions_.size(); }

private:
    std::unordered_map<KeyItemId, KeyItemDefinition> definitions_;
};

bool RegisterStarterKeyItems(KeyItemCatalog& catalog);
inline constexpr std::size_t kKeyItemCount = 4;
}
