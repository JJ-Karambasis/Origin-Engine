#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

function renderer* Renderer_Get() {
	return &Get_Engine()->Renderer;
}

function void Draw_Mesh(gdi_render_pass* RenderPass, gfx_mesh* Mesh) {
	Render_Set_Vtx_Buffer(RenderPass, 0, Mesh->VtxBuffers[0]);
	if (!GDI_Is_Null(Mesh->VtxBuffers[1])) {
		Render_Set_Vtx_Buffer(RenderPass, 1, Mesh->VtxBuffers[1]);
	}

	Render_Set_Idx_Buffer(RenderPass, Mesh->IdxBuffer, GDI_IDX_FORMAT_32_BIT);
	Render_Draw_Idx(RenderPass, Mesh->IdxCount, 0, 0);
}

function inline gfx_mesh* Get_GFX_Mesh(gfx_mesh_id ID) {
	gfx_mesh* Result = (gfx_mesh*)Pool_Get(&Renderer_Get()->GfxMeshes, ID);
	return Result;
}

#include "draw.cpp"

function gfx_mesh_id Create_GFX_Mesh(editable_mesh* EditableMesh, string DebugName) {
	Assert(Editable_Mesh_Has_Position(EditableMesh));
	
	arena* Scratch = Scratch_Get();
	renderer* Renderer = Renderer_Get();
	gfx_mesh_id Result = Pool_Allocate(&Renderer->GfxMeshes);
	gfx_mesh* Mesh = (gfx_mesh*)Pool_Get(&Renderer->GfxMeshes, Result);

	gdi_buffer_create_info VtxBufferInfo = {
		.Size = EditableMesh->VertexCount*sizeof(v3),
		.Usage = GDI_BUFFER_USAGE_VTX_BUFFER,
		.InitialData = Make_Buffer(EditableMesh->Positions, EditableMesh->VertexCount*sizeof(v3)),
		.DebugName = String_Concat((allocator*)Scratch, DebugName, String_Lit("_VtxBuffer_Positions"))
	};

	gdi_buffer_create_info IdxBufferInfo = {
		.Size = EditableMesh->IndexCount*sizeof(u32),
		.Usage = GDI_BUFFER_USAGE_IDX_BUFFER,
		.InitialData = Make_Buffer(EditableMesh->Indices, EditableMesh->IndexCount*sizeof(u32)),
		.DebugName = String_Concat((allocator*)Scratch, DebugName, String_Lit("_IdxBuffer"))
	};
			
	Mesh->VtxBuffers[0] = GDI_Create_Buffer(&VtxBufferInfo);
	Mesh->IdxBuffer = GDI_Create_Buffer(&IdxBufferInfo);
	
	if (Editable_Mesh_Has_Attribs(EditableMesh)) {
		vtx_attrib* Attribs = Arena_Push_Array(Renderer->Arena, EditableMesh->VertexCount, vtx_attrib);
		for (u32 i = 0; i < EditableMesh->VertexCount; i++) {
			Attribs[i].Normal = EditableMesh->Normals[i];
			Attribs[i].UV = EditableMesh->UVs[i];
		}

		gdi_buffer_create_info AttribBufferInfo = {
			.Size = EditableMesh->VertexCount*sizeof(vtx_attrib),
			.Usage = GDI_BUFFER_USAGE_VTX_BUFFER,
			.InitialData = Make_Buffer(Attribs, EditableMesh->VertexCount*sizeof(vtx_attrib)),
			.DebugName = String_Concat((allocator*)Scratch, DebugName, String_Lit("_VtxBuffer_Attribs"))
		};

		Mesh->VtxBuffers[1] = GDI_Create_Buffer(&AttribBufferInfo);
	}
	
	Scratch_Release();

	Mesh->VtxCount = EditableMesh->VertexCount;
	Mesh->IdxCount = EditableMesh->IndexCount;
	return Result;
}

function inline gfx_component* Get_GFX_Component(gfx_component_id ID) {
	renderer* Renderer = Renderer_Get();
	gfx_component* Result = (gfx_component*)Pool_Get(&Renderer->GfxComponents, ID);
	return Result;
}

function void Delete_GFX_Texture(gfx_texture_id ID) {
	if (!ID.ID) return;
	
	renderer* Renderer = Renderer_Get();
	texture_manager* TextureManager = &Renderer->TextureManager;

	if (Slot_Map_Is_Allocated(&TextureManager->TextureSlotMap, ID)) {
		gfx_texture* Texture = TextureManager->Textures + ID.Index;
		GDI_Delete_Texture_View(Texture->View);
		GDI_Delete_Texture(Texture->Texture);
		Slot_Map_Free(&TextureManager->TextureSlotMap, ID);
	}
}

