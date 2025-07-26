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
ConstantBuffer<entity_data> EntityData[MAX_ENTITY_DATA] : register(b0, space1);

Texture2D Textures[MAX_BINDLESS_TEXTURES] : register(t0, space2);
SamplerState Samplers[MAX_BINDLESS_SAMPLERS] : register(s1, space2);

[[vk::push_constant]]
ConstantBuffer<entity_draw_data> DrawData;

vs_output VS_Main(vs_input Input) {
	vs_output Result = (vs_output)0;
	entity_data Entity = EntityData[DrawData.EntityIndex];

	v3 WorldP = mul(v4(Input.P, 1.0f), Entity.ModelToWorld);
	Result.N = normalize(mul(v4(Input.N, 0.0f), Entity.NormalModelToWorld));
	Result.P = mul(v4(WorldP, 1.0f), ShaderData.WorldToClip);
	Result.UV = Input.UV;
	return Result;
}

static v3 LightColor = v3(1.0f, 1.0f, 1.0f)*3;
static v3 LightDirection = v3(0.0f, 0.0f, -1.0f);

v4 PS_Main(vs_output Pxl) : SV_TARGET0 {
	entity_data Entity = EntityData[DrawData.EntityIndex];
	int SamplerIndex = asuint(Entity.FlagsPacking.x);
	int DiffuseIsTexture = asuint(Entity.DiffusePacking.x);
	
	v3 Diffuse = v3(0, 0, 0);
	if (DiffuseIsTexture) {
		int DiffuseIndex = asuint(Entity.DiffusePacking.y);
		Diffuse = Textures[DiffuseIndex].Sample(Samplers[SamplerIndex], Pxl.UV).rgb;
	} else {
		Diffuse = Entity.DiffusePacking.yzw;
	}

	v3 l = -normalize(LightDirection);
	v3 n = normalize(Pxl.N);

	v3 DirectLight = LightColor * Diffuse * saturate(dot(n, l));
	v3 AmbientLight = Diffuse * 0.03f;

	v3 FinalLight = AmbientLight + DirectLight;

	return v4(FinalLight, 1.0f);
}