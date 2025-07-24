function renderer* Renderer_Get() {
	return &Get_Engine()->Renderer;
}

function gfx_mesh* Create_GFX_Mesh(editable_mesh* EditableMesh, string DebugName) {
	Assert(Editable_Mesh_Has_Position(EditableMesh));
	
	arena* Scratch = Scratch_Get();
	renderer* Renderer = Renderer_Get();
	gfx_mesh* Mesh = (gfx_mesh*)Arena_Push(Renderer->Arena, sizeof(gfx_mesh)+(EditableMesh->PartCount*sizeof(mesh_part)));

	gdi_buffer_create_info VtxBufferInfo = {
		.Size = EditableMesh->VertexCount*sizeof(v3),
		.Usage = GDI_BUFFER_USAGE_VTX_BUFFER,
		.InitialData = Make_Buffer(EditableMesh->Positions, EditableMesh->VertexCount*sizeof(v3)),
		.DebugName = String_Concat((allocator*)Scratch, DebugName, String_Lit("_VtxBuffer"))
	};

	gdi_buffer_create_info IdxBufferInfo = {
		.Size = EditableMesh->IndexCount*sizeof(u32),
		.Usage = GDI_BUFFER_USAGE_IDX_BUFFER,
		.InitialData = Make_Buffer(EditableMesh->Indices, EditableMesh->IndexCount*sizeof(u32)),
		.DebugName = String_Concat((allocator*)Scratch, DebugName, String_Lit("_IdxBuffer"))
	};
			
	Mesh->VtxBuffer = GDI_Create_Buffer(&VtxBufferInfo);
	Mesh->IdxBuffer = GDI_Create_Buffer(&IdxBufferInfo);
	Scratch_Release();

	Mesh->VtxCount = EditableMesh->VertexCount;
	Mesh->IdxCount = EditableMesh->IndexCount;
	Mesh->Parts = (mesh_part*)(Mesh + 1);
	Mesh->PartCount = EditableMesh->PartCount;
	Memory_Copy(Mesh->Parts, EditableMesh->Parts, sizeof(mesh_part)*EditableMesh->PartCount);
	return Mesh;
}

function gfx_component_id Create_GFX_Component(m4_affine Transform, v4 Color, string MeshName) {
	renderer* Renderer = Renderer_Get();
	gfx_component_id ID = Pool_Allocate(&Renderer->GfxComponents);
	gfx_component* Component = (gfx_component*)Pool_Get(&Renderer->GfxComponents, ID);
	Component->Transform = Transform;
	Component->Color = Color;

	u64 Hash = U64_Hash_String(MeshName);
	u64 SlotIndex = (Hash & MESH_SLOT_MASK);
	gfx_mesh_slot* Slot = Renderer->MeshSlots + SlotIndex;
	gfx_mesh* Mesh = NULL;
	for (gfx_mesh* HashMesh = Slot->First; HashMesh; HashMesh = HashMesh->Next) {
		if (HashMesh->Hash == Hash) {
			Mesh = HashMesh;
			break;
		}
	}

	if (!Mesh) {
		arena* Scratch = Scratch_Get();
		string FilePath = String_Format((allocator*)Scratch, "meshes/%.*s.fbx", MeshName.Size, MeshName.Ptr);
		editable_mesh* EditableMesh = Create_Editable_Mesh_From_File(FilePath);

		if (EditableMesh) {
			Mesh = Create_GFX_Mesh(EditableMesh, MeshName);
			Mesh->Hash = Hash;
			DLL_Push_Back(Slot->First, Slot->Last, Mesh);
			Delete_Editable_Mesh(EditableMesh);
		}
		Scratch_Release();
	}

	Component->Mesh = Mesh;

	return ID;
}

