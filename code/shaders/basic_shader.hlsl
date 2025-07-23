#include "shader_include.h"

struct vs_input {
	v3 P : POSITION0;
	v4 C : COLOR0;
};

struct vs_output {
	v4 P : SV_POSITION;
	v4 C : COLOR0;
};

vs_output VS_Main(vs_input Input) {
	vs_output Result = (vs_output)0;
	Result.P = v4(Input.P, 1.0f);
	Result.C = Input.C;
	return Result;
}

v4 PS_Main(vs_output Pxl) : SV_TARGET0 {
	return Pxl.C;
}