function gfx_texture_id Create_GFX_Texture(const gfx_texture_create_info& CreateInfo) {	
	arena* Scratch = Scratch_Get();
	
	renderer* Renderer = Renderer_Get();
	texture_manager* TextureManager = &Renderer->TextureManager;

	gfx_texture_id ID = Slot_Map_Allocate(&TextureManager->TextureSlotMap);
	gfx_texture* Texture = TextureManager->Textures + ID.Index;
	Memory_Clear(Texture, sizeof(gfx_texture));

	gdi_texture_create_info TextureInfo = {
		.Format = CreateInfo.Format,
		.Dim = CreateInfo.Dim,
		.Usage = GDI_TEXTURE_USAGE_SAMPLED,
		.MipCount = 1,
		.InitialData = CreateInfo.Texels,
		.DebugName = CreateInfo.DebugName
	};

	Texture->ID = ID;
	Texture->Texture = GDI_Create_Texture(&TextureInfo);
	if (!GDI_Is_Null(Texture->Texture)) {
		gdi_texture_view_create_info TextureViewInfo = {
			.Texture = Texture->Texture,
			.Format = CreateInfo.Format,
			.DebugName = String_Is_Empty(CreateInfo.DebugName) ? String_Empty() : String_Concat((allocator*)Scratch, CreateInfo.DebugName, String_Lit(" View"))
		};
		Texture->View = GDI_Create_Texture_View(&TextureViewInfo);
		if (!GDI_Is_Null(Texture->View)) {
			gdi_bind_group_write Write = {
				.Binding = 0,
				.Index = ID.Index,
				.TextureViews = { .Ptr = &Texture->View, .Count = 1 }
			};

			gdi_bind_group_write_info WriteInfo = {
				.Writes = { .Ptr = &Write, .Count = 1 }
			};

			GDI_Write_Bind_Group(TextureManager->BindlessTextureBindGroup, &WriteInfo);

			Scratch_Release();
			return ID;
		}
	}

	Delete_GFX_Texture(ID);
	Scratch_Release();

	return Slot_Null();
}

function inline gfx_texture* Get_GFX_Texture(gfx_texture_id ID) {
	renderer* Renderer = Renderer_Get();
	texture_manager* TextureManager = &Renderer->TextureManager;
	if (!Slot_Map_Is_Allocated(&TextureManager->TextureSlotMap, ID)) return NULL;

	gfx_texture* Result = TextureManager->Textures + ID.Index;
	return Result;
}

function inline gfx_texture* Get_Texture(string TextureName, b32 IsSRGB) {
	renderer* Renderer = Renderer_Get();
	texture_manager* TextureManager = &Renderer->TextureManager;
	
	u64 Hash = U64_Hash_String(TextureName);
	u64 SlotIndex = Hash & TEXTURE_SLOT_MASK;
	
	texture_slot* Slot = TextureManager->TextureHashSlots + SlotIndex;
	gfx_texture* Texture = NULL;
	for (gfx_texture* HashTexture = Slot->First; HashTexture; HashTexture = HashTexture->Next) {
		if (HashTexture->Hash == Hash) {
			Texture = HashTexture;
			break;
		}
	}

	if (!Texture) {
		arena* Scratch = Scratch_Get();
		string FilePath = String_Format((allocator*)Scratch, "textures/%.*s.png", TextureName.Size, TextureName.Ptr);
		buffer Buffer = Read_Entire_File((allocator*)Scratch, FilePath);
		if (!Buffer_Is_Empty(Buffer)) {
			int Width, Height, NumChannels;
			u8* Texels = (u8*)stbi_load_from_memory(Buffer.Ptr, Buffer.Size, &Width, &Height, &NumChannels, 0);
			if (Texels) {
				u8* DstTexels = Texels;
				gdi_format Format = GDI_FORMAT_NONE;
				if (NumChannels == 1) {
					Format = GDI_FORMAT_R8_UNORM;
				}

				if (NumChannels == 2) {
					Format = GDI_FORMAT_R8G8_UNORM;
				}
				
				if (NumChannels == 3) {
					Format = GDI_FORMAT_R8G8B8A8_UNORM;
					DstTexels = Arena_Push_Array(Scratch, Width*Height * 4, u8);
					
					u8* DstTexelsAt = DstTexels;
					u8* SrcTexelsAt = Texels;

					for (int y = 0; y < Height; y++) {
						for (int x = 0; x < Width; x++) {
							*DstTexelsAt++ = *SrcTexelsAt++;
							*DstTexelsAt++ = *SrcTexelsAt++;
							*DstTexelsAt++ = *SrcTexelsAt++;
							*DstTexelsAt++ = 255;
						}
					}
				}

				if (NumChannels == 4) {
					Format = GDI_FORMAT_R8G8B8A8_UNORM;
				}

				if (Format != GDI_FORMAT_NONE) {
					buffer FinalTexels = Make_Buffer(DstTexels, Width * Height*GDI_Get_Format_Size(Format));
					gfx_texture_id ID = Create_GFX_Texture( {
						.Dim = V2i(Width, Height),
						.Format = IsSRGB ? GDI_Get_SRGB_Format(Format) : Format,
						.Texels = &FinalTexels,
						.DebugName = TextureName
					});

					Texture = Get_GFX_Texture(ID);
					if (Texture) {
						Texture->Hash = Hash;
						DLL_Push_Back(Slot->First, Slot->Last, Texture);
					}
				}

				stbi_image_free(Texels);
			}
		}

		Scratch_Release();
	}

	return Texture;
}

