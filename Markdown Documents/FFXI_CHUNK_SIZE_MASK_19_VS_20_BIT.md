# FFXI DAT Chunk-Size Mask: 19-Bit versus 20-Bit Interpretations

**Created:** 2026-07-27  
**Status:** Working decision for DATura; original-client behavior remains unverified

## Purpose

FFXI DAT chunks begin with a packed 32-bit information word.
DATura and xim agree about most of its interpretation but disagree about whether one particular header bit belongs to the chunk-size field.

DATura currently uses the 19-bit interpretation:

```cpp
chunkType = info & 0x7F;
chunkSize = (info >> 3) & 0x7FFFF0;
```

xim uses the 20-bit interpretation:

```cpp
chunkType = info & 0x7F;
chunkSize = (info >> 3) & 0xFFFFF0;
```

This document explains the difference and why DATura currently selects the 19-bit expression.
That selection is a conservative implementation decision, not a claim that the original FFXI client has been independently proven to use 19 bits.

## Common ground

Both interpretations agree that:

- raw header bits 0–6 identify the chunk type;
- the size is encoded in 16-byte units;
- shifting the packed word right by three converts the encoded size field into a 16-byte-aligned byte count;
- the low four bits of the resulting size are therefore zero;
- both formulas produce identical results unless raw header bit 26 is set.

The masks differ by exactly one decoded bit:

```text
19-bit mask: 0x7FFFF0
20-bit mask: 0xFFFFF0
                    ^
              difference = 0x800000
```

Decoded size bit `0x800000` comes from raw information-word bit 26:

```text
0x800000 << 3 = 0x04000000
```

Consequently, the disagreement can also be stated as:

> Is raw header bit 26 (`0x04000000`) the twentieth size bit, or is it outside the size field?

## Interpretation A: 19-bit size

```cpp
chunkSize = (info >> 3) & 0x7FFFF0;
```

Under this interpretation:

- raw bits 7–25 encode the size;
- raw bit 26 is excluded from the size;
- the largest representable nonzero aligned value is `0x7FFFF0`;
- the maximum is 16 bytes short of 8 MiB.

If raw bit 26 has another purpose—such as a flag or reserved field—masking it out is necessary to prevent that unrelated bit from increasing the decoded size by 8 MiB.

## Interpretation B: 20-bit size

```cpp
chunkSize = (info >> 3) & 0xFFFFF0;
```

Under this interpretation:

- raw bits 7–26 encode the size;
- raw bit 26 is the highest size bit;
- the largest representable nonzero aligned value is `0xFFFFF0`;
- the maximum is 16 bytes short of 16 MiB.

If authentic FFXI chunks can be 8 MiB or larger, this additional bit is required to represent their complete size.

## Observable consequence

Let `L` be the size produced from all mutually accepted size bits. When raw bit 26 is clear:

```text
19-bit result = L
20-bit result = L
```

When raw bit 26 is set:

```text
19-bit result = L
20-bit result = L + 0x800000
```

For example, suppose the accepted lower bits describe a size of `0x001230`. With raw bit 26 also set:

```text
19-bit result = 0x001230
20-bit result = 0x801230
```

The two candidate chunk boundaries would be separated by exactly 8 MiB. A valid file containing such a header would therefore provide a strong discriminating test.

## Evidence currently available

### Local corpus comparison

On 2026-07-27, both masks were tested against the installed FFXI DAT corpus:

```text
C:\Program Files (x86)\PlayOnline\SquareEnix\FINAL FANTASY XI
```

The scan examined 52,899 DAT files. A file was counted as a qualifying chunk container only when its chunks:

- formed an exact walk to the end of the file;
- had nonzero, 16-byte-aligned sizes that remained in bounds; and
- used types in FFXI Tool's documented `0x00`–`0x5D` range.

Results:

| Result | Count |
|---|---:|
| Qualifying files valid under both masks | 48,990 |
| Qualifying chunks walked identically | 2,020,681 |
| Files valid only under the 19-bit mask | 0 |
| Files valid only under the 20-bit mask | 0 |
| Qualifying chunks with raw bit 26 set | 0 |

These results establish that the two masks are operationally equivalent for every qualifying file in this installed corpus. They do **not** establish which interpretation the original client intended, because no qualifying header sets the only bit on which the formulas disagree.

The largest observed qualifying chunk was:

| Property | Value |
|---|---|
| File | `ROM4/0/8.DAT` (Ilrusi Atoll) |
| Chunk name | `d_pa` |
| Chunk type | `0x1C` |
| Header offset | `0x6E0C0` |
| Chunk size | `0x7E2EE0` = 8,269,536 bytes |
| 19-bit capacity used | 98.58% |
| Remaining 19-bit headroom | 119,056 bytes |

