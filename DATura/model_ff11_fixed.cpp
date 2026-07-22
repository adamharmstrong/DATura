/*========================================================================================
 Noesis FF11 support
 Life regrettably wasted by Rich Whitehouse
 (c) Never, all rights to be taken to the grave.
========================================================================================*/

#include "stdafx.h"
#include "noesis_rapi.h"
#include "model_ff11.h"
#include "model_ff11_decrypt.h"
#pragma warning(disable: 4996)  // suppress deprecated POSIX name warnings (_stricmp etc.)
#pragma warning(disable: 4267)  // suppress size_t -> int conversion warnings

// All structs in the included implementation files that map directly onto binary
// file data must be tightly packed to match the FFXI DAT on-disk layout exactly.
#pragma pack(push, 1)

#include "model_ff11_common.inl"
#include "model_ff11_texture.inl"
#include "model_ff11_effect.inl"
#include "model_ff11_skeleton_animation.inl"
#include "model_ff11_geo.inl"
#include "model_ff11_map.inl"
#include "model_ff11_loader.inl"
#include "model_ff11_creation.inl"

#pragma pack(pop)
