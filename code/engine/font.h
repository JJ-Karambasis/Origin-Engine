#ifndef FONT_H
#define FONT_H

#include <stb_truetype.h>

struct glyph_key {
	u32 GlyphIndex;
	f32 Size;
};

struct glyph {
	u64 		   Hash;
	v2  		   Dim;
	v2  		   Offset;
	f32 		   Advance;
	gfx_texture_id Texture;

	glyph* Next;
	glyph* Prev;
};

struct glyph_slot {
	glyph* First;
	glyph* Last;
};

#define MAX_GLYPH_SLOT_COUNT 4096
#define GLYPH_SLOT_MASK (MAX_GLYPH_SLOT_COUNT-1)
struct font {
	arena*  	   Arena;
	buffer 		   Buffer;
	glyph_slot 	   GlyphSlots[MAX_GLYPH_SLOT_COUNT];
	stbtt_fontinfo FontInfo;
	s32 		   Ascent;
	s32 		   Descent;
	s32 		   LineGap;
};

#endif