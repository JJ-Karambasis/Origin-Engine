Dynamic_Array_Implement_Type(draw_primitive, Draw_Primitive);

function draw_primitives* Get_Draw_Primitives() {
	renderer* Renderer = Renderer_Get();
	return &Renderer->DrawPrimitives;
}

function void Draw_Box(draw_flags Flags, v3 CenterP, v3 HalfExtent, quat Orientation, v3 Color) {
	draw_primitives* DrawPrimitives = Get_Draw_Primitives();

	gfx_mesh* Mesh = NULL;
	if (Flags & DRAW_FLAG_LINE) {
		Mesh = DrawPrimitives->BoxLineMesh;
	}

	draw_primitive Primitive = {
		.Mesh = Mesh,
		.Transform = M4_Affine_Transform_Quat(CenterP, Orientation, HalfExtent),
		.Color = V4_From_V3(Color, 1.0f)
	};

	if (Flags & DRAW_FLAG_LINE) {
		Dynamic_Draw_Primitive_Array_Add(&DrawPrimitives->LineDraws, Primitive);
	} else {
		Dynamic_Draw_Primitive_Array_Add(&DrawPrimitives->OpaqueDraws, Primitive);
	}
}

function void Draw_Sphere(draw_flags Flags, v3 CenterP, f32 Radius, v3 Color) {
	draw_primitives* DrawPrimitives = Get_Draw_Primitives();

	gfx_mesh* Mesh = DrawPrimitives->SphereTriangleMesh;
	if (Flags & DRAW_FLAG_LINE) {
		Mesh = DrawPrimitives->SphereLineMesh;
	}

	draw_primitive Primitive = {
		.Mesh = Mesh,
		.Transform = {
			Radius, 0, 0,
			0, Radius, 0, 
			0, 0, Radius,
			CenterP.x, CenterP.y, CenterP.z
		},
		.Color = V4_From_V3(Color, 1.0f)
	};

	if (Flags & DRAW_FLAG_LINE) {
		Dynamic_Draw_Primitive_Array_Add(&DrawPrimitives->LineDraws, Primitive);
	} else {
		Dynamic_Draw_Primitive_Array_Add(&DrawPrimitives->OpaqueDraws, Primitive);
	}
}

function void Draw_Cone(draw_flags Flags, v3 P0, v3 P1, f32 Radius, v3 Color) {
	draw_primitives* DrawPrimitives = Get_Draw_Primitives();

	gfx_mesh* Mesh = DrawPrimitives->ConeTriangleMesh;
	if (Flags & DRAW_FLAG_LINE) {
		Mesh = NULL;
	}

	v3 Z = V3_Sub_V3(P1, P0);
	m3 Basis = M3_Basis(Z);
	Basis.x = V3_Mul_S(Basis.x, Radius);
	Basis.y = V3_Mul_S(Basis.y, Radius);
	Basis.z = Z;

	draw_primitive Primitive = {
		.Mesh = Mesh,
		.Transform = {
			.x = Basis.x,
			.y = Basis.y,
			.z = Basis.z,
			.t = P0
		},
		.Color = V4_From_V3(Color, 1.0f)
	};

	if (Flags & DRAW_FLAG_LINE) {
		Dynamic_Draw_Primitive_Array_Add(&DrawPrimitives->LineDraws, Primitive);
	} else {
		Dynamic_Draw_Primitive_Array_Add(&DrawPrimitives->OpaqueDraws, Primitive);
	}
}

function void Draw_Cylinder(draw_flags Flags, v3 P0, v3 P1, f32 Radius, v3 Color) {
	draw_primitives* DrawPrimitives = Get_Draw_Primitives();

	gfx_mesh* Mesh = DrawPrimitives->CylinderTriangleMesh;
	if (Flags & DRAW_FLAG_LINE) {
		Mesh = NULL;
	}

	v3 Z = V3_Sub_V3(P1, P0);
	m3 Basis = M3_Basis(Z);
	Basis.x = V3_Mul_S(Basis.x, Radius);
	Basis.y = V3_Mul_S(Basis.y, Radius);
	Basis.z = Z;

	draw_primitive Primitive = {
		.Mesh = Mesh,
		.Transform = {
			.x = Basis.x,
			.y = Basis.y,
			.z = Basis.z,
			.t = P0
		},
		.Color = V4_From_V3(Color, 1.0f)
	};

	if (Flags & DRAW_FLAG_LINE) {
		Dynamic_Draw_Primitive_Array_Add(&DrawPrimitives->LineDraws, Primitive);
	} else {
		Dynamic_Draw_Primitive_Array_Add(&DrawPrimitives->OpaqueDraws, Primitive);
	}
}

function void Draw_Capsule(draw_flags Flags, v3 P0, v3 P1, f32 Radius, v3 Color);
function void Draw_Arrow(v3 P0, v3 V, f32 Radius, v3 Color) {
	f32 LineRadius = Radius * 0.5f;
	f32 Length = V3_Mag(V);
	v3 Direction = V3_Div_S(V, Length);

	Radius = Min(Radius, Length);


	f32 LineLength = Length - Radius;
	v3 P1 = V3_Add_V3(P0, V3_Mul_S(Direction, LineLength));
	
	if (LineLength > 0) {
		Draw_Cylinder(0, P0, P1, LineRadius, Color);
	}

	v3 ArrowP0 = P1;
	Draw_Cone(0, ArrowP0, V3_Add_V3(ArrowP0, V3_Mul_S(Direction, Radius)), Radius, Color);
}

