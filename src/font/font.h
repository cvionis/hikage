#pragma once

#define F_GLYPH_MAX 128

struct F_FontSize {
  V2S32 adv[F_GLYPH_MAX]; // Advances
  V2U32 dim[F_GLYPH_MAX]; // Sizes
  V2S32 spc[F_GLYPH_MAX]; // Left, top spacings
  V2U32 off[F_GLYPH_MAX]; // Offsets in texture atlas
  U8   *bmp[F_GLYPH_MAX]; // Bitmaps
  
  U32 max_glyph_height;
  F32 size_pt; 
};

struct F_FontCache {
  F_FontSize *sizes; // TODO: Use a better name.
  U32 sizes_count;
};

function F_FontCache *f_parse_and_cache_font_ttf(Arena *arena, String8 path, F32 *sizes_pt, U32 sizes_count);
function V2S32 f_text_string_dim_px(String8 text, F_FontCache *font, F32 size_pt);
