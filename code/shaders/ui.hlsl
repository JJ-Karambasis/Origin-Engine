#include "shader_include.h"

struct vs_input {
	v2 P  : POSITION0;
	v2 UV : TEXCOORD0;
	v4 C  : COLOR0;
};

struct vs_output {
	v4 P  : SV_POSITION;
	v2 UV : TEXCOORD0;
	v4 C  : COLOR0;
};

[[vk::push_constant]]
ConstantBuffer<ui_shader_data> ShaderData : register(b0, space0);

Texture2D Texture : register(t0, space0);
SamplerState Sampler : register(s1, space0);

vs_output VS_Main(vs_input Input) {
	vs_output Result = (vs_output)0;
	Result.P = v4((2.0f*Input.P.x*ShaderData.InvResolution.x)-1.0f,
				  1.0f-(2.0f*Input.P.y*ShaderData.InvResolution.y), 
				  0.0f, 1.0f);
	Result.UV = Input.UV;
	Result.C = Input.C;
	return Result;
}

v4 PS_Main(vs_output Pxl) : SV_TARGET0 {
	v4 Color = Texture.Sample(Sampler, Pxl.UV)*Pxl.C;
	return Color;
}