function gfx_texture_id Create_GFX_Texture(const gfx_texture_create_info* CreateInfo) {
	arena* Scratch = Scratch_Get();
	
	renderer* Renderer = Renderer_Get();

	gfx_texture_id ID = Pool_Allocate(&Renderer->GfxTextures);
	gfx_texture* Texture = (gfx_texture*)Pool_Get(&Renderer->GfxTextures, ID);
	gdi_handle Sampler = GDI_Is_Null(CreateInfo->Sampler) ? Renderer->DefaultSampler : CreateInfo->Sampler;

	gdi_texture_create_info TextureInfo = {
		.Format = CreateInfo->Format,
		.Dim = CreateInfo->Dim,
		.Usage = GDI_TEXTURE_USAGE_SAMPLED,
		.MipCount = 1,
		.InitialData = CreateInfo->Texels,
		.DebugName = CreateInfo->DebugName
	};

	Texture->Texture = GDI_Create_Texture(&TextureInfo);
	if (!GDI_Is_Null(Texture->Texture)) {
		gdi_texture_view_create_info TextureViewInfo = {
			.Texture = Texture->Texture,
			.Format = CreateInfo->IsSRGB ? GDI_Get_SRGB_Format(CreateInfo->Format) : CreateInfo->Format,
			.DebugName = String_Is_Empty(CreateInfo->DebugName) ? String_Empty() : String_Concat((allocator*)Scratch, CreateInfo->DebugName, String_Lit(" View"))
		};
		Texture->View = GDI_Create_Texture_View(&TextureViewInfo);
		if (!GDI_Is_Null(Texture->View)) {
			gdi_bind_group_create_info BindGroupInfo = {
				.Layout = Renderer->TextureBindGroupLayout,
				.TextureViews = { .Ptr = &Texture->View, .Count = 1 },
				.Samplers = { .Ptr = &Sampler, .Count = 1 },
				.DebugName = String_Is_Empty(CreateInfo->DebugName) ? String_Empty() : String_Concat((allocator*)Scratch, CreateInfo->DebugName, String_Lit(" Bind Group"))
			};

			Texture->BindGroup = GDI_Create_Bind_Group(&BindGroupInfo);
			if (!GDI_Is_Null(Texture->BindGroup)) {
				Scratch_Release();
				return ID;
			}

			GDI_Delete_Texture_View(Texture->View);
		}

		GDI_Delete_Texture(Texture->Texture);
	}

	Pool_Free(&Renderer->GfxTextures, ID);
	Scratch_Release();

	return Empty_Pool_ID();
}

function inline gfx_texture* Get_GFX_Texture(gfx_texture_id ID) {
	renderer* Renderer = Renderer_Get();
	gfx_texture* Result = (gfx_texture*)Pool_Get(&Renderer->GfxTextures, ID);
	return Result;
}

function void Draw_Text(gdi_render_pass* RenderPass, font* Font, v2 PixelP, f32 Size, v4 Color, string Text) {
	const char* UTF8At = Text.Ptr;
	const char* UTF8End = Text.Ptr + Text.Size;
	f32 Ascent = Font_Get_Ascent(Font, Size);

	//Premultiplied alpha
	Color.xyz = V3_Mul_S(Color.xyz, Color.w);

	u32 LastCodepoint = (u32)-1;

	for (;;) {
		if (UTF8At >= UTF8End) {
			Assert(UTF8At == UTF8End);
			return;
		}

		u32 Length;
		u32 Codepoint = UTF8_Read(UTF8At, &Length);
		UTF8At += Length;

		if (LastCodepoint != (u32)-1) {
			PixelP.x += Font_Get_Kerning(Font, LastCodepoint, Codepoint, Size);
		}

		glyph* Glyph = Font_Get_Glyph(Font, Codepoint, Size);
		
		gfx_texture* Texture = Get_GFX_Texture(Glyph->Texture);
		if (Texture) {
			v2 P = V2(PixelP.x+Glyph->Offset.x, PixelP.y+Ascent+Glyph->Offset.y);
			Render_Set_Bind_Group(RenderPass, 0, Texture->BindGroup);
			IM_Push_Rect2D_Color_UV_Norm(P, V2_Add_V2(P, Glyph->Dim), Color);
			IM_Flush(RenderPass);
		}

		PixelP.x += Glyph->Advance;
		LastCodepoint = Codepoint;
	}
}

