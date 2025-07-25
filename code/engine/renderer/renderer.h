#ifndef RENDERER_H
#define RENDERER_H

#include <gdi/gdi.h>
#include <shader_data.h>

struct gfx_mesh;

struct gfx_component {
	m4_affine Transform;
	v4 		  Color;
	gfx_mesh* Mesh;
};
typedef pool_id gfx_component_id;

struct gfx_component_create_info {
	m4_affine Transform;
	v4 Color;
	string MeshName;
};

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

struct gfx_mesh {
	u64 	   Hash;
	gdi_handle VtxBuffer;
	gdi_handle IdxBuffer;
	u32 	   VtxCount;
	u32 	   IdxCount;
	u32 	   PartCount;
	mesh_part* Parts;
	gfx_mesh*  Next;
	gfx_mesh*  Prev;
};

struct gfx_mesh_slot {
	gfx_mesh* First;
	gfx_mesh* Last;
};

struct shader_ui_box {
	v2 P0;
	v2 P1;
	v4 C;
};

#define MAX_MESH_SLOT_COUNT (4096)
#define MESH_SLOT_MASK (MAX_MESH_SLOT_COUNT-1)
struct renderer {
	arena* Arena;
	gdi* GDI;
	pool GfxComponents;
	pool GfxTextures;
	gfx_mesh_slot MeshSlots[MAX_MESH_SLOT_COUNT];
	gdi_handle DefaultSampler;
	gdi_handle BasicShader;
	gdi_handle BasicLineShader;
	gdi_handle UIShader;

	gdi_handle DepthBuffer;
	gdi_handle DepthView;

	gdi_handle TextureBindGroupLayout;
	gfx_texture* WhiteTexture;

	v2 LastDim;
};

function gfx_component_id Create_GFX_Component(const gfx_component_create_info& CreateInfo);
function gfx_texture_id Create_GFX_Texture(const gfx_texture_create_info* CreateInfo);

#endif