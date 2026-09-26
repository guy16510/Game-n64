#pragma once

// Start from Jet's pinned upstream template so new required feature switches are
// inherited automatically. Override only the native-validation settings here.
#include "JetConfig.example.hpp"

#undef JET32_WORLD_SCALE
#define JET32_WORLD_SCALE 4

#undef HALF_WIDTH_BUFFERS
#define HALF_WIDTH_BUFFERS 0

#undef FIELD_BUFFERS
#define FIELD_BUFFERS 0

#undef SSR_FIELD_REFLECT
#define SSR_FIELD_REFLECT 0

#undef Z_BUFFERING
#define Z_BUFFERING 1

#undef LIGHTING
#define LIGHTING 1

#undef TEXTURE_MAPPING
#define TEXTURE_MAPPING 0

#undef PERSPECTIVE_CORRECT_TEXTURES
#define PERSPECTIVE_CORRECT_TEXTURES 0

#undef POSTFX_ANTIALIASING
#define POSTFX_ANTIALIASING 0
#undef POSTFX_BLOOM
#define POSTFX_BLOOM 0
#undef POSTFX_MOTION_BLUR
#define POSTFX_MOTION_BLUR 0
#undef POSTFX_CHROMATIC
#define POSTFX_CHROMATIC 0
#undef POSTFX_PIXELATE
#define POSTFX_PIXELATE 0
