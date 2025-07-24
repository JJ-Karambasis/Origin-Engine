#include "shader_include.h"

struct vs_input {
	v3 P : POSITION0;
};

struct vs_output {
	v4 P : SV_POSITION;
};

[[vk::push_constant]]
ConstantBuffer<basic_shader_data> ShaderData : register(b0, space0);

vs_output VS_Main(vs_input Input) {
	vs_output Result = (vs_output)0;
	Result.P = mul(ShaderData.ModelToClip, v4(Input.P, 1.0f));
	return Result;
}

v4 PS_Main(vs_output Pxl) : SV_TARGET0 {
	return ShaderData.C;
}