This chunk demonstrates that authentic data uses almost the entire 19-bit range. Its proximity to, but position below, the 8 MiB boundary is consistent with an intentional 19-bit authoring limit. That pattern is suggestive rather than conclusive: a format with a 20-bit field could also happen to contain no chunk above 8 MiB.

### The bit-allocation conflict

DATura and the historical FFXI Tool source do not treat raw bit 26 as unnamed or unused. They describe this complete packed layout:

```text
raw bits  0–6   type
raw bits  7–25  next/size in 16-byte units
raw bit   26    is_shadow
raw bit   27    is_extracted
raw bits 28–30  version
raw bit   31    is_virtual
```

DATura implements that interpretation in `DATura/model_ff11.h`:

```cpp
mType        = info & 0x7F;
mSize        = (info >> 3) & 0x7FFFF0;
mIsShadow    = ((info >> 26) & 1) != 0;
mIsExtracted = ((info >> 27) & 1) != 0;
mVersion     = (info >> 28) & 7;
mIsVirtual   = ((info >> 31) & 1) != 0;
```

Multiple historical FFXI Tool source versions declare the same allocation as `type:7`, `next:19`, `is_shadow:1`, `is_extracted:1`, `ver_num:3`, and `is_virtual:1`. Several later community viewers repeat that structure, although shared lineage means those copies should not be counted as fully independent discoveries.

This creates a direct semantic conflict:

- under the 19-bit interpretation, raw bit 26 is `is_shadow`;
- under the 20-bit interpretation, raw bit 26 contributes `0x800000` bytes to the chunk size.

The same bit cannot serve both purposes in the same header layout. If it is a shadow flag, the 20-bit mask turns a flagged small chunk into one that appears 8 MiB larger. If it is a size bit, the historical `is_shadow` field is a mistaken label, and the 19-bit mask truncates every chunk whose encoded size is at least 8 MiB.

The local corpus does not resolve that conflict because neither raw bit 26 nor raw bit 27 appears in any qualifying chunk. Consequently, the corpus validates the lower 19 size bits and the surrounding version/virtual fields, but it provides no observed example of the two disputed flag positions.

### Evidence favoring 19 bits

1. **DATura already uses the 19-bit expression consistently.**  
   The main parser in `DATura/model_ff11.h` and the current corpus-analysis tools use `0x7FFFF0`. Known DAT workflows have been developed and tested around this behavior.

2. **A contributed disassembly report explicitly shows `AND 0x7FFFF0`.**  
   [`FFXI_RENDERING_FINDINGS_FOR_COLLABORATORS_2026-07-22.md`](FFXI_RENDERING_FINDINGS_FOR_COLLABORATORS_2026-07-22.md) reports an original-client chunk walker containing:

   ```text
   and ecx, 0x7FFFF0
   ```

   If accurate and taken from the relevant path, that instruction is direct support for the 19-bit interpretation.

3. **No known DAT currently requires the extra bit.**  
   The local comparison found no qualifying file that parsed only with the wider mask and no qualifying chunk with raw bit 26 set. The largest observed chunk comes within 119,056 bytes of the 19-bit ceiling but does not cross it. Therefore, the twentieth candidate bit has not been shown to be necessary for a known authentic chunk.

4. **The disputed bit has a historically documented competing meaning.**  
   DATura, three FFXI Tool source versions, and several related viewers place `is_shadow` at raw bit 26. This does not prove the label is correct, but it gives the narrower field a concrete structural explanation.

### Evidence favoring 20 bits

1. **xim implements the wider mask.**  
   The correction document reports xim's expression as `(info >> 3) & 0xFFFFF0`.
   This is inspectable reimplementation evidence that another reverse-engineering effort interpreted raw bit 26 as part of the size.

2. **The packed layout is superficially compatible with a contiguous 20-bit field.**  
   Treating raw bits 7–26 as one uninterrupted size field is structurally plausible if the historical `is_shadow` interpretation is wrong. However, the 20-bit interpretation must explicitly reject or relocate that flag; it cannot simply add capacity without changing the rest of the header layout.

3. **It doubles the representable chunk range.**  
   A 20-bit field supports aligned chunks up to `0xFFFFF0`, just under 16 MiB. That can be useful for custom DAT-like containers, future extensions, or tolerant analysis tools even though no qualifying installed FFXI file currently requires it.

