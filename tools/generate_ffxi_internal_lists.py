from __future__ import annotations

import csv
from pathlib import Path


ALTANA_PC_ROOT = Path(r"C:\Users\nanob\FFXI Stuff\AltanaViewer\List\PC")
OUT_HEADER = Path(r"C:\Users\nanob\source\repos\DATura\DATura\ffxi_internal_lists.h")


RACE_BASES = {
    "HumeM": {
        "table": "HumeM",
        "slots": {"Head": (27, 103), "Body": (28, 7), "Hands": (28, 52), "Legs": (28, 84), "Feet": (28, 116),
                  "Main": (29, 20), "Sub": (29, 20), "Range": (29, 20), "Action": (32, 13), "Motion": (32, 13), "WS": (32, 13)},
    },
    "HumeF": {
        "table": "HumeF",
        "slots": {"Head": (32, 79), "Body": (32, 111), "Hands": (33, 28), "Legs": (33, 60), "Feet": (33, 92),
                  "Main": (33, 124), "Sub": (33, 124), "Range": (33, 124), "Action": (36, 117), "Motion": (36, 117), "WS": (36, 117)},
    },
    "ElvaanM": {
        "table": "ElvaanM",
        "slots": {"Head": (37, 52), "Body": (37, 83), "Hands": (37, 127), "Legs": (38, 30), "Feet": (38, 61),
                  "Main": (38, 92), "Sub": (38, 92), "Range": (38, 92), "Action": (41, 84), "Motion": (41, 84), "WS": (41, 84)},
    },
    "ElvaanF": {
        "table": "ElvaanF",
        "slots": {"Head": (42, 25), "Body": (42, 56), "Hands": (42, 100), "Legs": (43, 3), "Feet": (43, 34),
                  "Main": (43, 65), "Sub": (43, 65), "Range": (43, 65), "Action": (46, 57), "Motion": (46, 57), "WS": (46, 57)},
    },
    "Tarutaru": {
        "table": "Tarutaru",
        "slots": {"Head": (46, 114), "Body": (47, 17), "Hands": (47, 61), "Legs": (47, 92), "Feet": (47, 123),
                  "Main": (48, 26), "Sub": (48, 26), "Range": (48, 26), "Action": (51, 19), "Motion": (51, 19), "WS": (51, 19)},
    },
    "Mithra": {
        "table": "Mithra",
        "slots": {"Head": (51, 94), "Body": (52, 13), "Hands": (52, 57), "Legs": (52, 88), "Feet": (52, 119),
                  "Main": (53, 22), "Sub": (53, 22), "Range": (53, 22), "Action": (56, 14), "Motion": (56, 14), "WS": (56, 14)},
    },
    "Galka": {
        "table": "Galka",
        "slots": {"Head": (56, 80), "Body": (56, 111), "Hands": (57, 27), "Legs": (57, 58), "Feet": (57, 89),
                  "Main": (57, 120), "Sub": (57, 120), "Range": (57, 120), "Action": (60, 112), "Motion": (60, 112), "WS": (60, 112)},
    },
}

DATURA_RACE_ORDER = ["HumeM", "HumeF", "ElvaanM", "ElvaanF", "Tarutaru", "Tarutaru", "Mithra", "Galka"]
SLOTS = ["Head", "Body", "Hands", "Legs", "Feet", "Main", "Sub", "Range", "Action", "Motion", "WS"]
COMBO_INDEXED_SLOTS = {"Head", "Main", "Sub", "Range"}


def c_escape(text: str) -> str:
    text = text.replace("\\", "\\\\").replace('"', '\\"')
    return "".join(ch if 32 <= ord(ch) <= 126 else "?" for ch in text)


def iter_file_refs(key: str):
    current_folder = None
    for token in key.split(";"):
        token = token.strip()
        if not token or token.startswith("..\\"):
            continue
        if "/" in token:
            folder_text, files_text = token.split("/", 1)
            if not folder_text.isdigit():
                continue
            current_folder = int(folder_text)
        else:
            if current_folder is None:
                continue
            files_text = token

        for part in files_text.split("/"):
            part = part.strip()
            if not part:
                continue
            if "-" in part:
                first, last = part.split("-", 1)
                if first.isdigit() and last.isdigit():
                    for file_no in range(int(first), int(last) + 1):
                        yield current_folder, file_no
            elif part.isdigit():
                yield current_folder, int(part)