function void Draw_Text_Formatted(gdi_render_pass* RenderPass, font* Font, v2 P, f32 Size, v4 Color, const char* Format, ...) {
	arena* Scratch = Scratch_Get();

	va_list List;
	va_start(List, Format);
	string Text = String_FormatV((allocator*)Scratch, Format, List);
	va_end(List);

	Draw_Text(RenderPass, Font, P, Size, Color, Text);
	Scratch_Release();
}

function void Draw_UI_Box(gdi_render_pass* RenderPass, ui* UI, ui_box* Box) {
	Render_Set_Bind_Group(RenderPass, 0, Renderer_Get()->WhiteTexture->BindGroup);

	v4 BackgroundColor = Box->BackgroundColor;
	if (Box->CurrentState & UI_BOX_STATE_HOVERING) {
		v3 HSV = RGB_To_HSV(BackgroundColor.xyz);
		HSV.y *= 0.5f;
		BackgroundColor.xyz = HSV_To_RGB(HSV);
	}

	BackgroundColor.xyz = V3_Mul_S(BackgroundColor.xyz, BackgroundColor.w);

	IM_Push_Rect2D_Color_UV_Norm(Box->Rect.p0, Box->Rect.p1, BackgroundColor);
	IM_Flush(RenderPass);

	if (Box->Flags & UI_BOX_FLAG_DRAW_TEXT) {
		string DisplayString = Box->DisplayString;
		Draw_Text(RenderPass, Box->Font.Font, Box->Rect.p0, Box->Font.Size, 
				  Box->TextColor, Box->DisplayString);
	}

	for (ui_box* Child = Box->FirstChild; Child; Child = Child->NextSibling) {
		Draw_UI_Box(RenderPass, UI, Child);
	}
}

function void Draw_UI(gdi_render_pass* RenderPass, ui* UI) {
	Draw_UI_Box(RenderPass, UI, UI->Root);
}

function void Draw_Mesh(gdi_render_pass* RenderPass, gfx_mesh* Mesh) {
	Render_Set_Vtx_Buffer(RenderPass, 0, Mesh->VtxBuffer);
	Render_Set_Idx_Buffer(RenderPass, Mesh->IdxBuffer, GDI_IDX_FORMAT_32_BIT);
	Render_Draw_Idx(RenderPass, Mesh->IdxCount, 0, 0);
}

