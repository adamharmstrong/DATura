# FFXI cloud/sky checkerboarding — cause and fix

Applies to any FFXI model/zone viewer that uploads DAT textures to a GPU. Symptom: cloud layers, and sometimes skin or other "opaque" surfaces, show a fine per-texel checkerboard of light/dark or solid/see-through.

## Cause

FFXI stores partial transparency as a **dither/stipple pattern**, not as smooth alpha. Two encodings do this, and both show up in sky textures:

**DXT1 — 1-bit alpha.** A texel is either fully opaque or fully transparent. "50% transparent" is faked as a checkerboard of on/off texels. Transparent DXT1 texels also carry **black RGB**, so any filtering that bleeds colour out of them produces dark specks.

**DXT3 — 4-bit alpha.** Only multiples of 17 are representable (0, 17, ..., 119, 136, ..., 255). FFXI's "fully opaque" is half-scale `0x80` = 128, which is **not representable in 4 bits**, so encoders alternate nibble 7 (119) and nibble 8 (136) to average out at 128. That alternation is a per-texel checkerboard. An alpha histogram of an affected texture typically shows only three values: 0, 119, 136 — roughly 48% / 48% / remainder.

Two things in a modern renderer turn that stored pattern into a visible one:

**1. A per-texel alpha threshold sitting inside the dithered band.** This is the big one, and it produces the crispest, ugliest version. FFXI alpha is half-scale, so a shader typically nets a x2 (commonly `4 * vColor.a * tex.a` with a neutral `0x80` vertex alpha). Under that scaling, nibble 8 -> 1.07 (clamps to 1.0) but nibble 7 -> **0.937**. Any "is this texel opaque?" test with a cutoff between those two values — an alpha test, an `alphaTest`/`MASK` cutoff, or a split between an opaque pass and a blended no-depth-write pass — classifies half the texels one way and half the other, per texel. Tell-tale sign: **the dots stay razor-sharp even where the texture is magnified and blurry**, because they come from the threshold, not from the texels.

**2. No mipmaps at roughly 1:1 texel-to-pixel.** `LINEAR` filtering degenerates to point sampling when one texel covers about one pixel, so nothing averages the stipple away. The original game draws the sky shell heavily minified and filtered, and the pattern dissolves into a soft gradient there.

## Diagnosis

Before changing anything, decode one affected texture and print an alpha histogram.

- Only `{0, 119, 136}` present -> DXT3 dithered-opaque. The threshold (cause 1) is almost certainly the bug.
- Only `{0, 255}` present -> DXT1 1-bit alpha. It is a genuine stipple in the source art; filtering/mip work (fix 2) is the answer.
- A smooth spread of values -> the dither is not your problem, look elsewhere.

Also worth checking: does the checkerboard survive when the surface is magnified and visibly blurry? If yes, it is a threshold artefact, not a sampling artefact.

## Fix 1 — get every alpha cutoff out of the dithered band

Cheapest fix, usually a one-line change, do it first.

Any per-texel alpha test or opaque/blended classification must use a cutoff **well below** the dithered-opaque band:

- Dithered-opaque lands at **0.937 and above** after the x2 scaling.
- Genuine FFXI translucency is alpha <= `0x60` (about 0.75 scaled), and typically <= `0x40` (0.5).

A cutoff of **0.5** — the glTF `MASK` default — separates those cleanly: dithered-opaque always classifies as solid, real translucency still blends. A cutoff of 0.99 does not, and that is the classic form of this bug.

Sanity check on the other extreme: a renderer that treats everything as solid unless alpha is exactly zero (`transparent: false, alphaTest: 1/255`) never exhibits this at all — which is why some viewers of the same data look clean.

## Fix 2 — undither the sky/cloud textures at upload

Fix 1 stops the threshold from amplifying the pattern. If the stipple is genuinely in the art (DXT1), it will still be visible up close. Removing it means resolving the dither on the CPU before upload:

- Decode DXT to RGBA on the CPU. Uploading the compressed block format straight to the GPU preserves 1-bit DXT1 alpha, so it would still checkerboard.
- Run a 5x5 box filter over the alpha channel. A 50% stipple becomes a continuous ~0.5.
- For fully transparent texels, take RGB from opaque neighbours weighted by their alpha, so DXT1's black holes do not bleed dark specks into the sky.
- Upload as uncompressed RGBA, generate mipmaps, and use trilinear minification.

