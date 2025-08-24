#include "shader_include.h"

struct vs_input {
	v3 P  : POSITION0;
};

struct vs_output {
	v4 P  : SV_POSITION;
};

ConstantBuffer<shadow_shader_data> ShaderData : register(b0, space0);
ConstantBuffer<entity_data> EntityData[MAX_ENTITY_DATA] : register(b0, space1);

[[vk::push_constant]]
ConstantBuffer<entity_draw_data> DrawData;

vs_output VS_Main(vs_input Input) {
	vs_output Result = (vs_output)0;
	entity_data Entity = EntityData[DrawData.EntityIndex];

	v3 WorldP = mul(v4(Input.P, 1.0f), Entity.ModelToWorld);
	Result.P = mul(v4(WorldP, 1.0f), ShaderData.WorldToClip);
	return Result;
}

void PS_Main(vs_output Pxl) {
}