def read_slot_entries(race: str, slot: str):
    path = ALTANA_PC_ROOT / race / f"{slot}.csv"
    if not path.exists():
        return {}

    base_folder, base_file = RACE_BASES[race]["slots"][slot]
    entries: dict[int, str] = {}
    with path.open("r", encoding="utf-8", errors="replace", newline="") as fp:
        reader = csv.reader(fp)
        category = None
        for row in reader:
            if not row:
                continue
            key = row[0].strip()
            if not key:
                continue
            if key.startswith("@"):
                category = key[1:].strip() or None
                continue
            label = row[1].strip() if len(row) > 1 else ""
            if not label:
                continue
            if category and label != category and slot in {"Main", "Sub", "Range", "Action", "Motion", "WS"}:
                label = f"{category}: {label}"
            for folder, file_no in iter_file_refs(key):
                offset = (folder - base_folder) * 128 + (file_no - base_file)
                if offset < 0:
                    continue
                index = offset + 1 if slot in COMBO_INDEXED_SLOTS else offset
                old = entries.get(index)
                if old is None or old == "None":
                    entries[index] = label
    return entries


def array_name(race: str, slot: str) -> str:
    return f"kFFXIInternal{RACE_BASES[race]['table']}{slot}Labels"


def main() -> None:
    all_entries: dict[tuple[str, str], dict[int, str]] = {}
    for race in RACE_BASES:
        for slot in SLOTS:
            all_entries[(race, slot)] = read_slot_entries(race, slot)

    lines: list[str] = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// Generated by tools/generate_ffxi_internal_lists.py from AltanaViewer/List.")
    lines.append("// These tables provide native DATura labels for low-poly PC equipment and animation lists.")
    lines.append("")
    lines.append("struct FFXIInternalListEntry")
    lines.append("{")
    lines.append("    int index;")
    lines.append("    const char *label;")
    lines.append("};")
    lines.append("")
    lines.append("struct FFXIInternalList")
    lines.append("{")
    lines.append("    const FFXIInternalListEntry *entries;")
    lines.append("    int count;")
    lines.append("};")
    lines.append("")
    lines.append("enum FFXIInternalPCSlot")
    lines.append("{")
    for i, slot in enumerate(SLOTS):
        comma = "," if i + 1 < len(SLOTS) else ""
        lines.append(f"    kFFXIInternalPCSlot_{slot}{comma}")
    lines.append("};")
    lines.append("")
    lines.append("#define FFXI_INTERNAL_LIST_COUNT(arr) (int)(sizeof(arr) / sizeof(arr[0]))")
    lines.append("")

    for race in RACE_BASES:
        for slot in SLOTS:
            entries = all_entries[(race, slot)]
            name = array_name(race, slot)
            lines.append(f"static const FFXIInternalListEntry {name}[] =")
            lines.append("{")
            for index, label in sorted(entries.items()):
                lines.append(f"    {{ {index}, \"{c_escape(label)}\" }},")
            lines.append("};")
            lines.append("")

    lines.append("static const FFXIInternalList kFFXIInternalPCLists[][11] =")
    lines.append("{")
    for race in DATURA_RACE_ORDER:
        lines.append("    {")
        for slot in SLOTS:
            name = array_name(race, slot)
            lines.append(f"        {{ {name}, FFXI_INTERNAL_LIST_COUNT({name}) }},")
        lines.append("    },")
    lines.append("};")
    lines.append("")
    lines.append("static inline const char *FFXIInternal_FindPCLabel(int raceIndex, int slot, int index)")
    lines.append("{")
    lines.append("    if (raceIndex < 0 || raceIndex >= (int)(sizeof(kFFXIInternalPCLists) / sizeof(kFFXIInternalPCLists[0])) ||")
    lines.append("        slot < 0 || slot >= 11)")
    lines.append("        return nullptr;")
    lines.append("")
    lines.append("    const FFXIInternalList &list = kFFXIInternalPCLists[raceIndex][slot];")
    lines.append("    for (int i = 0; i < list.count; ++i)")
    lines.append("    {")
    lines.append("        if (list.entries[i].index == index)")
    lines.append("            return list.entries[i].label;")
    lines.append("    }")
    lines.append("    return nullptr;")
    lines.append("}")
    lines.append("")

    OUT_HEADER.write_text("\n".join(lines), encoding="ascii")


if __name__ == "__main__":
    main()
