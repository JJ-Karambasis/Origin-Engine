#ifndef RENDERER_H
#define RENDERER_H

#include <gdi/gdi.h>
#include <shader_data.h>

#define shader_sizeof(x) Align(sizeof(x), GDI_Constant_Buffer_Alignment())

struct vtx_p3_n3_uv2 {
	v3 P;
	v3 N;
	v2 UV;
};

typedef slot_id gfx_texture_id;
struct gfx_texture {
	gfx_texture_id ID;
	u64 		   Hash;
	gdi_handle     Texture;
	gdi_handle     View;

	gfx_texture* Next;
	gfx_texture* Prev;
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
	gdi_handle VtxBuffers[2];
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

struct material_channel_v4 {
	b32 		   IsTexture;
	v4  		   Value;
	gfx_texture_id Texture;
};

struct material {
	material_channel_v4 Diffuse;
};
Array_Define(material);

struct material_channel_info_v4 {
	b32    IsTexture;
	v4 	   Value;
	string TextureName;
};

struct material_info {
	material_channel_info_v4 Diffuse;
};

struct gfx_component {
	m4_affine Transform;
	gfx_mesh* Mesh;
	material  Material;
};
typedef pool_id gfx_component_id;

struct gfx_component_create_info {
	m4_affine 	  Transform;
	v4 			  Color;
	string 		  MeshName;
	material_info Material;
};

struct shader_ui_box {
	v2 P0;
	v2 P1;
	v4 C;
};

struct shader_data {
	gdi_handle Buffer;
	gdi_handle BindGroupLayout;
	gdi_handle BindGroup;
};

#define MAX_TEXTURE_SLOT_COUNT (4096)
#define TEXTURE_SLOT_MASK (MAX_TEXTURE_SLOT_COUNT-1)
struct texture_slot {
	gfx_texture* First;
	gfx_texture* Last;
};

struct texture_manager {
	slot_map SamplerSlotMap;
	gfx_sampler Samplers[MAX_BINDLESS_SAMPLERS];

	slot_map 	   TextureSlotMap;
	gfx_texture    Textures[MAX_BINDLESS_TEXTURES];
	gdi_handle     BindlessTextureBindGroupLayout;
	gdi_handle     BindlessTextureBindGroup;

	texture_slot TextureHashSlots[MAX_TEXTURE_SLOT_COUNT];
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

#define MAX_SHADER_SLOT_COUNT (4096)
#define SHADER_SLOT_MASK (MAX_SHADER_SLOT_COUNT-1)
struct shader_manager {
	shader_slot ShaderSlots[MAX_SHADER_SLOT_COUNT];
	gdi_handle SingleShaderDataBindGroupLayout;

	//Basic
	gdi_handle BasicShader;
	gdi_handle BasicLineShader;

	//Entity
	shader_data EntityShaderData;
	shader_data EntityData;
	gdi_handle EntityShader;

	//UI
	gdi_handle UIShader;
	shader_data UIShaderData;
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
	
	gdi_handle DepthBuffer;
	gdi_handle DepthView;

	gfx_sampler_id DefaultSampler;
	gfx_texture_id WhiteTexture;

	v2 LastDim;
};

function gfx_component_id Create_GFX_Component(const gfx_component_create_info& CreateInfo);
function gfx_texture_id Create_GFX_Texture(const gfx_texture_create_info& CreateInfo);

#endif