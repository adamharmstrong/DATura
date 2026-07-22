# DATura

DATura is an open-source FINAL FANTASY XI DAT exploration and visualization project.

## Resource inspection

The **Resources** menu exposes non-model data learned from the
[Windower/POLUtils](https://github.com/Windower/POLUtils) readers:

- **Current Zone Dialog / NPCs...** resolves and combines the loaded zone's dialog and entity-name DATs.
- **Open Resource DAT...** inspects an individual resource file.

The resource browser currently recognizes:

- VTABLE/FTABLE file IDs across `ROM` through `ROM19`;
- zone dialog tables and 32-byte NPC/mob lists;
- `d_msg` string tables (all three known layouts), `XISTRING`, and legacy 64-byte string tables;
- encrypted 0xC00-byte item records, including general items, equipment, usable items, currencies, slips, and related layouts;
- `BGMStream` and `SeWave` audio headers; and
- embedded FFXI graphic-header metadata.

Text is decoded as Windows CP932 with known FFXI element, Auto-Translate,
resource-reference, and dialog control markers. Rare private glyphs may still
appear as fallback characters. Audio support is header inspection only, and the
graphic scanner inventories embedded headers without exporting their pixels.

For parser regression checks, build `tools/ffxi_resource_probe.vcxproj` and run:

```text
ffxi_resource_probe <resource-path>
ffxi_resource_probe --id <ffxi-install-directory> <file-id>
```

## License

Except for separately identified third-party material, DATura is free software licensed under the [GNU General Public License, version 3 or later](LICENSE).

Third-party components and reference material retain their respective copyright and license terms. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the currently identified material.

The DATura license does not grant permission to copy or redistribute FINAL FANTASY XI game data or other assets owned by their respective rights holders.
