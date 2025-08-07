@echo off

set build_debug=1
set build_clang=0
set build_shaders=1
set use_jolt=1

set build_path=%~dp0
set base_path=%build_path%\..
set code_path=%base_path%\code
set shader_path=%code_path%\shaders
set bin_path=%base_path%\bin
set data_path=%base_path%\data

if %build_shaders% == 0 (
	goto skip_shaders
)

set dxc_path=%VULKAN_SDK%\bin\dxc.exe
if not exist %data_path%\shaders (
	mkdir %data_path%\shaders
)

set hlsl_optimized_flags=-O3
if %build_debug% == 1 (
	set hlsl_optimized_flags=-Od
)
set hlsl_flags=-spirv -Zi -Qembed_debug %hlsl_optimized_flags%

set hlsl_vtx_flags=-T vs_6_0 -E VS_Main -fvk-invert-y %hlsl_flags%
set hlsl_pxl_flags=-T ps_6_0 -E PS_Main %hlsl_flags%

pushd %data_path%\shaders
	%dxc_path% %hlsl_vtx_flags% %shader_path%\basic_shader.hlsl -Fo basic_shader_vtx.shader
	%dxc_path% %hlsl_pxl_flags% %shader_path%\basic_shader.hlsl -Fo basic_shader_pxl.shader

	%dxc_path% %hlsl_vtx_flags% %shader_path%\entity.hlsl -Fo entity_vtx.shader
	%dxc_path% %hlsl_pxl_flags% %shader_path%\entity.hlsl -Fo entity_pxl.shader

	%dxc_path% %hlsl_vtx_flags% %shader_path%\ui.hlsl -Fo ui_vtx.shader
	%dxc_path% %hlsl_pxl_flags% %shader_path%\ui.hlsl -Fo ui_pxl.shader

	%dxc_path% %hlsl_vtx_flags% %shader_path%\linearize_depth.hlsl -Fo linearize_depth_vtx.shader
	%dxc_path% %hlsl_pxl_flags% %shader_path%\linearize_depth.hlsl -Fo linearize_depth_pxl.shader
popd

:skip_shaders

if %use_jolt% == 0 (
	goto skip_jolt
)

if exist %bin_path%\Jolt.lib (
	goto skip_jolt
)

pushd %code_path%\third_party\JoltPhysics\Build\
	cmake -S . -B bin -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release -DTARGET_UNIT_TESTS=OFF -DTARGET_HELLO_WORLD=OFF -DENABLE_OBJECT_STREAM=OFF
	cmake --build bin --config Debug
	copy bin\Debug\Jolt.lib %bin_path%\Jolt.lib
popd

:skip_jolt

set parameters=-bin_path %bin_path% -cpp
if %build_debug% == 1 (
	set parameters=%parameters% -debug
)

call %code_path%\third_party\base\build.bat %parameters%

set simulation_define=-DSIMULATION_FLOW
set simulation_include=-I%code_path%\third_party\flow
if %use_jolt% == 1 (
	set simulation_define=-DSIMULATION_JOLT -DJPH_DEBUG_RENDERER -DJPH_FLOATING_POINT_EXCEPTIONS_ENABLED -DJPH_PROFILE_ENABLED
	set simulation_include=-I%code_path%\third_party\JoltPhysics\
)

set app_includes=-I%code_path%\third_party\base\code -I%code_path%\engine -I%code_path%\shaders -I%code_path%\third_party\stb -I%code_path%\third_party\ufbx %simulation_include%
set app_defines=-DUSE_CONSOLE %simulation_define%

if %build_debug% == 1 (
	set app_defines=-DDEBUG_BUILD -DFLOW_DEBUG %app_defines%
)

echo %app_defines%

set msvc_optimized_flag=/Od /MTd
if %build_debug% == 0 (
	set msvc_optimized_flag=/O2 /MTd
)

set msvc_dll=-LD
set msvc_compile_only=/c
set msvc_warnings=/Wall /WX /wd4061 /wd4062 /wd4065 /wd4100 /wd4189 /wd4201 /wd4242 /wd4244 /wd4245 /wd4267 /wd4334 /wd4365 /wd4388 /wd4456 /wd4457 /wd4505 /wd4577 /wd4623 /wd4625 /wd4626 /wd4668 /wd4701 /wd4710 /wd4711 /wd4800 /wd4820 /wd5219 /wd5026 /wd5027 /wd5039 /wd5045 /wd5220 /wd5246 /wd5262
set msvc_flags=/nologo /FC /Z7 /EHsc %msvc_optimized_flag%
set msvc_out=/out:
set msvc_link=/link /opt:ref /incremental:no

REM enable string memory pools for MSVC
set msvc_flags=%msvc_flags% /GF

if %build_clang% == 0 (
	set obj_out=-Fo:
	set compile_only=%msvc_compile_only%
	set compile_warnings=%msvc_warnings%
	set compile_flags=%msvc_flags%
	set compile_out=%msvc_out%
	set compile_dll=%msvc_dll%
	set compile_link=%msvc_link%
	set compiler=cl /std:c++20
	set compiler_link_codegen=/LTCG
)

set engine_link_flags=%compile_link%
if %use_jolt% == 1 (
	set engine_link_flags=%engine_link_flags% %compiler_link_codegen%
)

pushd %bin_path%
	meta_program.exe -d "%code_path%\engine"
	%compiler% %compile_flags% %compile_warnings% %app_defines% %app_includes% %compile_dll% "%code_path%\engine\engine.cpp" %engine_link_flags% %compile_out%engine.dll
	%compiler% %compile_flags% %compile_warnings% %app_defines% %app_includes% "%code_path%\engine\os\win32\win32_engine.cpp" %compile_link% %compile_out%Win32_Engine.exe
popd