4. **Existing corpus success does not distinguish the formulas.**  
   Raw bit 26 was clear in all 2,020,681 qualifying chunks, so both parsers walked exactly the same data. Successful parsing under the 19-bit mask therefore does not prove that the wider interpretation is wrong.

## Why DATura currently selects 19 bits

DATura retains `0x7FFFF0` for four practical reasons.

### 1. It preserves established behavior

The 19-bit expression is already used by DATura's loader and its analysis utilities.
Changing every parser to 20 bits without a discriminating fixture would alter a foundational format rule without demonstrating that the change fixes a real file.

### 2. The only client-behavior evidence presently reported favors 19 bits

The contributed disassembly account specifically reports `AND 0x7FFFF0`.
DATura cannot currently reproduce that trace because the underlying binary, raw instruction bytes, and analysis project are not available in this repository.
The report must therefore remain labeled externally reported rather than “retail-confirmed.”

Even with that limitation, a specific claimed client instruction is more directly relevant to original-client behavior than the mask selected by a reimplementation.
It is reasonable evidence for a working choice, though not enough to close the question.

### 3. There is no demonstrated need for the wider range

No qualifying installed example requires an encoded chunk size between 8 and 16 MiB, and the 20-bit mask did not uniquely recover a single file.
Selecting the wider mask solely because it permits larger chunks would add capacity that has not been connected to an authentic FFXI record.

### 4. The conservative interpretation avoids treating an unknown bit as size

When the meaning of a disputed bit is unknown, including it in a length field has a large consequence: it moves the next chunk boundary forward by 8 MiB.
If the historical `is_shadow` interpretation is correct, the wider mask can turn a flagged small chunk into an impossible or out-of-bounds chunk.

The historical field declarations and the near-limit corpus result make the flag interpretation plausible, but they do not prove it. This is a risk-management reason to retain the narrower interpretation until evidence demonstrates that the bit carries size.

## Decision

DATura will currently use:

```cpp
chunkSize = (info >> 3) & 0x7FFFF0;
```

The decision should be described as:

> **Selected working interpretation: 19-bit size.**

It should not be described as:

> **Retail-confirmed**, **proven**, or **the 20-bit mask is wrong**.

The 20-bit interpretation remains plausible.
DATura's selection reflects compatibility, conservative boundary handling, the absence of a known chunk requiring the extra bit,
the historical `is_shadow` allocation, the observed near-8-MiB ceiling, and an externally reported original-client instruction that favors 19 bits.

### Optional diagnostic fallback

The wider interpretation can still be useful as a diagnostic for custom or future files. A tolerant tool may:

1. attempt the 19-bit boundary first;
2. when raw bit 26 is set and the 19-bit walk is structurally invalid, test the 20-bit boundary;
3. accept the 20-bit result only when it produces a demonstrably valid in-bounds structure; and
4. report that the file uses an ambiguous or nonstandard header interpretation.

This fallback should not silently replace the default mask. If both candidate boundaries appear valid, the tool should preserve the ambiguity instead of guessing, because choosing 20 bits also discards the historical `is_shadow` meaning.

## What would resolve the disagreement

The strongest practical test would be an authentic, valid DAT chunk header with raw bit 26 set. For that file, we should:

1. decode the header with both masks;
2. check which candidate boundary lands on a structurally valid next chunk;
3. validate that the candidate size remains within the containing file or archive;
4. inspect the payload structure and any internal offsets against both boundaries;
5. verify whether raw bit 26 correlates with some non-size property across additional records.

Original-client evidence could also resolve the question if it is independently inspectable and reproducible. Useful artifacts would include:

- the relevant executable build identity;
- raw bytes surrounding the chunk-walker instruction;
- the function's calling context;
- a decompiler listing tied to those bytes;
- a runtime trace showing the same mask on a real DAT header;
- confirmation that no alternate chunk-walking path uses a different mask.

Until one of those tests distinguishes the formulas, the correct conclusion is not that 19 bits have been proven.
It is that 19 bits are DATura's better-supported and lower-risk working choice.

## Related documents

- [`FFXI_RENDERING_ADJUDICATED_FINDINGS.md`](FFXI_RENDERING_ADJUDICATED_FINDINGS.md)
- [`FFXI_GEOMETRY_AND_TEXTURE_RENDERING Corrections.md`](<FFXI_GEOMETRY_AND_TEXTURE_RENDERING Corrections.md>)
- [`FFXI_RENDERING_FINDINGS_FOR_COLLABORATORS_2026-07-22.md`](FFXI_RENDERING_FINDINGS_FOR_COLLABORATORS_2026-07-22.md)
