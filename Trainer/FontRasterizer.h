#pragma once
// Validate selected files with the same rasterizer as ImGui before committing
// them to the atlas; unsupported installed/custom files use the normal fallback.
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#include "imgui/imstb_truetype.h"
#undef STB_TRUETYPE_IMPLEMENTATION
#undef STBTT_STATIC