function b32 Renderer_Init(renderer* Renderer) {
	Renderer->Arena = Arena_Create();
	Renderer->GfxComponents = Pool_Init(sizeof(gfx_component));
	Renderer->GfxTextures = Pool_Init(sizeof(gfx_texture));

	{
		gdi_sampler_create_info SamplerInfo = {
			.Filter = GDI_FILTER_LINEAR,
			.AddressModeU = GDI_ADDRESS_MODE_CLAMP,
			.AddressModeV = GDI_ADDRESS_MODE_CLAMP,
			.DebugName = String_Lit("Default Sampler")
		};
		Renderer->DefaultSampler = GDI_Create_Sampler(&SamplerInfo);
		if (GDI_Is_Null(Renderer->DefaultSampler)) return false;
	}

	{
		gdi_bind_group_binding Bindings[] = {
			{ .Type = GDI_BIND_GROUP_TYPE_TEXTURE, .Count = 1 },
			{ .Type = GDI_BIND_GROUP_TYPE_SAMPLER, .Count = 1 }
		};

		gdi_bind_group_layout_create_info BindGroupLayoutInfo = {
			.Bindings = { .Ptr = Bindings, .Count = Array_Count(Bindings) },
			.DebugName = String_Lit("Texture Bind Group Layout")
		};

		Renderer->TextureBindGroupLayout = GDI_Create_Bind_Group_Layout(&BindGroupLayoutInfo);
		if (GDI_Is_Null(Renderer->TextureBindGroupLayout)) return false;
	}

	{
		arena* Scratch = Scratch_Get();
		buffer VSCode = Read_Entire_File((allocator*)Scratch, String_Lit("shaders/basic_shader_vtx.shader"));
		buffer PSCode = Read_Entire_File((allocator*)Scratch, String_Lit("shaders/basic_shader_pxl.shader"));

		gdi_vtx_attribute Attributes[] = {
			{ .Semantic = String_Lit("POSITION"), .Format = GDI_FORMAT_R32G32B32_FLOAT }
		};

		gdi_vtx_binding VtxBindings[] = {
			{ .Stride = sizeof(v3), .Attributes = { .Ptr = Attributes, .Count = Array_Count(Attributes) } }
		};

		gdi_shader_create_info ShaderCreateInfo = {
			.VS = VSCode,
			.PS = PSCode,
			.PushConstantCount = sizeof(basic_shader_data)/sizeof(u32),
			.VtxBindings = { .Ptr = VtxBindings, .Count = Array_Count(VtxBindings) },
			.RenderTargetFormats = { GDI_Get_View_Format() },
			.DepthState = {
				.TestEnabled = true,
				.WriteEnabled = true,
				.CompareFunc = GDI_COMPARE_FUNC_LEQUAL,
				.DepthFormat = GDI_FORMAT_D32_FLOAT
			},
			.DebugName = String_Lit("Basic Shader")
		};

		Renderer->BasicShader = GDI_Create_Shader(&ShaderCreateInfo);
		if (GDI_Is_Null(Renderer->BasicShader)) return false;

		ShaderCreateInfo.Primitive = GDI_PRIMITIVE_LINE;
		ShaderCreateInfo.DebugName = String_Lit("Basic Line Shader");
		Renderer->BasicLineShader = GDI_Create_Shader(&ShaderCreateInfo);

		Scratch_Release();
	}

	{
		arena* Scratch = Scratch_Get();

		buffer VSCode = Read_Entire_File((allocator*)Scratch, String_Lit("shaders/ui_vtx.shader"));
		buffer PSCode = Read_Entire_File((allocator*)Scratch, String_Lit("shaders/ui_pxl.shader"));

		gdi_vtx_attribute Attributes[] = {
			{ .Semantic = String_Lit("POSITION"), .Format = GDI_FORMAT_R32G32_FLOAT },
			{ .Semantic = String_Lit("TEXCOORD"), .Format = GDI_FORMAT_R32G32_FLOAT },
			{ .Semantic = String_Lit("COLOR"), .Format = GDI_FORMAT_R32G32B32A32_FLOAT }
		};

		gdi_vtx_binding VtxBindings[] = {
			{ .Stride = sizeof(im_vtx_v2_uv2_c), .Attributes = { .Ptr = Attributes, .Count = Array_Count(Attributes) } }
		};

		gdi_blend_state BlendStates[] = {
			{ .SrcFactor = GDI_BLEND_ONE, .DstFactor = GDI_BLEND_INV_SRC_ALPHA }
		};

		gdi_shader_create_info ShaderCreateInfo = {
			.VS = VSCode,
			.PS = PSCode,
			.BindGroupLayouts = { .Ptr = &Renderer->TextureBindGroupLayout, .Count = 1 },
			.PushConstantCount = sizeof(ui_shader_data)/sizeof(u32),
			.VtxBindings = { .Ptr = VtxBindings, .Count = Array_Count(VtxBindings) },
			.RenderTargetFormats = { GDI_Get_View_Format() },
			.BlendStates = { .Ptr = BlendStates, .Count = Array_Count(BlendStates) },
			.DebugName = String_Lit("UI Shader")
		};

		Renderer->UIShader = GDI_Create_Shader(&ShaderCreateInfo);
		if (GDI_Is_Null(Renderer->UIShader)) return false;

		Scratch_Release();
	}

	{
		u32 TexelData[] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
		buffer Texels = Make_Buffer(TexelData, sizeof(TexelData));
		gfx_texture_create_info WhiteTextureInfo = {
			.Dim = V2i(2, 2),
			.Format = GDI_FORMAT_R8G8B8A8_SRGB,
			.Texels = &Texels,
			.IsSRGB = true,
			.DebugName = String_Lit("Default White Texture")
		};
		gfx_texture_id TextureID = Create_GFX_Texture(&WhiteTextureInfo);
		Renderer->WhiteTexture = Get_GFX_Texture(TextureID);
	}

	return true;
}

