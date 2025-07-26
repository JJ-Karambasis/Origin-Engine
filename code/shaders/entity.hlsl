#include "shader_include.h"

struct vs_input {
	v3 P  : POSITION0;
	v3 N  : NORMAL0;
	v2 UV : TEXCOORD0;
};

struct vs_output {
	v4 P  : SV_POSITION;
	v3 N  : NORMAL0;
	v2 UV : TEXCOORD0;
};

ConstantBuffer<entity_shader_data> ShaderData : register(b0, space0);
ConstantBuffer<entity_material_data> MaterialData[] : register(b0, space1);

Texture2D Textures[] : register(t0, space2);
SamplerState Samplers[] : register(s1, space2);

[[vk::push_constant]]
ConstantBuffer<entity_draw_data> DrawData;

vs_output VS_Main(vs_input Input) {
	vs_output Result = (vs_output)0;
	v3 WorldP = mul(v4(Input.P, 1.0f), DrawData.ModelToWorld);
	Result.N = mul(Input.N, (m3)DrawData.NormalModelToWorld);
	Result.P = mul(v4(WorldP, 1.0f), ShaderData.WorldToClip);
	Result.UV = Input.UV;
	return Result;
}

v4 PS_Main(vs_output Pxl) : SV_TARGET0 {
	entity_material_data Material = MaterialData[DrawData.MaterialIndex];
	int SamplerIndex = asuint(Material.FlagsPacking.x);
	
	int DiffuseIsTexture = asuint(Material.DiffusePacking.x);
	
	v3 Diffuse = v3(0, 0, 0);
	if (DiffuseIsTexture) {
		int DiffuseIndex = asuint(Material.DiffusePacking.y);
		Diffuse = Textures[DiffuseIndex].Sample(Samplers[SamplerIndex], Pxl.UV).rgb;
	} else {
		Diffuse = Material.DiffusePacking.yzw;
	}

	return v4(Diffuse, 1.0f);
}