#!/usr/bin/env python3
"""Generate DATura/npc_trigger_dialogue.csv from a LandSandBoat checkout.

Retail NPCs answer a click in one of two ways: the server starts one of the
NPC's client event scripts (startEvent), or it sends a single line from the
zone's dialog DAT (showText / messageSpecial / messageText). Neither choice is
stored in the client DATs, so DATura reads it from LandSandBoat:

  * scripts/zones/<Zone>/DefaultActions.lua  (plain talk NPCs)
  * scripts/zones/<Zone>/npcs/<Name>.lua     (onTrigger handlers)
  * scripts/zones/<Zone>/IDs.lua             (text constant -> dialog DAT id)

DefaultActions wins over an onTrigger handler for the same NPC, since those
handlers only cover quest branches. Otherwise the last literal startEvent is used, because LSB
handlers list quest branches first and the plain greeting in the final else.
Regional merchants use their "open for business" line.

Usage: generate_npc_trigger_dialogue.py <lsb-root> <out.csv> [zone_id=Zone_Dir ...]
"""
import csv
import os
import re
import sys

DEFAULT_ZONES = {
    234: "Bastok_Mines",
    235: "Bastok_Markets",
    236: "Port_Bastok",
    237: "Metalworks",
}

TEXT_CALL = re.compile(
    r"(?:showText|messageSpecial|messageText|messageName)\(\s*(?:npc\s*,\s*)?"
    r"(?:ID|zones\[[^\]]+\])\.text\.([A-Z0-9_]+)")
START_EVENT = re.compile(r"startEvent\(\s*(\d+)")


def read(path):
    with open(path, encoding="utf-8", errors="replace") as handle:
        return handle.read()


def text_ids(zone_dir):
    ids = {}
    path = os.path.join(zone_dir, "IDs.lua")
    if os.path.exists(path):
        for name, value in re.findall(r"^\s*([A-Z0-9_]+)\s*=\s*(\d+)", read(path), re.M):
            ids.setdefault(name, int(value))
    return ids


def on_trigger(source):
    match = re.search(r"entity\.onTrigger\s*=\s*function\s*\([^)]*\)(.*?)\n end\b|"
                      r"entity\.onTrigger\s*=\s*function\s*\([^)]*\)(.*?)\nend\b",
                      source, re.S)
    if not match:
        return None
    return match.group(1) or match.group(2) or ""


def display_name(file_stem):
    return file_stem.replace("_", " ")


def zone_rows(zone_id, zone_dir):
    ids = text_ids(zone_dir)
    rows = {}
    default_names = set()

    defaults = os.path.join(zone_dir, "DefaultActions.lua")
    if os.path.exists(defaults):
        for key, body in re.findall(r"\[\s*'([^']+)'\s*\]\s*=\s*\{([^}]*)\}", read(defaults)):
            event = re.search(r"\bevent\s*=\s*(\d+)", body)
            text = re.search(r"\b(?:text|messageSpecial)\s*=\s*ID\.text\.([A-Z0-9_]+)", body)
            if event:
                rows[display_name(key)] = ("event", int(event.group(1)), "DefaultActions")
                default_names.add(display_name(key))
            elif text and text.group(1) in ids:
                rows[display_name(key)] = ("text", ids[text.group(1)], "DefaultActions " + text.group(1))
                default_names.add(display_name(key))

    npc_dir = os.path.join(zone_dir, "npcs")
    if os.path.isdir(npc_dir):
        for file_name in sorted(os.listdir(npc_dir)):
            if not file_name.endswith(".lua"):
                continue
            stem = file_name[:-4]
            source = read(os.path.join(npc_dir, file_name))
            # File names follow the retail spelling more reliably than the
            # "-- NPC:" header comments (e.g. Gelzerio.lua says "Galzerio").
            name = display_name(stem)
            if name in default_names:
                # DefaultActions holds the plain greeting; onTrigger handlers
                # for these NPCs are quest-only branches.
                continue
            body = on_trigger(source)
            if body is None:
                continue
            events = START_EVENT.findall(body)
            texts = [t for t in TEXT_CALL.findall(body) if t in ids]
            keys = [stem.upper().replace("-", "_"), re.sub(r"[^A-Z0-9]", "", stem.upper())]
            if "handleRegionalShop" in body:
                found = [k + s for s in ("_OPEN_DIALOG", "_CLOSED_DIALOG") for k in keys if k + s in ids]
                if found:
                    rows[name] = ("text", ids[found[0]], "regional " + found[0])
            elif events:
                rows[name] = ("event", int(events[-1]), "onTrigger")
            elif texts:
                rows[name] = ("text", ids[texts[0]], "onTrigger " + texts[0])
            else:
                # Shared shop helpers (e.g. handleValerianoShop) greet with
                # the NPC's <NAME>_SHOP_DIALOG line.
                found = [k + "_SHOP_DIALOG" for k in keys if k + "_SHOP_DIALOG" in ids]
                if found:
                    rows[name] = ("text", ids[found[0]], "shop " + found[0])
    return [(zone_id, name, kind, value, note) for name, (kind, value, note) in sorted(rows.items())]


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    root, out_path = sys.argv[1], sys.argv[2]
    zones = dict(DEFAULT_ZONES)
    if len(sys.argv) > 3:
        zones = {int(a.split("=", 1)[0]): a.split("=", 1)[1] for a in sys.argv[3:]}
    rows = []
    for zone_id, zone_name in sorted(zones.items()):
        rows.extend(zone_rows(zone_id, os.path.join(root, "scripts", "zones", zone_name)))
    with open(out_path, "w", newline="", encoding="utf-8") as handle:
        handle.write("# Generated by tools/generate_npc_trigger_dialogue.py from LandSandBoat scripts.\n")
        handle.write("# kind=event: run this retail event script id. kind=text: show this zone dialog DAT message id.\n")
        writer = csv.writer(handle, lineterminator="\n")
        writer.writerow(["zone_id", "name", "kind", "id", "source"])
        writer.writerows(rows)
    print(f"wrote {len(rows)} rows to {out_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
