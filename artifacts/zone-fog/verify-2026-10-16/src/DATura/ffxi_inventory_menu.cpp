#include "stdafx.h"
#include "ffxi_inventory_menu.h"
#ifndef DATURA_MENU_LOGIC_ONLY
#include "win32_drawing.h"
#endif

#include <algorithm>
#include <string>

namespace
{
using namespace FFXIInventoryMenu;
using namespace InventorySystem;

std::vector<std::size_t> VisibleSlots(const View view, const Inventory& inventory,
    const ItemCatalog& catalog)
{
    std::vector<std::size_t> result;
    for (std::size_t i = 0; i < inventory.SlotCount(); ++i)
    {
        const auto& slot = inventory.Slot(i);
        if (!slot) continue;
        const auto* item = catalog.Find(slot->itemId);
        if (!item) continue;
        if (view == View::Equipment && (item->flags & ItemFlagEquippable) == 0)
            continue;
        result.push_back(i);
    }
    return result;
}

std::vector<EventType> Actions(const ItemDefinition& item)
{
    std::vector<EventType> result;
    if ((item.flags & ItemFlagUsable) != 0 && item.use.effect != UseEffect::BeginSynthesis)
        result.push_back(EventType::Use);
    if ((item.flags & ItemFlagEquippable) != 0)
        result.push_back(EventType::Equip);
    result.push_back(EventType::Drop);
    return result;
}

const char* ActionName(const EventType action)
{
    switch (action)
    {
    case EventType::Use: return "Use";
    case EventType::Equip: return "Equip";
    case EventType::Drop: return "Drop";
    default: return "Cancel";
    }
}

#ifndef DATURA_MENU_LOGIC_ONLY
void PaintPanel(HDC dc, const RECT& panel)
{
    HBRUSH blue = CreateSolidBrush(RGB(17, 27, 70));
    HBRUSH dark = CreateSolidBrush(RGB(12, 18, 50));
    for (int y = panel.top; y < panel.bottom; y += 2)
    {
        RECT stripe{panel.left, y, panel.right, (std::min)(y + 2, (int)panel.bottom)};
        FillRect(dc, &stripe, ((y - panel.top) / 2) % 2 ? dark : blue);
    }
    DeleteObject(blue); DeleteObject(dark);
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(190, 197, 220));
    HGDIOBJ oldPen = SelectObject(dc, pen);
    HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(NULL_BRUSH));
    Rectangle(dc, panel.left, panel.top, panel.right, panel.bottom);
    SelectObject(dc, oldBrush); SelectObject(dc, oldPen); DeleteObject(pen);
}
#endif

std::string DetailText(const ItemDefinition& item)
{
    std::string text = item.name + "\n";
    if (item.category == ItemCategory::Weapon)
        text += "Lv." + std::to_string(item.equipment.level) + "  DMG:" +
            std::to_string(item.equipment.damage) + "  Delay:" +
            std::to_string(item.equipment.delay);
    else if (item.category == ItemCategory::Armor)
        text += "Lv." + std::to_string(item.equipment.level) + "  DEF:" +
            std::to_string(item.equipment.defense);
    else
        text += "Stack: " + std::to_string(item.stackLimit);
    return text;
}
}

