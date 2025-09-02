#include "shader_include.h"

struct vs_input {
	v3 P  : POSITION0;
	v3 N  : NORMAL0;
	v2 UV : TEXCOORD0;
};

struct vs_output {
	v4 P  	  : SV_POSITION;
	v3 WorldP : POSITION0;
	v3 N  	  : NORMAL0;
	v2 UV 	  : TEXCOORD0;
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

	Result.WorldP = mul(v4(Input.P, 1.0f), Entity.ModelToWorld);
	Result.N = normalize(mul(v4(Input.N, 0.0f), Entity.NormalModelToWorld));
	Result.P = mul(v4(Result.WorldP, 1.0f), ShaderData.WorldToClip);
	Result.UV = Input.UV;
	return Result;
}

f32 Calculate_Shadow(v4 LightSpaceP, f32 Bias, s32 ShadowMapIndex, s32 ShadowSamplerIndex) {
	v3 ProjCoords = LightSpaceP.xyz / LightSpaceP.w;
	ProjCoords.x = ProjCoords.x * 0.5f + 0.5f;
	ProjCoords.y = -ProjCoords.y * 0.5f + 0.5f;

	if (ProjCoords.x < 0.0f || ProjCoords.x > 1.0f ||
		ProjCoords.y < 0.0f || ProjCoords.y > 1.0f ||
		ProjCoords.z > 1.0f) {
		//No shadow
		return 0.0f;
	}

	f32 CurrentDepth = ProjCoords.z;
	f32 ShadowDepth = Textures[ShadowMapIndex].Sample(Samplers[ShadowSamplerIndex], ProjCoords.xy).r;
	return (CurrentDepth - Bias) > ShadowDepth ? 1.0f : 0.0f;
}

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

	v3 n = normalize(Pxl.N);

	v3 DirectLight = v3(0, 0, 0);

	for (s32 i = 0; i < ShaderData.DirLightCount; i++) {
		shader_dir_light DirLight = ShaderData.DirLights[i];
		
		v3 l = -normalize(DirLight.Dir.xyz);
		v3 LightValue = DirLight.Color.xyz * Diffuse * saturate(dot(n, l));

		v4 LightSpaceP = mul(v4(Pxl.WorldP, 1.0f), DirLight.WorldToClipLight);
		f32 Shadow = 0.0f;

		s32 ShadowMapIndex = asuint(DirLight.Dir.w);
		s32 ShadowSamplerIndex = asuint(DirLight.Color.w);
		if (ShadowMapIndex != -1 && ShadowSamplerIndex != -1) {
			Shadow = Calculate_Shadow(LightSpaceP, 1e-3f, ShadowMapIndex, ShadowSamplerIndex);
		}
		
		DirectLight += (1.0f - Shadow)*LightValue;
	}

	v3 AmbientLight = Diffuse * 0.03f;
	v3 FinalLight = DirectLight + AmbientLight;

	return v4(FinalLight, 1.0f);
}