function void Delete_GFX_Sampler(gfx_sampler_id ID) {
	if (!ID.ID) return;
	
	renderer* Renderer = Renderer_Get();
	texture_manager* TextureManager = &Renderer->TextureManager;

	if (Slot_Map_Is_Allocated(&TextureManager->SamplerSlotMap, ID)) {
		gfx_sampler* Sampler = TextureManager->Samplers + ID.Index;
		GDI_Delete_Sampler(Sampler->Sampler);
		Slot_Map_Free(&TextureManager->SamplerSlotMap, ID);
	}
}

function gfx_sampler_id Create_GFX_Sampler(const gdi_sampler_create_info& CreateInfo) {	
	arena* Scratch = Scratch_Get();
	
	renderer* Renderer = Renderer_Get();
	texture_manager* TextureManager = &Renderer->TextureManager;

	gfx_sampler_id ID = Slot_Map_Allocate(&TextureManager->SamplerSlotMap);
	gfx_sampler* Sampler = TextureManager->Samplers + ID.Index;
	Memory_Clear(Sampler, sizeof(gfx_sampler));

	Sampler->Sampler = GDI_Create_Sampler(&CreateInfo);
	if (!GDI_Is_Null(Sampler->Sampler)) {
		Sampler->ID = ID;

		gdi_bind_group_write Write = {
			.Binding = 1,
			.Index = ID.Index,
			.Samplers = { .Ptr = &Sampler->Sampler, .Count = 1 }
		};

		gdi_bind_group_write_info WriteInfo = {
			.Writes = { .Ptr = &Write, .Count = 1 }
		};

		GDI_Write_Bind_Group(TextureManager->BindlessTextureBindGroup, &WriteInfo);

		Scratch_Release();

		return ID;
	}

	Scratch_Release();

	Delete_GFX_Sampler(ID);
	return Slot_Null();
}

function inline gfx_sampler* Get_GFX_Sampler(gfx_sampler_id ID) {
	renderer* Renderer = Renderer_Get();
	texture_manager* TextureManager = &Renderer->TextureManager;
	if (!Slot_Map_Is_Allocated(&TextureManager->SamplerSlotMap, ID)) return NULL;

	gfx_sampler* Result = TextureManager->Samplers + ID.Index;
	return Result;
}

function gfx_component_id Create_GFX_Component(const gfx_component_create_info& CreateInfo) {
	renderer* Renderer = Renderer_Get();

	material_info Info = CreateInfo.Material;
	material Material = {};

	Material.Diffuse.IsTexture = Info.Diffuse.IsTexture;
	if (Material.Diffuse.IsTexture) {
		gfx_texture* Texture = Get_Texture(Info.Diffuse.TextureName, true);
		if (Texture) {
			Material.Diffuse.Texture = Texture->ID;
		}
	} else {
		Material.Diffuse.Value = Info.Diffuse.Value;
	}

	gfx_component_id ID = Pool_Allocate(&Renderer->GfxComponents);
	gfx_component* Component = (gfx_component*)Pool_Get(&Renderer->GfxComponents, ID);
	Component->Transform = CreateInfo.Transform;
	Component->Material = Material;
	Component->Mesh = CreateInfo.Mesh;

	return ID;
}

