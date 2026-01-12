// NOTE: size_px = size_pt * DPI/72.

function F_FontCache *
f_parse_and_cache_font_ttf(Arena *arena, String8 path, F32 *sizes_pt, U32 sizes_count)
{
  F_FontCache *result = 0;
  B32 error = 0;
  
  // Prepare FreeType for use
  
  FT_Library ft;
  FT_Error ft_result = FT_Init_FreeType(&ft);
  
  if (ft_result != 0) {
    error = 1;
  }
  
  FT_Face face; 
  ft_result = FT_New_Face(ft, (char *)path.data, 0, &face);
  
  if (ft_result != 0) {
    error = 1;
  }
  
  // Cache glyph information for each font size
  if (!error) {
    result = ArenaPushStruct(arena, F_FontCache);
    result->sizes = ArenaPushArray(arena, F_FontSize, sizes_count); 
    result->sizes_count = sizes_count;
    
    U32 off_x = 0;
    U32 off_y = 0;
    U32 max_glyph_height = 0;
    
    F_FontSize *sizes = result->sizes;
    
    for (U32 size_idx = 0; size_idx < sizes_count; size_idx += 1) {
      F_FontSize *curr_size = &sizes[size_idx];
      
      off_y += max_glyph_height;
      off_x = 0;
      max_glyph_height = 0; 
      
      F32 dpi = 96.f; // TODO: Use DPI of window's monitor.
      F32 size_pt = sizes_pt[size_idx]; 
      F32 size_px = size_pt * dpi/72.f;
      
      FT_Set_Pixel_Sizes(face, 0, (FT_UInt)size_px);
      FT_GlyphSlot g = face->glyph;
      
      for (S32 chr = 32; chr < F_GLYPH_MAX; chr += 1) {
        if (FT_Load_Char(face, chr, FT_LOAD_RENDER)) {
          continue;
        }
        
        U32 bitmap_w = g->bitmap.width;
        U32 bitmap_h = g->bitmap.rows;
        U32 bitmap_size = bitmap_w * bitmap_h;
        
        U8 *buff = ArenaPushArray(arena, U8, bitmap_size);
        MemoryCopy(buff, g->bitmap.buffer, bitmap_size);
        
        // NOTE: UV rect for rendering glyph can be calculated in render
        // layer from texture offsets and atlas dimensions.
        curr_size->adv[chr] = v2s32(g->advance.x>>6, g->advance.y>>6);
        curr_size->dim[chr] = v2u32(bitmap_w, bitmap_h);
        curr_size->spc[chr] = v2s32(g->bitmap_left, bitmap_h - g->bitmap_top);
        curr_size->off[chr] = v2u32(off_x, off_y);
        curr_size->bmp[chr] = buff;
        
        off_x += bitmap_w;
        max_glyph_height = Max(bitmap_h, max_glyph_height);
      }
      
      curr_size->max_glyph_height = max_glyph_height;
      curr_size->size_pt = size_pt;
    }
  }
  
  FT_Done_FreeType(ft);
  
  return result;
}

// NOTE: The methods I decided to use for determining width and height are 
// arbitrary; I simply chose what gave good results for several fonts.
function V2S32 
f_text_string_dim_px(String8 text, F_FontCache *font, F32 size_pt)
{
  V2S32 dim = {0};
  
  if (font) {
    U32 sizes_count = font->sizes_count; 
    U32 size_idx = 0;
    for (; size_idx < sizes_count; size_idx += 1) {
      F_FontSize *size = &font->sizes[size_idx];
      if (size->size_pt == size_pt) {
        break;
      }
    }
    
    F_FontSize *size = &font->sizes[size_idx];
    
    for (U64 chr_idx = 0; chr_idx < text.count; chr_idx += 1) {
      U8 chr = text.data[chr_idx];
      S32 space_x = size->adv[chr].x;
      S32 space_y = size->spc[chr].y;
      dim.x += space_x;
      dim.y = Max(dim.y, space_y);
    }
    // NOTE: Temporary; although there's no reason not to use it..
    dim.y = size->max_glyph_height;
  }
  
  return dim; 
}