function b32 Render_Frame(renderer* Renderer, camera* CameraView) {
	v2 ViewDim = V2_From_V2i(GDI_Get_View_Dim());
	if (ViewDim.x != Renderer->LastDim.x || ViewDim.y != Renderer->LastDim.y) {
		if (!GDI_Is_Null(Renderer->DepthBuffer)) {
			GDI_Delete_Texture_View(Renderer->DepthView);
			GDI_Delete_Texture(Renderer->DepthBuffer);
		}

		gdi_texture_create_info DepthBufferInfo = {
			.Format = GDI_FORMAT_D32_FLOAT,
			.Dim = GDI_Get_View_Dim(),
			.Usage = GDI_TEXTURE_USAGE_DEPTH,
			.MipCount = 1,
			.DebugName = String_Lit("Depth Buffer")
		};

		Renderer->DepthBuffer = GDI_Create_Texture(&DepthBufferInfo);
		if (GDI_Is_Null(Renderer->DepthBuffer)) return false;

		gdi_texture_view_create_info DepthViewInfo = {
			.Texture = Renderer->DepthBuffer,
			.DebugName = String_Lit("Depth Buffer View")
		};
		Renderer->DepthView = GDI_Create_Texture_View(&DepthViewInfo);
		if (GDI_Is_Null(Renderer->DepthView)) return false;

		Renderer->LastDim = ViewDim;
	}

	{
		m4_affine WorldToView = Camera_Get_View(CameraView);
		m4 ViewToClip = M4_Perspective(To_Radians(60.0f), ViewDim.x / ViewDim.y, 0.01f, 100.0f);

		m4 WorldToClip = M4_Affine_Mul_M4(&WorldToView, &ViewToClip);

		gdi_render_pass_begin_info RenderPassInfo = {
			.RenderTargetViews = { GDI_Get_View() },
			.DepthBufferView = Renderer->DepthView,
			.ClearState = {
				.ShouldClear = true,
				.ClearColor = { { .F32 = { 0, 0, 0, 1 } }  },
				.ClearDepth = 1.0f
			}
		};
		gdi_render_pass* RenderPass = GDI_Begin_Render_Pass(&RenderPassInfo);

		Render_Set_Shader(RenderPass, Renderer->BasicShader);

		for (pool_iter Iter = Pool_Begin_Iter(&Renderer->GfxComponents); Iter.IsValid; Pool_Iter_Next(&Iter)) {
			gfx_component* Component = (gfx_component*)Iter.Data;

			m4 ModelToClip = M4_Affine_Mul_M4(&Component->Transform, &WorldToClip);

			basic_shader_data ShaderData = {
				.ModelToClip = ModelToClip,
				.C = Component->Color
			};

			Render_Set_Push_Constants(RenderPass, &ShaderData, sizeof(ShaderData));
			Draw_Mesh(RenderPass, Component->Mesh);
		}

		GDI_End_Render_Pass(RenderPass);
		GDI_Submit_Render_Pass(RenderPass);
	}

	{
		gdi_render_pass_begin_info RenderPassInfo = { .RenderTargetViews = { GDI_Get_View() } };
		gdi_render_pass* RenderPass = GDI_Begin_Render_Pass(&RenderPassInfo);

		ui_shader_data ShaderData = {
			.InvResolution = V2(1.0f / ViewDim.x, 1.0f / ViewDim.y)
		};

		Render_Set_Shader(RenderPass, Renderer->UIShader);
		Render_Set_Push_Constants(RenderPass, &ShaderData, sizeof(ui_shader_data));

		Draw_UI(RenderPass, UI_Get());

		IM_Flush(RenderPass);
		GDI_End_Render_Pass(RenderPass);
		GDI_Submit_Render_Pass(RenderPass);
	}

	gdi_render_params RenderParams = {};
	GDI_Render(&RenderParams);
	
	return true;
}