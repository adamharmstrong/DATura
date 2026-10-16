#include "../DATura/inventory.h"
#include "../DATura/starter_item_catalog.h"
#include "../DATura/ffxi_inventory_menu.h"
#include "../DATura/world_item_system.h"
#include "../DATura/world_item_hover.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Check(const bool condition, const char* message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}
}

int main()
{
    using namespace InventorySystem;

    ItemCatalog catalog;
    Check(!catalog.Register({0, "Invalid", 1}), "item zero must be rejected");
    Check(!catalog.Register({1, "", 1}), "nameless items must be rejected");
    Check(!catalog.Register({1, "Invalid", 0}), "zero stack limits must be rejected");
    Check(catalog.Register({1, "Potion", 12}), "valid item registration failed");
    Check(catalog.Register({2, "Bronze Sword", 1}), "second item registration failed");
    Check(!catalog.Register({1, "Duplicate Potion", 99}), "duplicate item registration must fail");

    Inventory inventory(3, catalog);
    const auto potionAdd = inventory.Add(1, 14);
    Check(potionAdd.status == AddStatus::Complete && potionAdd.added == 14 &&
        potionAdd.remainder == 0, "stacked add result is incorrect");
    Check(inventory.OccupiedSlotCount() == 2 && inventory.Slot(0)->quantity == 12 &&
        inventory.Slot(1)->quantity == 2, "items did not split at the stack limit");

    const auto swordAdd = inventory.Add(2, 2);
    Check(swordAdd.status == AddStatus::Partial && swordAdd.added == 1 &&
        swordAdd.remainder == 1, "full inventory must report a partial add");
    Check(inventory.Count(2) == 1, "non-stackable item count is incorrect");
    Check(inventory.Add(99, 1).status == AddStatus::UnknownItem,
        "unknown items must be rejected");

    Check(!inventory.Remove(1, 15) && inventory.Count(1) == 14,
        "failed removal must not mutate the inventory");
    Check(inventory.Remove(1, 13) && inventory.Count(1) == 1,
        "removal across stacks failed");

    bool receiverCalled = false;
    Check(!inventory.TransferFromSlot(1, 1, [&](const ItemStack& stack) {
        receiverCalled = true;
        return stack.itemId == 1 && stack.quantity == 1 && false;
    }), "rejected transfer unexpectedly succeeded");
    Check(receiverCalled && inventory.Count(1) == 1,
        "rejected transfer must leave inventory unchanged");

    ItemStack dropped;
    Check(inventory.TransferFromSlot(1, 1, [&](const ItemStack& stack) {
        dropped = stack;
        return true;
    }), "accepted transfer failed");
    Check(dropped.itemId == 1 && dropped.quantity == 1 && inventory.Count(1) == 0,
        "accepted transfer did not atomically extract the stack");
    Check(!inventory.TransferFromSlot(1, 1, [](const ItemStack&) { return true; }),
        "empty slot transfer must fail");

    const std::vector<std::optional<ItemStack>> savedSlots =
        {ItemStack{2, 1}, std::nullopt, ItemStack{1, 4}};
    Check(inventory.Restore(savedSlots), "valid saved slots did not restore");
    Check(inventory.Slots() == savedSlots && inventory.Count(1) == 4,
        "restored inventory does not match the saved slot snapshot");
    const auto beforeInvalidRestore = inventory.Slots();
    Check(!inventory.Restore({ItemStack{2, 2}, std::nullopt, std::nullopt}) &&
        inventory.Slots() == beforeInvalidRestore,
        "invalid saved stacks must not mutate the inventory");

    ItemCatalog starterCatalog;
    Check(StarterItemCatalog::RegisterAll(starterCatalog),
        "starter catalog registration failed");
    Check(starterCatalog.Size() == StarterItemCatalog::kItemCount,
        "starter catalog item count is incorrect");
    Check(!StarterItemCatalog::RegisterAll(starterCatalog),
        "duplicate starter catalog registration must fail");
    ItemCatalog conflictingCatalog;
    Check(conflictingCatalog.Register({4096, "Conflicting Crystal", 1}) &&
        !StarterItemCatalog::RegisterAll(conflictingCatalog) && conflictingCatalog.Size() == 1,
        "starter catalog registration must be atomic on a preexisting ID conflict");

    const auto* potion = starterCatalog.Find(4112);
    Check(potion && potion->name == "Potion" && potion->stackLimit == 1 &&
        potion->use.effect == UseEffect::RestoreHp && potion->use.magnitude == 50,
        "Potion metadata is incorrect");
    const auto* gingerCookie = starterCatalog.FindByName("Ginger Cookie");
    Check(gingerCookie && gingerCookie->id == 4394 && gingerCookie->stackLimit == 99,
        "food lookup or stack metadata is incorrect");
    const auto* onionSword = starterCatalog.Find(16534);
    Check(onionSword && onionSword->equipment.slot == EquipmentSlot::Main &&
        onionSword->equipment.weaponType == WeaponType::Sword &&
        onionSword->equipment.damage == 5 && onionSword->equipment.delay == 228,
        "Onion Sword equipment metadata is incorrect");
    const auto* leatherVest = starterCatalog.Find(12568);
    Check(leatherVest && leatherVest->equipment.level == 7 &&
        leatherVest->equipment.defense == 7,
        "Leather Vest equipment metadata is incorrect");

    StarterItemCatalog::KeyItemCatalog keyItems;
    Check(StarterItemCatalog::RegisterStarterKeyItems(keyItems) &&
        keyItems.Size() == StarterItemCatalog::kKeyItemCount,
        "starter key-item catalog registration failed");
    Check(keyItems.Find(138) && keyItems.Find(386) && !starterCatalog.Find(138),
        "key items must remain separate from physical inventory items");

    Inventory menuInventory(4, starterCatalog);
    menuInventory.Add(4112, 2);
    menuInventory.Add(16534, 1);
    std::vector<StarterItemCatalog::KeyItemId> ownedKeyItems{385, 386};
    FFXIInventoryMenu::State menu;
    FFXIInventoryMenu::OpenItems(menu);
    Check(FFXIInventoryMenu::IsOpen(menu), "items menu did not open");
    FFXIInventoryMenu::HandleKey(menu, VK_RETURN, menuInventory,
        starterCatalog, ownedKeyItems);
    auto menuEvent = FFXIInventoryMenu::HandleKey(menu, VK_RETURN, menuInventory,
        starterCatalog, ownedKeyItems);
    Check(menuEvent.type == FFXIInventoryMenu::EventType::Use &&
        menuEvent.stack.itemId == 4112, "Potion menu did not expose Use");

    Inventory dropInventory(2, starterCatalog);
    dropInventory.Add(4394, 2);
    FFXIInventoryMenu::OpenItems(menu);
    FFXIInventoryMenu::HandleKey(menu, VK_RETURN, dropInventory,
        starterCatalog, ownedKeyItems);
    FFXIInventoryMenu::HandleKey(menu, VK_DOWN, dropInventory,
        starterCatalog, ownedKeyItems);
    FFXIInventoryMenu::HandleKey(menu, VK_RETURN, dropInventory,
        starterCatalog, ownedKeyItems);
    Check(menu.view == FFXIInventoryMenu::View::DropQuantity,
        "stacked Drop did not open quantity selection");
    FFXIInventoryMenu::HandleKey(menu, VK_RIGHT, dropInventory,
        starterCatalog, ownedKeyItems);
    menuEvent = FFXIInventoryMenu::HandleKey(menu, VK_RETURN, dropInventory,
        starterCatalog, ownedKeyItems);
    Check(menuEvent.type == FFXIInventoryMenu::EventType::Drop &&
        menuEvent.stack.quantity == 2, "drop quantity selection is incorrect");

    FFXIInventoryMenu::OpenEquipment(menu);
    FFXIInventoryMenu::HandleKey(menu, VK_RETURN, menuInventory,
        starterCatalog, ownedKeyItems);
    menuEvent = FFXIInventoryMenu::HandleKey(menu, VK_RETURN, menuInventory,
        starterCatalog, ownedKeyItems);
    Check(menuEvent.type == FFXIInventoryMenu::EventType::Equip &&
        menuEvent.stack.itemId == 16534, "equipment menu did not expose Equip");

    FFXIInventoryMenu::OpenKeyItems(menu);
    FFXIInventoryMenu::HandleKey(menu, VK_DOWN, menuInventory,
        starterCatalog, ownedKeyItems);
    Check(menu.selected == 1, "key-item menu navigation failed");
    menuEvent = FFXIInventoryMenu::HandleKey(menu, VK_ESCAPE, menuInventory,
        starterCatalog, ownedKeyItems);
    Check(menuEvent.type == FFXIInventoryMenu::EventType::Closed &&
        !FFXIInventoryMenu::IsOpen(menu), "key-item menu did not close");

    WorldItemSystem::State worldItems;
    const WorldItemSystem::SpawnRequest droppedPotion{235, {4112, 2}, {1.0f, 2.0f, 3.0f}};
    const auto worldId = WorldItemSystem::Spawn(worldItems, droppedPotion);
    Check(worldId != 0 && worldItems.entities.size() == 1,
        "valid world item did not spawn");
    const auto* worldItem = WorldItemSystem::Find(worldItems, worldId);
    Check(worldItem && worldItem->zoneId == 235 && worldItem->stack.itemId == 4112 &&
        worldItem->stack.quantity == 2 && worldItem->position[2] == 3.0f,
        "spawned world item state is incorrect");
    Check(WorldItemSystem::CountInZone(worldItems, 235) == 1 &&
        WorldItemSystem::CountInZone(worldItems, 236) == 0,
        "world item zone filtering is incorrect");
    WorldItemSystem::SpawnRequest invalidDrop = droppedPotion;
    invalidDrop.stack.quantity = 0;
    Check(WorldItemSystem::Spawn(worldItems, invalidDrop) == 0 &&
        worldItems.entities.size() == 1, "invalid world item mutated state");

    Inventory transactionalDrop(1, starterCatalog);
    transactionalDrop.Add(4394, 3);
    Check(transactionalDrop.TransferFromSlot(0, 2,
        [&](const ItemStack& stack)
        {
            WorldItemSystem::SpawnRequest request{235, stack, {4.0f, 5.0f, 6.0f}};
            return WorldItemSystem::Spawn(worldItems, request) != 0;
        }) && transactionalDrop.Count(4394) == 1 &&
        WorldItemSystem::CountInZone(worldItems, 235) == 2,
        "successful world spawn did not atomically remove the dropped stack");
    Check(!transactionalDrop.TransferFromSlot(0, 1,
        [&](const ItemStack& stack)
        {
            WorldItemSystem::SpawnRequest request{-1, stack, {4.0f, 5.0f, 6.0f}};
            return WorldItemSystem::Spawn(worldItems, request) != 0;
        }) && transactionalDrop.Count(4394) == 1,
        "rejected world spawn removed an inventory item");

    const std::vector<WorldItemHover::Candidate> hoverCandidates = {
        {1, 10.0f, 10.0f, 30.0f, 30.0f, 0.8f},
        {2, 15.0f, 15.0f, 35.0f, 35.0f, 0.3f}
    };
    Check(WorldItemHover::PickNearest(hoverCandidates, 20.0f, 20.0f) == 2,
        "hover selection did not prefer the nearest overlapping world item");
    Check(WorldItemHover::PickNearest(hoverCandidates, 12.0f, 12.0f) == 1,
        "hover selection missed a single world item");
    Check(WorldItemHover::PickNearest(hoverCandidates, 50.0f, 50.0f) == 0,
        "hover selection matched outside all world items");

    WorldItemSystem::State pickupWorld;
    const auto pickupId = WorldItemSystem::Spawn(pickupWorld,
        {235, {4394, 3}, {1.0f, 2.0f, 3.0f}});
    Inventory pickupBag(1, starterCatalog);
    Check(pickupBag.CanAdd(4394, 3) &&
        WorldItemSystem::TransferToInventory(pickupWorld, pickupId, pickupBag) &&
        pickupBag.Count(4394) == 3 && pickupWorld.entities.empty(),
        "click-style pickup did not transfer the complete world stack");

    const auto rejectedPickupId = WorldItemSystem::Spawn(pickupWorld,
        {235, {4394, 2}, {1.0f, 2.0f, 3.0f}});
    pickupBag.Add(4394, 96);
    Check(!pickupBag.CanAdd(4394, 2) &&
        !WorldItemSystem::TransferToInventory(
            pickupWorld, rejectedPickupId, pickupBag) &&
        pickupBag.Count(4394) == 99 && pickupWorld.entities.size() == 1,
        "failed pickup did not preserve the complete world stack");

    std::cout << "All inventory checks passed.\n";
}
