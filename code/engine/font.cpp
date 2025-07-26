function KEY_HASH_FUNC(Hash_Glyph_Key) {
	glyph_key* GlyphKey = (glyph_key*)Key;
	return U64_Hash_Bytes(GlyphKey, sizeof(glyph_key));
}

function KEY_COMP_FUNC(Compare_Glyph_Key) {
	glyph_key* A = (glyph_key*)KeyA;
	glyph_key* B = (glyph_key*)KeyB;
	return A->Size == B->Size && A->GlyphIndex == B->GlyphIndex;
}

function font* Font_Create(buffer FontBuffer) {
	arena* Arena = Arena_Create();
	font* Font = Arena_Push_Struct(Arena, font);
	Font->Arena = Arena;
	Font->Buffer = Buffer_Copy((allocator*)Arena, FontBuffer);
	stbtt_InitFont(&Font->FontInfo, Font->Buffer.Ptr, stbtt_GetFontOffsetForIndex(Font->Buffer.Ptr, 0));
	
	int Ascent, Descent, LineGap;
	stbtt_GetFontVMetrics(&Font->FontInfo, &Ascent, &Descent, &LineGap);

	Font->Ascent = Abs(Ascent);
	Font->Descent = Abs(Descent);
	Font->LineGap = Ascent - Descent + LineGap;

	return Font;
}

function u32 Font_Get_Glyph_Index(font* Font, u32 Codepoint) {
	u32 GlyphIndex = (u32)stbtt_FindGlyphIndex(&Font->FontInfo, Codepoint);
	return GlyphIndex;
}

function glyph* Font_Get_Glyph_From_Index(font* Font, u32 GlyphIndex, f32 Size) {
	glyph_key Key = { .GlyphIndex = GlyphIndex, .Size = Size };
	u64 Hash = U64_Hash_Bytes(&Key, sizeof(glyph_key));
	u64 SlotIndex = Hash & GLYPH_SLOT_MASK;
	glyph_slot* Slot = Font->GlyphSlots + SlotIndex;

	glyph* Glyph = NULL;
	for (glyph* HashGlyph = Slot->First; HashGlyph; HashGlyph = HashGlyph->Next) {
		if (HashGlyph->Hash == Hash) {
			Glyph = HashGlyph;
			break;
		}
	}

	if (!Glyph) {
		f32 Scale = stbtt_ScaleForPixelHeight(&Font->FontInfo, Size);

		int Width, Height, XOffset, YOffset;
		u8* Bitmap = stbtt_GetGlyphBitmap(&Font->FontInfo, Scale, Scale, GlyphIndex, &Width, &Height, &XOffset, &YOffset);
	
		gfx_texture_id Texture = {};
		if (Bitmap) {
			arena* Scratch = Scratch_Get();

			//Convert to rgba format
			const u8* SrcBitmap = Bitmap;
			u8* DstBitmap = (u8*)Arena_Push(Scratch, Width * Height * sizeof(u32));

			const u8* SrcAt = SrcBitmap;
			u8* DstAt = DstBitmap;
			for (int y = 0; y < Height; y++) {
				for (int x = 0; x < Width; x++) {
					*DstAt++ = *SrcAt;
					*DstAt++ = *SrcAt;
					*DstAt++ = *SrcAt;
					*DstAt++ = *SrcAt++;
				}
			}

			buffer Texels = Make_Buffer(DstBitmap, Width * Height * sizeof(u32));
			Texture = Create_GFX_Texture( {
				.Dim = V2i(Width, Height),
				.Format = GDI_FORMAT_R8G8B8A8_SRGB,
				.Texels = &Texels
			});

			stbtt_FreeBitmap(Bitmap, NULL);
			Scratch_Release();
		}

		int Advance;
		stbtt_GetGlyphHMetrics(&Font->FontInfo, GlyphIndex, &Advance, NULL);

		Glyph = Arena_Push_Struct(Font->Arena, glyph);
		Glyph->Hash = Hash;
		Glyph->Dim = V2_From_V2i(V2i(Width, Height));
		Glyph->Offset = V2_From_V2i(V2i(XOffset, YOffset));
		Glyph->Advance = (f32)Advance * Scale;
		Glyph->Texture = Texture;

		DLL_Push_Back(Slot->First, Slot->Last, Glyph);
	}

	return Glyph;
}

function glyph* Font_Get_Glyph(font* Font, u32 Codepoint, f32 Size) {
	u32 GlyphIndex = Font_Get_Glyph_Index(Font, Codepoint);
	return Font_Get_Glyph_From_Index(Font, GlyphIndex, Size);
}

function f32 Font_Get_Ascent(font* Font, f32 Size) {
	f32 Scale = stbtt_ScaleForPixelHeight(&Font->FontInfo, Size);
	return Scale * Font->Ascent;
}

function f32 Font_Get_Line_Height(font* Font, f32 Size) {
	f32 Scale = stbtt_ScaleForPixelHeight(&Font->FontInfo, Size);
	return Scale * Font->LineGap;
}

function f32 Font_Get_Kerning_From_Index(font* Font, u32 GlyphIndexA, u32 GlyphIndexB, f32 Size) {
	f32 Scale = stbtt_ScaleForPixelHeight(&Font->FontInfo, Size);
	int Kerning = stbtt_GetGlyphKernAdvance(&Font->FontInfo, GlyphIndexA, GlyphIndexB);
	return (f32)Kerning * Scale;
}

function f32 Font_Get_Kerning(font* Font, u32 CodepointA, u32 CodepointB, f32 Size) {
	u32 GlyphIndexA = Font_Get_Glyph_Index(Font, CodepointA);
	u32 GlyphIndexB = Font_Get_Glyph_Index(Font, CodepointB);
	return Font_Get_Kerning_From_Index(Font, GlyphIndexA, GlyphIndexB, Size);
}

#define STB_TRUETYPE_IMPLEMENTATION
#define STBTT_malloc(x, u) Allocator_Allocate_Memory(Default_Allocator_Get(), x)
#define STBTT_free(x, u) Allocator_Free_Memory(Default_Allocator_Get(), x)
#include <stb_truetype.h>