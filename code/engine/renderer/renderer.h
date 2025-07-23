#ifndef RENDERER_H
#define RENDERER_H

#include <gdi/gdi.h>

struct gfx_component {
	m4_affine Transform;
	v4 		  Color;
};

typedef pool_id gfx_component_id;

struct gfx_texture {
	gdi_handle Texture;
	gdi_handle View;
	gdi_handle BindGroup;
};

typedef pool_id gfx_texture_id;

struct gfx_texture_create_info {
	v2i        Dim;
	gdi_format Format;
	buffer*    Texels;
	gdi_handle Sampler;
	b32 	   IsSRGB;
	string     DebugName;
};

struct shader_ui_box {
	v2 P0;
	v2 P1;
	v4 C;
};

struct renderer {
	gdi* GDI;
	pool GfxComponents;
	pool GfxTextures;
	gdi_handle DefaultSampler;
	gdi_handle BasicShader;
	gdi_handle UIShader;

	gdi_handle DepthBuffer;
	gdi_handle DepthView;

	gdi_handle TextureBindGroupLayout;
	gfx_texture* WhiteTexture;

	v2 LastDim;
};

function gfx_texture_id Create_GFX_Texture(const gfx_texture_create_info* CreateInfo);

#endif