function shader_data Create_Shader_Data(size_t Size, size_t Count, string DebugName) {
	shader_data Result = {};

	arena* Scratch = Scratch_Get();

	gdi_buffer_create_info BufferInfo = {
		.Size = Size*Count,
		.Usage = GDI_BUFFER_USAGE_CONSTANT_BUFFER,
		.DebugName = String_Is_Empty(DebugName) ? String_Empty() : String_Concat((allocator *)Scratch, DebugName, String_Lit("_Buffer"))
	};

	Result.Buffer = GDI_Create_Buffer(&BufferInfo);

	gdi_bind_group_buffer* Buffers = Arena_Push_Array(Scratch, Count, gdi_bind_group_buffer);
	for (size_t i = 0; i < Count; i++) {
		Buffers[i].Buffer = Result.Buffer;
		Buffers[i].Size = Size;
		Buffers[i].Offset = i * Size;
	}

	gdi_handle Layout = Renderer_Get()->ShaderManager.SingleShaderDataBindGroupLayout;
	if (Count > 1) {
		gdi_bind_group_binding Bindings[] = {
			{ .Type = GDI_BIND_GROUP_TYPE_CONSTANT_BUFFER, .Count = (u32)Count }
		};

		gdi_bind_group_layout_create_info BindGroupLayoutInfo = {
			.Bindings = { .Ptr = Bindings, .Count = Array_Count(Bindings) },
			.DebugName = String_Is_Empty(DebugName) ? String_Empty() : String_Concat((allocator *)Scratch, DebugName, String_Lit("_BindGroupLayout"))
		};

		Layout = GDI_Create_Bind_Group_Layout(&BindGroupLayoutInfo);
		Result.BindGroupLayout = Layout;
	}

	gdi_bind_group_create_info BindGroupInfo = {
		.Layout = Layout,
		.Buffers = { .Ptr = Buffers, .Count = Count },
		.DebugName = String_Is_Empty(DebugName) ? String_Empty() : String_Concat((allocator *)Scratch, DebugName, String_Lit("_BindGroup"))
	};

	Result.BindGroup = GDI_Create_Bind_Group(&BindGroupInfo);

	Scratch_Release();

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
			
			ui_draw_data DrawData = {
				.TextureIndex = (s32)Texture->ID.Index,
				.SamplerIndex = (s32)Renderer_Get()->DefaultSampler.Index
			};
			
			Render_Set_Push_Constants(RenderPass, &DrawData, sizeof(ui_draw_data));
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
	renderer* Renderer = Renderer_Get();
	ui_draw_data DrawData = {
		.TextureIndex = (s32)Renderer->WhiteTexture.Index,
		.SamplerIndex = (s32)Renderer->DefaultSampler.Index
	};

	Render_Set_Push_Constants(RenderPass, &DrawData, sizeof(ui_draw_data));

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


function shader* Get_Shader(string ShaderName) {
	renderer* Renderer = Renderer_Get();

	u64 Hash = U64_Hash_String(ShaderName);
	u64 SlotIndex = Hash & SHADER_SLOT_MASK;
	shader_slot* ShaderSlot = Renderer->ShaderManager.ShaderSlots + SlotIndex;

	shader* Shader = NULL;
	for (shader* HashShader = ShaderSlot->First; HashShader; HashShader = HashShader->Next) {
		if (HashShader->Hash == Hash) {
			Shader = HashShader;
			break;
		}
	}
	
	if (!Shader) {
		Shader = Arena_Push_Struct(Renderer->Arena, shader);
		Shader->Hash = Hash;
		Shader->Reload = true;

		SLL_Push_Back(ShaderSlot->First, ShaderSlot->Last, Shader);
	}

	if (Shader->Reload) {
		arena* Scratch = Scratch_Get();
		string FilePath = String_Format((allocator*)Scratch, "shaders/%.*s.shader", ShaderName.Size, ShaderName.Ptr);

		if (!Shader->HotReload) {
			Shader->HotReload = OS_Hot_Reload_Create(FilePath);
		}

		if (!Buffer_Is_Empty(Shader->Code)) {
			Allocator_Free_Memory(Default_Allocator_Get(), Shader->Code.Ptr);
		}

		Shader->Code = Read_Entire_File(Default_Allocator_Get(), FilePath);

		Scratch_Release();
	}

	return Shader;
}

function b32 Hot_Reload_Shaders() {
	renderer* Renderer = Renderer_Get();
	shader_manager* ShaderManager = &Renderer->ShaderManager;

	//First check every shader and set their reload flags if the hot reloader
	//detects it. This way we can manually set the reload flag for whatever reason
	//(like startup) and still have it reload the shaders
	for (u32 i = 0; i < MAX_SHADER_SLOT_COUNT; i++) {
		shader_slot* Slot = ShaderManager->ShaderSlots + i;
		for (shader* Shader = Slot->First; Shader; Shader = Shader->Next) {
			if (OS_Hot_Reload_Has_Reloaded(Shader->HotReload)) {
				Shader->Reload = true;
			}
		}
	}

	//Basic 
	{
		shader* VtxShader = Get_Shader(String_Lit("basic_shader_vtx"));
		shader* PxlShader = Get_Shader(String_Lit("basic_shader_pxl"));

		if (VtxShader->Reload || PxlShader->Reload) {
			if (!GDI_Is_Null(ShaderManager->BasicShader)) {
				GDI_Delete_Shader(ShaderManager->BasicShader);
			}

			if (!GDI_Is_Null(ShaderManager->BasicLineShader)) {
				GDI_Delete_Shader(ShaderManager->BasicLineShader);
			}

			gdi_vtx_attribute Attributes[] = {
				{ .Semantic = String_Lit("POSITION"), .Format = GDI_FORMAT_R32G32B32_FLOAT }
			};

			gdi_vtx_binding VtxBindings[] = {
				{ .Stride = sizeof(v3), .Attributes = { .Ptr = Attributes, .Count = Array_Count(Attributes) } }
			};

			gdi_shader_create_info ShaderCreateInfo = {
				.VS = VtxShader->Code,
				.PS = PxlShader->Code,
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

			ShaderManager->BasicShader = GDI_Create_Shader(&ShaderCreateInfo);
			if (GDI_Is_Null(ShaderManager->BasicShader)) return false;

			ShaderCreateInfo.Primitive = GDI_PRIMITIVE_LINE;
			ShaderCreateInfo.DebugName = String_Lit("Basic Line Shader");
			ShaderManager->BasicLineShader = GDI_Create_Shader(&ShaderCreateInfo);
			if (GDI_Is_Null(ShaderManager->BasicLineShader)) return false;

			Memory_Clear(&ShaderCreateInfo.DepthState, sizeof(gdi_depth_state));
			ShaderCreateInfo.DepthState.DepthFormat = GDI_FORMAT_D32_FLOAT;
			ShaderCreateInfo.DebugName = String_Lit("Basic Line Shader No Depth");
			ShaderManager->BasicLineNoDepthShader = GDI_Create_Shader(&ShaderCreateInfo);
			if (GDI_Is_Null(ShaderManager->BasicLineNoDepthShader)) return false;
		}
	}

	//Entity
	{
		shader* VtxShader = Get_Shader(String_Lit("entity_vtx"));
		shader* PxlShader = Get_Shader(String_Lit("entity_pxl"));

		if (VtxShader->Reload || PxlShader->Reload) {
			if (!GDI_Is_Null(ShaderManager->EntityShader)) {
				GDI_Delete_Shader(ShaderManager->EntityShader);
			}

			gdi_vtx_attribute PositionAttrib[] = {
				{ .Semantic = String_Lit("POSITION"), .Format = GDI_FORMAT_R32G32B32_FLOAT }
			};

			gdi_vtx_attribute VtxAttribs[] = {
				{ .Semantic = String_Lit("NORMAL"), .Format = GDI_FORMAT_R32G32B32_FLOAT },
				{ .Semantic = String_Lit("UV"), .Format = GDI_FORMAT_R32G32_FLOAT }
			};

			gdi_vtx_binding VtxBindings[] = {
				{ .Stride = sizeof(v3), .Attributes = { .Ptr = PositionAttrib, .Count = Array_Count(PositionAttrib) } },
				{ .Stride = sizeof(vtx_attrib), .Attributes = { .Ptr = VtxAttribs, .Count = Array_Count(VtxAttribs) } }
			};

			gdi_handle BindGroupLayouts[] = {
				Renderer->ShaderManager.SingleShaderDataBindGroupLayout,
				Renderer->ShaderManager.EntityData.BindGroupLayout,
				Renderer->TextureManager.BindlessTextureBindGroupLayout
			};

			gdi_shader_create_info ShaderCreateInfo = {
				.VS = VtxShader->Code,
				.PS = PxlShader->Code,
				.BindGroupLayouts = { .Ptr = BindGroupLayouts, .Count = Array_Count(BindGroupLayouts) },
				.PushConstantCount = sizeof(entity_draw_data)/sizeof(u32),
				.VtxBindings = { .Ptr = VtxBindings, .Count = Array_Count(VtxBindings) },
				.RenderTargetFormats = { GDI_Get_View_Format() },
				.DepthState = {
					.TestEnabled = true,
					.WriteEnabled = true,
					.CompareFunc = GDI_COMPARE_FUNC_LEQUAL,
					.DepthFormat = GDI_FORMAT_D32_FLOAT
				},
				.DebugName = String_Lit("Entity Shader")
			};

			ShaderManager->EntityShader = GDI_Create_Shader(&ShaderCreateInfo);
			if (GDI_Is_Null(ShaderManager->EntityShader)) return false;
		}
	}

	//UI 
	{
		shader* VtxShader = Get_Shader(String_Lit("ui_vtx"));
		shader* PxlShader = Get_Shader(String_Lit("ui_pxl"));

		if (VtxShader->Reload || PxlShader->Reload) {
			if (!GDI_Is_Null(ShaderManager->UIShader)) {
				GDI_Delete_Shader(ShaderManager->UIShader);
			}

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

			gdi_handle BindGroupLayouts[] = {
				Renderer->ShaderManager.SingleShaderDataBindGroupLayout,
				Renderer->TextureManager.BindlessTextureBindGroupLayout
			};

			gdi_shader_create_info ShaderCreateInfo = {
				.VS = VtxShader->Code,
				.PS = PxlShader->Code,
				.BindGroupLayouts = { .Ptr = BindGroupLayouts, .Count = Array_Count(BindGroupLayouts) },
				.PushConstantCount = sizeof(ui_draw_data) / sizeof(u32),
				.VtxBindings = { .Ptr = VtxBindings, .Count = Array_Count(VtxBindings) },
				.RenderTargetFormats = { GDI_Get_View_Format() },
				.BlendStates = { .Ptr = BlendStates, .Count = Array_Count(BlendStates) },
				.DebugName = String_Lit("UI Shader")
			};

			ShaderManager->UIShader = GDI_Create_Shader(&ShaderCreateInfo);
			if (GDI_Is_Null(ShaderManager->UIShader)) return false;
		}
	}

	//Reset all the reload flags for all shaders after we have reloaded them
	for (u32 i = 0; i < MAX_SHADER_SLOT_COUNT; i++) {
		shader_slot* Slot = ShaderManager->ShaderSlots + i;
		for (shader* Shader = Slot->First; Shader; Shader = Shader->Next) {
			Shader->Reload = false;
		}
	}

	return true;
}

function b32 Renderer_Init(renderer* Renderer) {
	Renderer->Arena = Arena_Create();
	Renderer->GfxComponents = Pool_Init(sizeof(gfx_component));
	Renderer->GfxMeshes = Pool_Init(sizeof(gfx_mesh));

	{
		texture_manager* TextureManager = &Renderer->TextureManager;
		TextureManager->SamplerSlotMap = Slot_Map_Init((allocator*)Renderer->Arena, MAX_BINDLESS_SAMPLERS);
		TextureManager->TextureSlotMap = Slot_Map_Init((allocator*)Renderer->Arena, MAX_BINDLESS_TEXTURES);

		gdi_bind_group_binding Bindings[] = {
			{ .Type = GDI_BIND_GROUP_TYPE_TEXTURE, .Count = MAX_BINDLESS_TEXTURES },
			{ .Type = GDI_BIND_GROUP_TYPE_SAMPLER, .Count = MAX_BINDLESS_SAMPLERS }
		};

		gdi_bind_group_layout_create_info BindGroupLayoutInfo = {
			.Bindings = { .Ptr = Bindings, .Count = Array_Count(Bindings) },
			.DebugName = String_Lit("Bindless Texture Bind Group Layout")
		};

		TextureManager->BindlessTextureBindGroupLayout = GDI_Create_Bind_Group_Layout(&BindGroupLayoutInfo);
		if (GDI_Is_Null(TextureManager->BindlessTextureBindGroupLayout)) return false;

		gdi_bind_group_create_info BindGroupInfo = {
			.Layout = TextureManager->BindlessTextureBindGroupLayout,
			.DebugName = String_Lit("Bindless Texture Bind Group")
		};

		TextureManager->BindlessTextureBindGroup = GDI_Create_Bind_Group(&BindGroupInfo);
		if (GDI_Is_Null(TextureManager->BindlessTextureBindGroup)) return false;
	}

	{
		Renderer->DefaultSampler = Create_GFX_Sampler({
			.Filter = GDI_FILTER_LINEAR,
			.AddressModeU = GDI_ADDRESS_MODE_CLAMP,
			.AddressModeV = GDI_ADDRESS_MODE_CLAMP,
			.DebugName = String_Lit("Default Sampler")
		});
		if (Slot_Is_Null(Renderer->DefaultSampler)) return false;
	}

	{
		u32 TexelData[] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF };
		buffer Texels = Make_Buffer(TexelData, sizeof(TexelData));
		Renderer->WhiteTexture = Create_GFX_Texture({
			.Dim = V2i(2, 2),
			.Format = GDI_FORMAT_R8G8B8A8_SRGB,
			.Texels = &Texels,
			.DebugName = String_Lit("Default White Texture")
		});
	}

	{
		shader_manager* ShaderManager = &Renderer->ShaderManager;
		
		gdi_bind_group_binding Bindings[] = {
			{ .Type = GDI_BIND_GROUP_TYPE_CONSTANT_BUFFER, .Count = 1 }
		};
		gdi_bind_group_layout_create_info LayoutCreateInfo = {
			.Bindings = { .Ptr = Bindings, .Count = Array_Count(Bindings) },
			.DebugName = String_Lit("Single Shader Data Bind Group Layout")
		};
		ShaderManager->SingleShaderDataBindGroupLayout = GDI_Create_Bind_Group_Layout(&LayoutCreateInfo);
		if (GDI_Is_Null(ShaderManager->SingleShaderDataBindGroupLayout)) return false;

		ShaderManager->EntityShaderData = Create_Shader_Data(shader_sizeof(entity_shader_data), 1, String_Lit("Entity Shader Data"));
		ShaderManager->EntityData = Create_Shader_Data(shader_sizeof(entity_data), MAX_ENTITY_DATA, String_Lit("Entity Data"));
		ShaderManager->UIShaderData = Create_Shader_Data(shader_sizeof(ui_shader_data), 1, String_Lit("UI Shader Data"));
	}

	if (!Hot_Reload_Shaders()) {
		return false;
	}

	if (!Draw_Primitives_Init(&Renderer->DrawPrimitives)) {
		return false;
	}

	return true;
}

function void Update_Entity_Data(render_context* Context) {
	renderer* Renderer = Renderer_Get();
	shader_manager* ShaderManager = &Renderer->ShaderManager;

	entity_shader_data* ShaderData = (entity_shader_data*)GDI_Map_Buffer(ShaderManager->EntityShaderData.Buffer);
	ShaderData->WorldToClip = M4_Transpose(&Context->WorldToClip);
	GDI_Unmap_Buffer(ShaderManager->EntityShaderData.Buffer);

	entity_data* EntityData = (entity_data*)GDI_Map_Buffer(ShaderManager->EntityData.Buffer);
	for (pool_iter Iter = Pool_Begin_Iter(&Renderer->GfxComponents); Iter.IsValid; Pool_Iter_Next(&Iter)) {
		gfx_component* Component = (gfx_component*)Iter.Data;
		
		m4_affine_transposed ModelTranspose = M4_Affine_Transpose(&Component->Transform);
		Memory_Copy(&EntityData->ModelToWorld, &ModelTranspose, sizeof(m4_affine));

		m3 NormalTransform = M3_Inverse(&Component->Transform.M);
		NormalTransform = M3_Transpose(&NormalTransform);

		m4_affine NormalTransformAffine = M4_Affine_From_M3(&NormalTransform);
		m4_affine_transposed NormalTransformAffineTranspose = M4_Affine_Transpose(&NormalTransformAffine);
		Memory_Copy(&EntityData->NormalModelToWorld, &NormalTransformAffineTranspose, sizeof(m4_affine));

		material* Material = &Component->Material;
		EntityData->DiffusePacking.x = S32_To_F32_Bits(Material->Diffuse.IsTexture);
		if (Material->Diffuse.IsTexture) {
			gfx_texture* Texture = Get_GFX_Texture(Material->Diffuse.Texture);
			if (Texture) {
				EntityData->DiffusePacking.y = S32_To_F32_Bits(Texture->ID.Index);
			}
		} else {
			EntityData->DiffusePacking.yzw = Material->Diffuse.Value.xyz;
		}
		EntityData->FlagsPacking.x = S32_To_F32_Bits(Renderer->DefaultSampler.Index);

		EntityData = (entity_data*)Offset_Pointer(EntityData, shader_sizeof(entity_data));
	}
	GDI_Unmap_Buffer(ShaderManager->EntityData.Buffer);
}

function b32 Render_Frame(renderer* Renderer, camera* CameraView) {
	if (!Hot_Reload_Shaders()) return false;

	shader_manager* ShaderManager = &Renderer->ShaderManager;

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

	m4_affine WorldToView = Camera_Get_View(CameraView);
	m4 ViewToClip = M4_Perspective(To_Radians(60.0f), ViewDim.x / ViewDim.y, 0.1f, 10000.0f);
	m4 WorldToClip = M4_Affine_Mul_M4(&WorldToView, &ViewToClip);

	render_context RenderContext = {
		.ViewDim = ViewDim,
		.WorldToView = WorldToView,
		.ViewToClip = ViewToClip,
		.WorldToClip = WorldToClip
	};

	Update_Entity_Data(&RenderContext);

	{
		gdi_render_pass_begin_info RenderPassInfo = {
			.RenderTargetViews = { GDI_Get_View() },
			.DepthBufferView = Renderer->DepthView,
			.ClearColors = { {
				.ShouldClear = true,
				.F32 = { 0, 0, 0, 1 },
			} },
			.ClearDepth = {
				.ShouldClear = true,
				.Depth = 1.0f
			}
		};
		gdi_render_pass* RenderPass = GDI_Begin_Render_Pass(&RenderPassInfo);

		Render_Set_Shader(RenderPass, ShaderManager->EntityShader);       

		gdi_handle BindGroups[] = {
			ShaderManager->EntityShaderData.BindGroup,
			ShaderManager->EntityData.BindGroup,
			Renderer->TextureManager.BindlessTextureBindGroup
		};
		Render_Set_Bind_Groups(RenderPass, 0, BindGroups, Array_Count(BindGroups));

		s32 EntityIndex = 0;
		for (pool_iter Iter = Pool_Begin_Iter(&Renderer->GfxComponents); Iter.IsValid; Pool_Iter_Next(&Iter)) {
			gfx_component* Component = (gfx_component*)Iter.Data;
			gfx_mesh* Mesh = Get_GFX_Mesh(Component->Mesh);
			if (Mesh) {
				entity_draw_data DrawData = {
					.EntityIndex = EntityIndex
				};
				Render_Set_Push_Constants(RenderPass, &DrawData, sizeof(entity_draw_data));
				Draw_Mesh(RenderPass, Mesh);
				EntityIndex++;
			}
		}

		GDI_End_Render_Pass(RenderPass);
		GDI_Submit_Render_Pass(RenderPass);
	}

	if (!Render_Draw_Primitives(&RenderContext)) return false;

	{
		gdi_render_pass_begin_info RenderPassInfo = { .RenderTargetViews = { GDI_Get_View() } };
		gdi_render_pass* RenderPass = GDI_Begin_Render_Pass(&RenderPassInfo);

		ui_shader_data* ShaderData = (ui_shader_data*)GDI_Map_Buffer(ShaderManager->UIShaderData.Buffer);
		ShaderData->InvResolution = V2(1.0f / ViewDim.x, 1.0f / ViewDim.y);
		GDI_Unmap_Buffer(ShaderManager->UIShaderData.Buffer);

		gdi_handle BindGroups[] = { ShaderManager->UIShaderData.BindGroup, Renderer->TextureManager.BindlessTextureBindGroup };

		Render_Set_Shader(RenderPass, ShaderManager->UIShader);
		Render_Set_Bind_Groups(RenderPass, 0, BindGroups, Array_Count(BindGroups));

		Draw_UI(RenderPass, UI_Get());

		IM_Flush(RenderPass);
		GDI_End_Render_Pass(RenderPass);
		GDI_Submit_Render_Pass(RenderPass);
	}

	gdi_render_params RenderParams = {};
	GDI_Render(&RenderParams);
	
	return true;
}