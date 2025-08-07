#ifndef SHADER_DATA_H
#define SHADER_DATA_H

#define MAX_BINDLESS_TEXTURES (4096)
#define MAX_BINDLESS_SAMPLERS (16)
#define MAX_ENTITY_DATA (4096)

struct basic_shader_data {
	m4 ModelToClip;
	v4 C;
};

struct entity_shader_data {
	m4 WorldToClip;
};

struct entity_data {
	m4_affine ModelToWorld;
	m4_affine NormalModelToWorld;

	//x determines whether this is a texture or not. If it is then y stores 
	//the texture index, otherwise yzw stores the diffuse value
	v4 DiffusePacking; 

	//x is the sampler index, yzw are unused
	v4 FlagsPacking;
};

struct entity_draw_data {
	s32 EntityIndex;
};

struct ui_draw_data {
	s32 TextureIndex;
	s32 SamplerIndex;
};

struct ui_shader_data {
	v2 InvResolution;
};

struct linearize_depth_draw_data {
	s32 TextureIndex;
	s32 SamplerIndex;
	f32 ZNear;
	f32 ZFar;
};

#endif