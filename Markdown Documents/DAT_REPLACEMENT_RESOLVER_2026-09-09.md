# DAT replacement resolver

Phase 3 adds a shared read policy for previewing modified DATs without overwriting
the retail installation. The implementation follows the replacement-root idea
reviewed in XI-Test-Client, with original DATura code and one policy shared across
the existing native readers.

## Use

1. Create a replacement folder and mirror the desired retail paths inside it,
   for example `D:\DATuraOverrides\ROM\1\35.DAT` for Bastok Markets.
2. Open **Resources > DAT Replacements and Sources...**. Choose that folder,
   check **Enable DAT replacements**, and select **Save for next startup**.
3. Restart DATura. The window shows the active policy separately from the saved
   next-startup settings. Replacements are disabled by default.
4. Load assets normally. **Refresh sources** shows the most recent 128 distinct
   DAT opens, including logical path, selected physical source, and Windows open
   errors. Resource inspection also displays its logical and physical paths;
   replaced zone-resource rows include source provenance in their details.

Settings are saved as one value under DATura's existing per-user registry key.
Saving does not change the active process. Startup application avoids mixing
already loaded zones, character assets, fonts, weather, or texture caches with a
new policy. Individual reads resolve the current file on disk without a negative
cache: adding/removing an override takes effect on the next actual load. Restart
after editing replacement files when a complete cache refresh is needed.

## Resolution contract

Eligible keys are numeric DAT asset paths under the configured install root:
`ROM/<directory>/<file>.DAT`, and `ROM2` through `ROM19`. Matching uses Windows
case-insensitive paths, normalized slashes and absolute dot-segment resolution.
The installation-root match includes a directory boundary, so similarly named
sibling directories cannot be remapped. A path that normalizes outside the root
is an explicit direct read. Explicit external DATs also remain direct reads.

The selected replacement path is `replacementRoot / relativeAssetPath`. If it is
absent, the reader uses the retail file. If it exists, it is selected even when
empty, malformed, a directory, locked, or inaccessible. Normal read/parser errors
then apply; there is no silent retry against retail. This lets a broken edited
asset remain visible as a problem instead of appearing to work through fallback.
Parsers retain their existing supported-format and validation behavior.

Logical paths are never replaced with physical paths in scene requests, RAPI
current-file context, zone identification, or companion path composition.
FTABLE/VTABLE mappings remain retail data; file-ID resolution reports both the
logical DAT path and the selected source. A valid mapped replacement can load
even when the corresponding retail asset file is absent.

Configuration files, text DAT-set manifests, standalone `.bgw`/`.spw` files, and
mapping tables are outside this numeric DAT policy. DAT-set member DATs are
resolved individually. Reads through the audio pipeline honor replacements for
numeric DATs, and decoded WAV cache keys include the selected physical path,
file size and last-write time. Writes/exports keep their original explicit paths;
the resolver never writes to the retail installation or replacement folder.

## Integration

`DATura/ffxi_dat_resolver.h` owns the process-wide synchronized settings, path
selection, read opener and bounded open history. It is header-only because the
standalone parser and graphics test targets embed these readers independently.

The policy is used by shared binary/text file reads, Noesis-compatible companion
and DAT-set reads, resource inspection, file-ID asset checks, direct model/menu
existence checks, the texture viewer, both bitmap-font loaders, and audio DAT
header/payload reads. Existing scene and room loading reaches it through these
readers. No parser receives a replacement-root filename as its logical context.

Open history records OS opens, not successful parsing or rendering. It is a
bounded troubleshooting view, not a complete permanent asset manifest. Windows
narrow-path/MAX_PATH limitations in the existing folder-picker UI remain.

Binary and companion readers now use 64-bit file lengths and reject values above
the signed parser limit; an oversized replacement cannot wrap a 32-bit size into
a small, apparently valid DAT.

## Verification

Build with the same Visual Studio v145/x64 toolchain as DATura:

```powershell
& 'C:/Program Files/Microsoft Visual Studio/18/Community/MSBuild/Current/Bin/MSBuild.exe' tests/DatResolverTests.vcxproj /p:Configuration=Release /p:Platform=x64
tests/bin/dat-resolver/Release/DatResolverTests.exe 'C:/Program Files (x86)/PlayOnline/SquareEnix/FINAL FANTASY XI'
```

Omit the installation argument for synthetic fixtures only. The 43-check full
suite passes, covering enable/disable, precedence, absent retail files, numbered
hives, case/slash/dot normalization, root boundaries, excluded paths, resource
and RAPI consistency, retail table mapping, empty/directory/locked replacements,
live disk changes between reads, reconfiguration, bounded history, oversized
sparse files and audio-cache separation without playback.

The installed fixture reads local copies of Bastok Markets and one of its rooms
as replacements. It parses the zone and loads all 14 companions, verifying one
room uses the replacement while another uses retail fallback, with zone ID 235
preserved. Those two temporary asset copies are removed after a successful test;
retail files are never modified. Synthetic fixtures remain under the test output
directory. Temporary WAV outputs created by the audio test are removed.

The existing bitmap-font regression also passes, checking the two retail atlases,
glyph metadata, malformed layouts, spacing, transparency and root reloads.
DATura Debug/x64 and Release/x64 builds pass. Native dialog interaction and
registry persistence were not UI-automated; tests configure isolated in-process
policy and do not alter the user's saved settings.

The next planned phase is persistent zone editing: stable identities, project
change sets, undo/redo, baseline comparison and reload.
