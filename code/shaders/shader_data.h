#ifndef SHADER_DATA_H
#define SHADER_DATA_H

#define MAX_BINDLESS_TEXTURES (4096)
#define MAX_BINDLESS_SAMPLERS (16)
#define MAX_ENTITY_DATA (4096)
#define MAX_DIR_LIGHTS 8

struct basic_shader_data {
	m4 ModelToClip;
	v4 C;
};

struct shader_dir_light {
	v4 Dir; //xyz stores direction, z stores the shadow map index (-1 if it doesn't have one)
	v4 Color; //xyz stores color, z stores the sampler index (-1 if it doesn't have one)
	m4 WorldToClipLight;
};

struct entity_shader_data {
	m4 				 WorldToClip;
	shader_dir_light DirLights[MAX_DIR_LIGHTS];
	s32 			 DirLightCount;
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

struct shadow_shader_data {
	m4 WorldToClip;
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