namespace FFXIInventoryMenu
{
bool IsOpen(const State& state) { return state.view != View::Closed; }
void OpenItems(State& state) { state = {}; state.view = View::Items; }
void OpenEquipment(State& state) { state = {}; state.view = View::Equipment; }
void OpenKeyItems(State& state) { state = {}; state.view = View::KeyItems; }

Event HandleKey(State& state, const unsigned int key, const Inventory& inventory,
    const ItemCatalog& catalog, const std::vector<StarterItemCatalog::KeyItemId>& ownedKeyItems)
{
    if (!IsOpen(state)) return {};
    if (key == VK_ESCAPE)
    {
        if (state.view == View::Actions || state.view == View::DropQuantity)
        {
            state.view = state.view == View::DropQuantity ? View::Actions : state.returnView;
            state.actionSelected = 0;
            return {};
        }
        state.view = View::Closed;
        return {EventType::Closed};
    }

    if (state.view == View::KeyItems)
    {
        const int count = static_cast<int>(ownedKeyItems.size());
        if (count == 0) return {};
        if (key == VK_UP) state.selected = (state.selected + count - 1) % count;
        if (key == VK_DOWN) state.selected = (state.selected + 1) % count;
        return {};
    }

    const View listView = state.view == View::Items || state.view == View::Equipment
        ? state.view : state.returnView;
    const auto slots = VisibleSlots(listView, inventory, catalog);
    if (slots.empty()) return {};
    state.selected = std::clamp(state.selected, 0, static_cast<int>(slots.size()) - 1);
    const std::size_t slotIndex = slots[state.selected];
    const ItemStack stack = *inventory.Slot(slotIndex);
    const ItemDefinition* item = catalog.Find(stack.itemId);
    if (!item) return {};

    if (state.view == View::Items || state.view == View::Equipment)
    {
        const int count = static_cast<int>(slots.size());
        if (key == VK_UP) state.selected = (state.selected + count - 1) % count;
        else if (key == VK_DOWN) state.selected = (state.selected + 1) % count;
        else if (key == VK_RETURN)
        {
            state.returnView = state.view;
            state.view = View::Actions;
            state.actionSelected = 0;
        }
        return {};
    }

    const auto actions = Actions(*item);
    if (state.view == View::Actions)
    {
        const int count = static_cast<int>(actions.size()) + 1;
        if (key == VK_UP) state.actionSelected = (state.actionSelected + count - 1) % count;
        else if (key == VK_DOWN) state.actionSelected = (state.actionSelected + 1) % count;
        else if (key == VK_RETURN)
        {
            if (state.actionSelected == static_cast<int>(actions.size()))
            {
                state.view = state.returnView;
                return {};
            }
            const EventType action = actions[state.actionSelected];
            if (action == EventType::Drop && stack.quantity > 1)
            {
                state.view = View::DropQuantity;
                state.dropQuantity = 1;
                return {};
            }
            return {action, slotIndex, {stack.itemId, 1}};
        }
        return {};
    }

    if (state.view == View::DropQuantity)
    {
        if (key == VK_LEFT || key == VK_DOWN)
            state.dropQuantity = state.dropQuantity == 1 ? stack.quantity : state.dropQuantity - 1;
        else if (key == VK_RIGHT || key == VK_UP)
            state.dropQuantity = state.dropQuantity == stack.quantity ? 1 : state.dropQuantity + 1;
        else if (key == VK_RETURN)
            return {EventType::Drop, slotIndex, {stack.itemId, state.dropQuantity}};
    }
    return {};
}

void Draw(HDC dc, const State& state, const int width, const int height,
    const Inventory& inventory, const ItemCatalog& catalog,
    const StarterItemCatalog::KeyItemCatalog& keyItemCatalog,
    const std::vector<StarterItemCatalog::KeyItemId>& ownedKeyItems)
{
#ifdef DATURA_MENU_LOGIC_ONLY
    (void)dc; (void)state; (void)width; (void)height; (void)inventory;
    (void)catalog; (void)keyItemCatalog; (void)ownedKeyItems;
#else
    if (!dc || !IsOpen(state)) return;
    const int rowHeight = (std::max)(25, height / 29);
    RECT panel{40, 45, (std::min)(width - 40, 570), (std::min)(height - 45, 570)};
    PaintPanel(dc, panel);
    HFONT font = CreateFontA(-18, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
        DEFAULT_PITCH, "Arial");
    HGDIOBJ oldFont = SelectObject(dc, font);
    const View listView = state.view == View::Items || state.view == View::Equipment ||
        state.view == View::KeyItems ? state.view : state.returnView;
    const char* title = listView == View::Items ? "Items" :
        listView == View::Equipment ? "Equipment" : "Key Items";
    RECT titleRect{panel.left + 15, panel.top + 8, panel.right - 15, panel.top + 36};
    Win32Drawing::DrawShadowText(dc, font, title, titleRect,
        DT_LEFT | DT_VCENTER | DT_SINGLELINE, RGB(255, 226, 102), 2);

    if (listView == View::KeyItems)
    {
        if (ownedKeyItems.empty())
        {
            RECT row{panel.left + 20, panel.top + 45, panel.right - 20, panel.top + 45 + rowHeight};
            Win32Drawing::DrawShadowText(dc, font, "No key items.", row,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE, RGB(190, 210, 240), 2);
        }
        for (std::size_t i = 0; i < ownedKeyItems.size(); ++i)
        {
            const auto* item = keyItemCatalog.Find(ownedKeyItems[i]);
            if (!item) continue;
            RECT row{panel.left + 20, panel.top + 45 + static_cast<int>(i) * rowHeight,
                panel.right - 20, panel.top + 45 + static_cast<int>(i + 1) * rowHeight};
            Win32Drawing::DrawShadowText(dc, font, item->name.c_str(), row,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE,
                static_cast<int>(i) == state.selected ? RGB(255, 226, 102) : RGB(235, 240, 255), 2);
        }
    }
    else
    {
        const auto slots = VisibleSlots(listView, inventory, catalog);
        if (slots.empty())
        {
            RECT row{panel.left + 20, panel.top + 45, panel.right - 20, panel.top + 45 + rowHeight};
            Win32Drawing::DrawShadowText(dc, font, "No items.", row,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE, RGB(190, 210, 240), 2);
        }
        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            const auto& stack = *inventory.Slot(slots[i]);
            const auto* item = catalog.Find(stack.itemId);
            if (!item) continue;
            std::string label = item->name;
            if (stack.quantity > 1) label += "   " + std::to_string(stack.quantity);
            RECT row{panel.left + 20, panel.top + 45 + static_cast<int>(i) * rowHeight,
                panel.left + 300, panel.top + 45 + static_cast<int>(i + 1) * rowHeight};
            Win32Drawing::DrawShadowText(dc, font, label.c_str(), row,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE,
                static_cast<int>(i) == state.selected ? RGB(255, 226, 102) : RGB(235, 240, 255), 2);
        }
        if (!slots.empty())
        {
            const int selected = std::clamp(state.selected, 0, static_cast<int>(slots.size()) - 1);
            const auto& stack = *inventory.Slot(slots[selected]);
            if (const auto* item = catalog.Find(stack.itemId))
            {
                const std::string details = DetailText(*item);
                RECT detail{panel.left + 315, panel.top + 48, panel.right - 15, panel.bottom - 20};
                Win32Drawing::DrawShadowText(dc, font, details.c_str(), detail,
                    DT_LEFT | DT_TOP, RGB(190, 210, 240), 2);
                if (state.view == View::Actions)
                {
                    const auto actions = Actions(*item);
                    for (std::size_t i = 0; i <= actions.size(); ++i)
                    {
                        const char* label = i == actions.size() ? "Cancel" : ActionName(actions[i]);
                        RECT row{detail.left, panel.top + 125 + static_cast<int>(i) * rowHeight,
                            detail.right, panel.top + 125 + static_cast<int>(i + 1) * rowHeight};
                        Win32Drawing::DrawShadowText(dc, font, label, row,
                            DT_LEFT | DT_VCENTER | DT_SINGLELINE,
                            static_cast<int>(i) == state.actionSelected ? RGB(255, 226, 102) : RGB(235, 240, 255), 2);
                    }
                }
                else if (state.view == View::DropQuantity)
                {
                    const std::string prompt = "Drop how many?  < " +
                        std::to_string(state.dropQuantity) + " >";
                    RECT row{detail.left, panel.top + 125, detail.right, panel.top + 125 + rowHeight};
                    Win32Drawing::DrawShadowText(dc, font, prompt.c_str(), row,
                        DT_LEFT | DT_VCENTER | DT_SINGLELINE, RGB(255, 226, 102), 2);
                }
            }
        }
    }
    SelectObject(dc, oldFont); DeleteObject(font);
#endif
}
}