```js
/**
 * Convert dithered (checkerboard) alpha into continuous alpha.
 * A 5x5 box filter turns a 50% stipple into ~0.5 everywhere; fully transparent
 * texels borrow RGB from opaque neighbours so black holes don't speck the sky.
 * src: RGBA bytes, returns new RGBA bytes.
 */
function unditherAlphaRGBA(src, w, h) {
  const out = new Uint8Array(src.length);
  const R = 2;                                   // 5x5 kernel
  for (let y = 0; y < h; y++) {
    for (let x = 0; x < w; x++) {
      let aSum = 0, n = 0, rSum = 0, gSum = 0, bSum = 0, wSum = 0;
      for (let dy = -R; dy <= R; dy++) {
        const yy = y + dy;
        if (yy < 0 || yy >= h) continue;
        for (let dx = -R; dx <= R; dx++) {
          const xx = x + dx;
          if (xx < 0 || xx >= w) continue;
          const i = (yy * w + xx) * 4;
          const a = src[i + 3];
          aSum += a;
          n++;
          if (a > 0) {                           // colour average, alpha-weighted
            rSum += src[i] * a;
            gSum += src[i + 1] * a;
            bSum += src[i + 2] * a;
            wSum += a;
          }
        }
      }
      const o = (y * w + x) * 4;
      const aSrc = src[o + 3];
      if (aSrc < 8 && wSum > 0) {                // transparent: borrow neighbour colour
        out[o]     = (rSum / wSum + 0.5) | 0;
        out[o + 1] = (gSum / wSum + 0.5) | 0;
        out[o + 2] = (bSum / wSum + 0.5) | 0;
      } else {
        out[o]     = src[o];
        out[o + 1] = src[o + 1];
        out[o + 2] = src[o + 2];
      }
      out[o + 3] = n ? ((aSum / n) + 0.5) | 0 : aSrc;
    }
  }
  return out;
}
```

Upload path (WebGL2; the same shape applies in any API):

```js
let rgba = image.format === 'rgba32'
  ? image.data
  : decodeDxt(image.data, image.width, image.height, image.format === 'dxt1' ? 1 : 3);

rgba = unditherAlphaRGBA(rgba, image.width, image.height);

gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, image.width, image.height, 0,
              gl.RGBA, gl.UNSIGNED_BYTE, rgba);
gl.generateMipmap(gl.TEXTURE_2D);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR_MIPMAP_LINEAR);
gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
```

Notes:

- Apply this to sky and cloud shells only. It is a CPU pass per texture and it costs the compressed-format memory saving, so it is not worth doing to every world texture.
- Even without the CPU pass, generating mipmaps and using `LINEAR_MIPMAP_LINEAR` removes most of the shimmer at distance. It will not remove the pattern where the sky is drawn near 1:1.
- If the clouds are drawn as camera-following shells or particle quads, hook the undither where those specific textures are created, not in the shared texture path.

## Fix 3 — related trap: FFXI's half-scale alpha

Separate bug that often shows up alongside this one. FFXI alpha is half-scale: `0x80` = fully opaque, and the render path multiplies by 2 to compensate. Code that decides "is this texture half-scale?" by testing `maxAlpha <= 128` **fails on every DXT3 texture**, because 4-bit alpha cannot store 128 — the peak is 136. The doubling then never happens and the whole image draws at about 50% opacity.

- Raise that ceiling to **136**.
- After doubling, snap anything at or above ~230 to 255. The surviving nibble-7 texels double to 238, a ~7% ripple that reads as a faint shimmer; both nibbles mean "opaque", so snapping is correct.
- Do not apply the doubling blindly — some textures (character skins, icon sheets) genuinely use the full 0-255 range. Decide per texture from its own maximum.

## Order of work

1. Histogram an affected texture to confirm which encoding you are dealing with.
2. Move every alpha cutoff to 0.5 or below. Re-check — this alone often fixes it entirely.
3. Add mipmaps + trilinear for sky textures.
4. Add the CPU undither for sky/cloud textures if the stipple is still visible up close.
5. Verify the half-scale alpha test separately, if surfaces look uniformly 50% see-through.
