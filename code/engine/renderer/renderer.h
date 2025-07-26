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

typedef slot_id gfx_texture_id;
struct gfx_texture {
	gfx_texture_id ID;
	gdi_handle     Texture;
	gdi_handle     View;
};

struct gfx_texture_create_info {
	v2i        Dim;
	gdi_format Format;
	buffer*    Texels;
	string     DebugName;
};

typedef slot_id gfx_sampler_id;
struct gfx_sampler {
	gfx_sampler_id ID;
	gdi_handle 	   Sampler;
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

struct shader_data {
	gdi_handle Buffer;
	gdi_handle BindGroup;
};

struct texture_manager {
	slot_map SamplerSlotMap;
	gfx_sampler Samplers[MAX_BINDLESS_SAMPLERS];

	slot_map 	   TextureSlotMap;
	gfx_texture    Textures[MAX_BINDLESS_TEXTURES];
	gdi_handle     BindlessTextureBindGroupLayout;
	gdi_handle     BindlessTextureBindGroup;
};

struct shader {
	u64 		   Hash;
	os_hot_reload* HotReload;
	buffer  	   Code;
	b32 		   Reload;
	shader* 	   Prev;
	shader* 	   Next;
};

struct shader_slot {
	shader* First;
	shader* Last;
};

#define MAX_SHADER_SLOT_COUNT 4096
#define SHADER_SLOT_MASK (MAX_SHADER_SLOT_COUNT-1)
struct shader_manager {
	shader_slot ShaderSlots[MAX_SHADER_SLOT_COUNT];

	//Basic
	gdi_handle BasicShader;
	gdi_handle BasicLineShader;

	//UI
	gdi_handle UIShader;
};

#define MAX_MESH_SLOT_COUNT (4096)
#define MESH_SLOT_MASK (MAX_MESH_SLOT_COUNT - 1)
struct renderer {
	arena* Arena;
	gdi* GDI;
	pool GfxComponents;
	gfx_mesh_slot MeshSlots[MAX_MESH_SLOT_COUNT];
	texture_manager TextureManager;
	shader_manager ShaderManager;
	
	gdi_handle ShaderDataBindGroupLayout;

	shader_data UIShaderData;

	gdi_handle DepthBuffer;
	gdi_handle DepthView;

	gfx_sampler_id DefaultSampler;
	gfx_texture_id WhiteTexture;

	v2 LastDim;
};

function gfx_component_id Create_GFX_Component(const gfx_component_create_info& CreateInfo);
function gfx_texture_id Create_GFX_Texture(const gfx_texture_create_info& CreateInfo);

#endif