function b32 Draw_Primitives_Init(draw_primitives* DrawPrimitives) {
	editable_mesh* BoxLineMesh = Create_Box_Line_Editable_Mesh();
	editable_mesh* SphereLineMesh = Create_Sphere_Line_Editable_Mesh(60);

	editable_mesh* SphereTriangleMesh = Create_Sphere_Mesh(3);
	editable_mesh* ConeTriangleMesh = Create_Cone_Mesh(60);
	editable_mesh* CylinderTriangleMesh = Create_Cylinder_Mesh(60);

	DrawPrimitives->BoxLineMesh = Get_GFX_Mesh(Create_GFX_Mesh(BoxLineMesh, String_Lit("Box_Line")));
	DrawPrimitives->SphereLineMesh = Get_GFX_Mesh(Create_GFX_Mesh(SphereLineMesh, String_Lit("Sphere_Line")));
	
	DrawPrimitives->SphereTriangleMesh = Get_GFX_Mesh(Create_GFX_Mesh(SphereTriangleMesh, String_Lit("Sphere_Triangle")));
	DrawPrimitives->ConeTriangleMesh = Get_GFX_Mesh(Create_GFX_Mesh(ConeTriangleMesh, String_Lit("Cone_Triangle")));
	DrawPrimitives->CylinderTriangleMesh = Get_GFX_Mesh(Create_GFX_Mesh(CylinderTriangleMesh, String_Lit("Cylinder_Triangle")));

	Delete_Editable_Mesh(BoxLineMesh);
	Delete_Editable_Mesh(SphereLineMesh);
	
	Delete_Editable_Mesh(SphereTriangleMesh);
	Delete_Editable_Mesh(ConeTriangleMesh);
	Delete_Editable_Mesh(CylinderTriangleMesh);

	DrawPrimitives->OpaqueDraws = Dynamic_Draw_Primitive_Array_Init(Default_Allocator_Get());
	DrawPrimitives->LineDraws = Dynamic_Draw_Primitive_Array_Init(Default_Allocator_Get());

	return true;
}

function b32 Render_Draw_Primitives(render_context* RenderContext) {
	draw_primitives* DrawPrimitives = Get_Draw_Primitives();
	shader_manager* ShaderManager = &Renderer_Get()->ShaderManager;

	v2 ViewDim = V2_From_V2i(GDI_Get_View_Dim());
	if (ViewDim.x != DrawPrimitives->LastDim.x || ViewDim.y != DrawPrimitives->LastDim.y) {
		if (!GDI_Is_Null(DrawPrimitives->DepthBuffer)) {
			GDI_Delete_Texture_View(DrawPrimitives->DepthView);
			GDI_Delete_Texture(DrawPrimitives->DepthBuffer);
		}

		gdi_texture_create_info DepthBufferInfo = {
			.Format = GDI_FORMAT_D32_FLOAT,
			.Dim = GDI_Get_View_Dim(),
			.Usage = GDI_TEXTURE_USAGE_DEPTH,
			.MipCount = 1,
			.DebugName = String_Lit("Draw Primitives Depth Buffer")
		};

		DrawPrimitives->DepthBuffer = GDI_Create_Texture(&DepthBufferInfo);
		if (GDI_Is_Null(DrawPrimitives->DepthBuffer)) return false;

		gdi_texture_view_create_info DepthViewInfo = {
			.Texture = DrawPrimitives->DepthBuffer,
			.DebugName = String_Lit("Draw Primitives Depth Buffer View")
		};
		DrawPrimitives->DepthView = GDI_Create_Texture_View(&DepthViewInfo);
		if (GDI_Is_Null(DrawPrimitives->DepthView)) return false;

		DrawPrimitives->LastDim = ViewDim;
	}

	gdi_render_pass_begin_info RenderPassInfo = {
		.RenderTargetViews = { GDI_Get_View() },
		.DepthBufferView = { DrawPrimitives->DepthView },
		.ClearDepth = {
			.ShouldClear = true,
			.Depth = 1.0f
		}
	};
	gdi_render_pass* RenderPass = GDI_Begin_Render_Pass(&RenderPassInfo);

	//Opaque
	Render_Set_Shader(RenderPass, ShaderManager->BasicShader);
	for (size_t i = 0; i < DrawPrimitives->OpaqueDraws.Count; i++) {
		draw_primitive* Primitive = DrawPrimitives->OpaqueDraws.Ptr + i;
		Assert(Primitive->Mesh);

		basic_shader_data ShaderData = {
			.ModelToClip = M4_Affine_Mul_M4(&Primitive->Transform, &RenderContext->WorldToClip),
			.C = Primitive->Color
		};
		Render_Set_Push_Constants(RenderPass, &ShaderData, sizeof(basic_shader_data));
		Draw_Mesh(RenderPass, Primitive->Mesh);
	}

	//Lines
	Render_Set_Shader(RenderPass, ShaderManager->BasicLineNoDepthShader);
	for (size_t i = 0; i < DrawPrimitives->LineDraws.Count; i++) {
		draw_primitive* Primitive = DrawPrimitives->LineDraws.Ptr + i;
		Assert(Primitive->Mesh);

		basic_shader_data ShaderData = {
			.ModelToClip = M4_Affine_Mul_M4(&Primitive->Transform, &RenderContext->WorldToClip),
			.C = Primitive->Color
		};
		Render_Set_Push_Constants(RenderPass, &ShaderData, sizeof(basic_shader_data));
		Draw_Mesh(RenderPass, Primitive->Mesh);
	}

	GDI_End_Render_Pass(RenderPass);
	GDI_Submit_Render_Pass(RenderPass);

	Dynamic_Draw_Primitive_Array_Clear(&DrawPrimitives->OpaqueDraws);
	Dynamic_Draw_Primitive_Array_Clear(&DrawPrimitives->LineDraws);

	return true;
}