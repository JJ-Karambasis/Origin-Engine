#undef function
#undef local
#include <ufbx.h>
#include <ufbx.c>
#define function static
#define local static

function void* UFBX_Alloc(void* UserData, size_t Size) {
	arena* Arena = (arena*)UserData;
	void* Result = Arena_Push(Arena, Size);
	return Result;
}

function void* UFBX_Realloc(void* UserData, void* Ptr, size_t OldSize, size_t NewSize) {
	arena* Arena = (arena*)UserData;
	void* Result = Arena_Push(Arena, NewSize);

	if (Result && Ptr && OldSize) {
		Memory_Copy(Result, Ptr, Min(OldSize, NewSize));
	}

	return Result;
}

function void UFBX_Free(void* UserData, void* Ptr, size_t Size) {
	//Noop
}

function void Allocate_Indices(editable_mesh* Mesh, u32 IndexCount) {
	u64 IndexByteAt = IndexCount * sizeof(u32);
	if (IndexByteAt > Mesh->IndicesReserve.CommitSize) {
		void* Memory = Commit_New_Size(&Mesh->IndicesReserve, IndexByteAt);
		Assert(Memory);
	}
	Mesh->IndexCount = IndexCount;
}

function u32 Add_Vtx(editable_mesh* Mesh, v3 V, v3 N, v2 UV) {
	u32 Result = Mesh->VertexCount++;

	if (!Mesh->Normals) {
		Mesh->Normals = (v3*)Mesh->NormalReserve.BaseAddress;
	}

	if (!Mesh->UVs) {
		Mesh->UVs = (v2*)Mesh->UVReserve.BaseAddress;
	}

	//Take into account the editable mesh since it uses the position memory reservation to get the ptr
	u64 PositionByteAt = (Mesh->VertexCount * sizeof(v3))+sizeof(editable_mesh);

	u64 NormalByteAt = Mesh->VertexCount * sizeof(v3);
	u64 UVByteAt = Mesh->VertexCount * sizeof(v2);

	if (PositionByteAt > Mesh->PositionReserve.CommitSize) {
		void* Memory = Commit_New_Size(&Mesh->PositionReserve, PositionByteAt);
		Assert(Memory);
	}

	if (NormalByteAt > Mesh->NormalReserve.CommitSize) {
		void* Memory = Commit_New_Size(&Mesh->NormalReserve, NormalByteAt);
		Assert(Memory);
	}

	if (UVByteAt > Mesh->UVReserve.CommitSize) {
		void* Memory = Commit_New_Size(&Mesh->UVReserve, UVByteAt);
		Assert(Memory);
	}

	Mesh->Positions[Result] = V;
	Mesh->Normals[Result] = N;
	Mesh->UVs[Result] = UV;

	return Result;
}

function u32 Add_Position(editable_mesh* Mesh, v3 V) {
	u32 Result = Mesh->VertexCount++;

	//Take into account the editable mesh since it uses the position memory reservation to get the ptr
	u64 PositionByteAt = (Mesh->VertexCount * sizeof(v3))+sizeof(editable_mesh);
	if (PositionByteAt > Mesh->PositionReserve.CommitSize) {
		void* Memory = Commit_New_Size(&Mesh->PositionReserve, PositionByteAt);
		Assert(Memory);
	}

	Mesh->Positions[Result] = V;

	return Result;
}

function inline void Add_Index(editable_mesh* Mesh, u32 Value) {
	u32 Index = Mesh->IndexCount;
	Allocate_Indices(Mesh, Index+1);
	Mesh->Indices[Index] = Value;
}

function inline void Add_Triangle(editable_mesh* Mesh, u32 I0, u32 I1, u32 I2) {
	u32 Index = Mesh->IndexCount;
	Allocate_Indices(Mesh, Index + 3);
	Mesh->Indices[Index+0] = I0;
	Mesh->Indices[Index+1] = I1;
	Mesh->Indices[Index+2] = I2;
}

function inline void Add_Segment(editable_mesh* Mesh, u32 I0, u32 I1) {
	u32 Index = Mesh->IndexCount;
	Allocate_Indices(Mesh, Index + 2);
	Mesh->Indices[Index + 0] = I0;
	Mesh->Indices[Index + 1] = I1;
}

function void Add_Circle_Line_Indices(editable_mesh* Mesh, u32 StartSampleIndex, u32 CircleSampleCount, u32 ValueOffset) {
    u32 TotalSampleCount = StartSampleIndex+CircleSampleCount;
    for(u32 SampleIndex = StartSampleIndex; SampleIndex < TotalSampleCount; SampleIndex++)
    {
        if(SampleIndex == (TotalSampleCount-1))
        {
            Add_Segment(Mesh, SampleIndex + ValueOffset, (SampleIndex - (CircleSampleCount-1)) + ValueOffset);
        }
        else
        {
            Add_Segment(Mesh, SampleIndex + ValueOffset, (SampleIndex+1) + ValueOffset);
        }
    } 
}

function void Add_Part(editable_mesh* Mesh, u32 VertexAt, u32 VertexCount, u32 IndexAt, u32 IndexCount) {
	u32 PartIndex = Mesh->PartCount++;

	if (!Mesh->Parts) {
		Mesh->Parts = (mesh_part*)Mesh->PartReserve.BaseAddress;
	}

	u64 PartByAt = (Mesh->PartCount * sizeof(mesh_part));
	if (PartByAt > Mesh->PartReserve.CommitSize) {
		void* Memory = Commit_New_Size(&Mesh->PartReserve, PartByAt);
		Assert(Memory);
	}

	mesh_part Part = {
		.VertexAt = VertexAt,
		.VertexCount = VertexCount,
		.IndexAt = IndexAt,
		.IndexCount = IndexCount
	};
	Mesh->Parts[PartIndex] = Part;
}

function inline b32 Editable_Mesh_Has_Position(editable_mesh* Mesh) {
	return Mesh->VertexCount > 0;
}

function inline b32 Editable_Mesh_Has_Indices(editable_mesh* Mesh) {
	return Mesh->IndexCount > 0;
}

function inline b32 Editable_Mesh_Has_UVs(editable_mesh* Mesh) {
	return Mesh->VertexCount > 0 && Mesh->UVs != NULL; 
}

function inline b32 Editable_Mesh_Has_Normals(editable_mesh* Mesh) {
	return Mesh->VertexCount > 0 && Mesh->Normals != NULL; 
}

function inline b32 Editable_Mesh_Has_Attribs(editable_mesh* Mesh) {
	return Editable_Mesh_Has_UVs(Mesh) && Editable_Mesh_Has_Normals(Mesh);
}

function editable_mesh* Create_Editable_Mesh_With_Size(u32 MaxVertexCount, u32 MaxIndexCount) {
	//This is probably way more material parts than what would actually be needed
	const u32 MaxPartCount = 10000;

	size_t PageSize = OS_Page_Size();

	size_t TotalPositionSize = Align(sizeof(editable_mesh)+(sizeof(v3)*MaxVertexCount), PageSize);
	size_t TotalNormalSize = Align(sizeof(v3)*MaxVertexCount, PageSize);
	size_t TotalUVSize = Align(sizeof(v2)*MaxVertexCount, PageSize);
	size_t TotalIndicesSize = Align(sizeof(u32)*MaxIndexCount, PageSize);
	size_t TotalPartSize = Align(sizeof(mesh_part)*MaxPartCount, PageSize);

	size_t TotalSize = TotalPositionSize + TotalNormalSize + TotalUVSize + TotalIndicesSize+TotalPartSize;
	memory_reserve BaseReserve = Make_Memory_Reserve(TotalSize);
	if (!BaseReserve.BaseAddress) {
		return NULL;
	}

	size_t Offset = 0;

	memory_reserve PositionReserve = Subdivide_Memory_Reserve(&BaseReserve, Offset, TotalPositionSize);
	Offset += TotalPositionSize;

	memory_reserve NormalReserve = Subdivide_Memory_Reserve(&BaseReserve, Offset, TotalNormalSize);
	Offset += TotalNormalSize;

	memory_reserve UVReserve = Subdivide_Memory_Reserve(&BaseReserve, Offset, TotalUVSize);
	Offset += TotalUVSize;

	memory_reserve IndicesReserve = Subdivide_Memory_Reserve(&BaseReserve, Offset, TotalIndicesSize);
	Offset += TotalIndicesSize;

	memory_reserve PartReserve = Subdivide_Memory_Reserve(&BaseReserve, Offset, TotalPartSize);

	editable_mesh* Result = (editable_mesh*)Commit_New_Size(&PositionReserve, sizeof(editable_mesh));
	
	Result->BaseReserve = BaseReserve;
	Result->PositionReserve = PositionReserve;
	Result->NormalReserve = NormalReserve;
	Result->UVReserve = UVReserve;
	Result->IndicesReserve = IndicesReserve;
	Result->PartReserve = PartReserve;

	//Take into account the editable mesh since it uses the position memory reservation to get the ptr
	Result->Positions = (v3*)(PositionReserve.BaseAddress + sizeof(editable_mesh));
	Result->Indices = (u32*)IndicesReserve.BaseAddress;

	return Result;
}

function editable_mesh* Create_Editable_Mesh() {
	editable_mesh* Result = Create_Editable_Mesh_With_Size(1000000, 3000000);
	return Result;
}

function editable_mesh* Create_Editable_Mesh_From_File(string Path) {
	arena* Scratch = Scratch_Get();

	buffer Buffer = Read_Entire_File((allocator*)Scratch, Path);
	if (Buffer_Is_Empty(Buffer)) {
		//todo: Diagnostic and error logging
		Scratch_Release();
		return NULL;
	}

	ufbx_allocator Allocator = {
		.alloc_fn = UFBX_Alloc,
		.realloc_fn = UFBX_Realloc,
		.free_fn = UFBX_Free,
		.user = Scratch
	};

	ufbx_load_opts Options = {
		.temp_allocator = { .allocator = Allocator },
		.result_allocator = { .allocator = Allocator },
		.target_axes = ufbx_axes_right_handed_z_up
	};

	ufbx_error Error;
	ufbx_scene* Scene = ufbx_load_memory(Buffer.Ptr, Buffer.Size, &Options, &Error);
	if (!Scene) {
		//todo: Diagnostic and error logging
		Scratch_Release();
		return NULL;
	}

	editable_mesh* Result = Create_Editable_Mesh();
	for (u64 i = 0; i < Scene->nodes.count; i++) {
		ufbx_node* UNode = Scene->nodes.data[i];
		if (UNode->is_root) continue;

		if (UNode->mesh) {
			ufbx_mesh* UMesh = UNode->mesh;

			u32 TriangleIndicesCount = (u32)(UMesh->max_face_triangles * 3);
			u32* TriangleIndices = Arena_Push_Array(Scratch, TriangleIndicesCount, u32);

			m4_affine Transform = M4_Affine_F64(UNode->node_to_world.v);
			m3 InvTransform = M3_Inverse(&Transform.M);
			m3 NormalTransform = M3_Transpose(&InvTransform);

			for (u64 j = 0; j < UMesh->material_parts.count; j++) {
				ufbx_mesh_part* UMeshPart = &UMesh->material_parts.data[j];
				u32 VertexAt = Result->VertexCount;

				u32 TriangleCount = (u32)UMeshPart->num_triangles;
				for (u64 f = 0; f < UMeshPart->num_faces; f++) {
					ufbx_face UFace = UMesh->faces.data[UMeshPart->face_indices.data[f]];
					u32 NumTriangles = ufbx_triangulate_face(TriangleIndices, TriangleIndicesCount, UMesh, UFace);
					for (u32 t = 0; t < NumTriangles*3; t++) {
						u32 Index = TriangleIndices[t];

						v3 P = V3_F64(ufbx_get_vertex_vec3(&UMesh->vertex_position, Index).v);
						v3 N = V3_F64(ufbx_get_vertex_vec3(&UMesh->vertex_normal, Index).v);
						v2 UV = V2_F64(ufbx_get_vertex_vec2(&UMesh->vertex_uv, Index).v);

						Add_Vtx(Result, P, N, UV);
					}
				}
			
				u32 IndexCount = TriangleCount*3;
				u32 IndexAt = Result->IndexCount;
				u32* Indices = Result->Indices + IndexAt;
				Allocate_Indices(Result, Result->IndexCount+IndexCount);

				u32 VertexCount = Result->VertexCount - VertexAt;
				Assert(VertexCount == IndexCount);

				ufbx_vertex_stream Streams[] = {
					{ Result->Positions+VertexAt, VertexCount, sizeof(v3) },
					{ Result->Normals+VertexAt, VertexCount, sizeof(v3) },
					{ Result->UVs+VertexAt, VertexCount, sizeof(v2) }
				};

				VertexCount = (u32)ufbx_generate_indices(Streams, Array_Count(Streams), Indices, 
														 IndexCount, &Options.temp_allocator, NULL);
				Result->VertexCount = VertexAt+VertexCount;

				Add_Part(Result, VertexAt, VertexCount, IndexAt, IndexCount);

				for (size_t i = 0; i < VertexCount; i++) {
					size_t VertexIndex = i + VertexAt;
					Result->Positions[VertexIndex] = V4_Mul_M4_Affine(V4_From_V3(Result->Positions[VertexIndex], 1.0f), &Transform);
					Result->Normals[VertexIndex] = V3_Norm(V3_Mul_M3(Result->Normals[VertexIndex], &NormalTransform));
				}
			}
		}
	}

	ufbx_free_scene(Scene);

	Scratch_Release();
	 
	return Result;
}

function void Delete_Editable_Mesh(editable_mesh* Mesh) {
	if (Mesh) {
		Delete_Memory_Reserve(&Mesh->BaseReserve);
	}
}

function f32 Get_Circle_Rad_Offset(u32 SampleCount)
{
    f32 Result = (2.0f*PI)/(f32)SampleCount;
    return Result;
}

function editable_mesh* Create_Cone_Mesh(u32 CircleSampleCount) {
	editable_mesh* Mesh = Create_Editable_Mesh_With_Size(CircleSampleCount+2, CircleSampleCount*3*2);
	Commit_All(&Mesh->PositionReserve);
	Commit_All(&Mesh->IndicesReserve);

	f32 CircleRadOffset = Get_Circle_Rad_Offset(CircleSampleCount);

	f32 Radians = 0.0f;
    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++, Radians += CircleRadOffset) {
        Add_Position(Mesh, V3(Cos_F32(Radians), Sin_F32(Radians), 0.0f));
    }

    Add_Position(Mesh, V3_Zero());
    Add_Position(Mesh, V3(0.0f, 0.0f, 1.0f));

    u32 CenterVertexOffset = CircleSampleCount;

    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++) {
        u32 SampleIndexPlusOne = (SampleIndex+1) % CircleSampleCount;
        Add_Triangle(Mesh, SampleIndex, CenterVertexOffset, SampleIndexPlusOne);
    }

    CenterVertexOffset++;
    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++) {
        u32 SampleIndexPlusOne = (SampleIndex+1) % CircleSampleCount;
        Add_Triangle(Mesh, SampleIndex, SampleIndexPlusOne, CenterVertexOffset);
    }

    return Mesh;
}

function editable_mesh* Create_Cylinder_Mesh(u32 CircleSampleCount) {
	editable_mesh* Mesh = Create_Editable_Mesh_With_Size(CircleSampleCount * 2 + 2, CircleSampleCount * 3 * 2 + CircleSampleCount * 6);
	Commit_All(&Mesh->PositionReserve);
	Commit_All(&Mesh->IndicesReserve);

	f32 CircleRadOffset = Get_Circle_Rad_Offset(CircleSampleCount);

	u32 CenterVertexOffset = CircleSampleCount*2;

    f32 Radians = 0.0f;
    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++, Radians += CircleRadOffset) {
        Add_Position(Mesh, V3(Cos_F32(Radians), Sin_F32(Radians), 0.0f));
    }

    Radians = 0.0f;
    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++, Radians += CircleRadOffset) {
        Add_Position(Mesh, V3(Cos_F32(Radians), Sin_F32(Radians), 1.0f));
    }

    Add_Position(Mesh, V3(0.0f, 0.0f, 0.0f));
    Add_Position(Mesh, V3(0.0f, 0.0f, 1.0f));

    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++) {
        u32 SampleIndexPlusOne = (SampleIndex+1) % CircleSampleCount;
		Add_Triangle(Mesh, SampleIndex, CenterVertexOffset, SampleIndexPlusOne);
    }

    CenterVertexOffset++;

    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++) {
        u32 ActualSampleIndex = SampleIndex+CircleSampleCount;
        u32 ActualSampleIndexPlusOne = ActualSampleIndex+1;
        if(ActualSampleIndexPlusOne == (CircleSampleCount*2))
            ActualSampleIndexPlusOne = CircleSampleCount;

        Add_Triangle(Mesh, ActualSampleIndexPlusOne, CenterVertexOffset, ActualSampleIndex);
    }

    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++) {
        u32 BottomSampleIndex = SampleIndex;
        u32 TopSampleIndex = SampleIndex + CircleSampleCount;

        u32 BottomSampleIndexPlusOne = (SampleIndex+1) % CircleSampleCount;
        u32 TopSampleIndexPlusOne = TopSampleIndex+1;

        if(TopSampleIndexPlusOne == (CircleSampleCount*2))
            TopSampleIndexPlusOne = CircleSampleCount;

        Add_Triangle(Mesh, BottomSampleIndex, TopSampleIndexPlusOne, TopSampleIndex);
        Add_Triangle(Mesh, BottomSampleIndex, BottomSampleIndexPlusOne, TopSampleIndexPlusOne);
    }

	return Mesh;
}

function u32 Mesh_Edge_Hash(mesh_edge Key) {
	u32 V0 = Key.Idx1 > Key.Idx0 ? Key.Idx0 : Key.Idx1;
	u32 V1 = Key.Idx1 > Key.Idx0 ? Key.Idx1 : Key.Idx0;
	return U32_Hash_U64(((u64)V0 << 32) | V1);
}

function b32 Mesh_Edge_Key_Equal(mesh_edge A, mesh_edge B) {
	b32 Result = (A.Idx0 == B.Idx0) && (A.Idx1 == B.Idx1);
	return Result;
}

function u32* Mesh_Edge_Table_Get(mesh_edge_table* Table, mesh_edge Key) {
	u32 Hash = Mesh_Edge_Hash(Key);
	u32 SlotMask = Table->SlotCapacity - 1;
	u32 SlotIndex = Hash & SlotMask;

	mesh_edge_table_slot* Slot = Table->Slots + SlotIndex;
	for (mesh_edge_table_slot_entry* Entry = Slot->First; Entry; Entry = Entry->Next) {
		if (Entry->Hash == Hash && Mesh_Edge_Key_Equal(Entry->Key, Key)) {
			return &Entry->Value;
		}
	}

	return NULL;
}

function void Mesh_Edge_Table_Add(mesh_edge_table* Table, mesh_edge Key, u32 Value) {
	Assert(Mesh_Edge_Table_Get(Table, Key) == NULL);

	u32 Hash = Mesh_Edge_Hash(Key);
	u32 SlotMask = Table->SlotCapacity - 1;
	u32 SlotIndex = Hash & SlotMask;

	mesh_edge_table_slot* Slot = Table->Slots + SlotIndex;
	mesh_edge_table_slot_entry* Entry = Arena_Push_Struct(Table->Arena, mesh_edge_table_slot_entry);
	Entry->Hash = Hash;
	Entry->Key = Key;
	Entry->Value = Value;

	SLL_Push_Back(Slot->First, Slot->Last, Entry);
}

function u32 Mesh_Subdivide_Edge(u32 Index0, u32 Index1, v3 P0, v3 P1, v3_array* OutVertices, u32_array* OutIndices, mesh_edge_table* EdgeTable) {
    mesh_edge Key = {Index0, Index1};
	u32* Index = Mesh_Edge_Table_Get(EdgeTable, Key);
    if(Index) return *Index;

    v3 P = V3_Norm(V3_Mul_S(V3_Add_V3(P0, P1), 0.5f));
    u32 Result = (u32)OutVertices->Count++;
    OutVertices->Ptr[Result] = P;
    Mesh_Edge_Table_Add(EdgeTable, Key, Result);
    return Result;
}

function editable_mesh* Create_Sphere_Mesh(u32 Subdivisions) {
	f32 t = (1.0f + Sqrt_F32(5.0f)) * 0.5f;
    v3 TempVertices[] = 
    {
        V3_Norm(V3(-1.0f,  t,     0.0f)),
        V3_Norm(V3( 1.0f,  t,     0.0f)),
        V3_Norm(V3(-1.0f, -t,     0.0f)),
        V3_Norm(V3( 1.0f, -t,     0.0f)),        
        V3_Norm(V3( 0.0f, -1.0f,  t)),
        V3_Norm(V3( 0.0f,  1.0f,  t)),
        V3_Norm(V3( 0.0f, -1.0f, -t)),
        V3_Norm(V3( 0.0f,  1.0f, -t)),        
        V3_Norm(V3( t,     0.0f, -1.0f)),
        V3_Norm(V3( t,     0.0f,  1.0f)),
        V3_Norm(V3(-t,     0.0f, -1.0f)),
        V3_Norm(V3(-t,     0.0f,  1.0f))
    };
    
    u32 TempIndices[] = 
    {
        0, 11, 5,
        0, 5, 1,
        0, 1, 7, 
        0, 7, 10,
        0, 10, 11,
        1, 5, 9,
        5, 11, 4,
        11, 10, 2,
        10, 7, 6,
        7, 1, 8,
        3, 9, 4,
        3, 4, 2,
        3, 2, 6,
        3, 6, 8,
        3, 8, 9,
        4, 9, 5,
        2, 4, 11,
        6, 2, 10,
        8, 6, 7,
        9, 8, 1
    };

	arena* Scratch = Scratch_Get();

	v3_array InVertices = V3_Array_Copy((allocator*)Scratch, TempVertices, Array_Count(TempVertices));
	u32_array InIndices = U32_Array_Copy((allocator*)Scratch, TempIndices, Array_Count(TempIndices));

	for (u32 i = 0; i < Subdivisions; i++) {
		Assert((InIndices.Count % 3) == 0);
		
		u32 TriangleCount = (u32)InIndices.Count / 3;
		u32 NewVertexCount = (u32)InVertices.Count + (TriangleCount * 3);
		u32 NewIndexCount = (TriangleCount * 3) * 4;

		v3_array OutVertices = V3_Array_Alloc((allocator*)Scratch, NewVertexCount);
		u32_array OutIndices = U32_Array_Alloc((allocator*)Scratch, NewIndexCount);

		OutVertices.Count = InVertices.Count;
		OutIndices.Count = 0;

		Memory_Copy(OutVertices.Ptr, InVertices.Ptr, InVertices.Count * sizeof(v3));

		mesh_edge_table EdgeTable = { 
			.Arena = Scratch,
			.SlotCapacity = Ceil_Pow2_U32(NewIndexCount*3)
		};
		EdgeTable.Slots = Arena_Push_Array(Scratch, EdgeTable.SlotCapacity, mesh_edge_table_slot);

		for (u32 t = 0; t < TriangleCount; t++) {
			u32 Index0 = InIndices.Ptr[t*3 + 0];
            u32 Index1 = InIndices.Ptr[t*3 + 1];
            u32 Index2 = InIndices.Ptr[t*3 + 2];

            v3 P0 = InVertices.Ptr[Index0];
            v3 P1 = InVertices.Ptr[Index1];
            v3 P2 = InVertices.Ptr[Index2];

            u32 Index3 = Mesh_Subdivide_Edge(Index0, Index1, P0, P1, &OutVertices, &OutIndices, &EdgeTable);
            u32 Index4 = Mesh_Subdivide_Edge(Index1, Index2, P1, P2, &OutVertices, &OutIndices, &EdgeTable);
            u32 Index5 = Mesh_Subdivide_Edge(Index2, Index0, P2, P0, &OutVertices, &OutIndices, &EdgeTable);

            OutIndices.Ptr[OutIndices.Count++] = Index0;
            OutIndices.Ptr[OutIndices.Count++] = Index3;
            OutIndices.Ptr[OutIndices.Count++] = Index5;

            OutIndices.Ptr[OutIndices.Count++] = Index3;
            OutIndices.Ptr[OutIndices.Count++] = Index1;
            OutIndices.Ptr[OutIndices.Count++] = Index4;
            
            OutIndices.Ptr[OutIndices.Count++] = Index4;
            OutIndices.Ptr[OutIndices.Count++] = Index2;
            OutIndices.Ptr[OutIndices.Count++] = Index5;
            
            OutIndices.Ptr[OutIndices.Count++] = Index3;
            OutIndices.Ptr[OutIndices.Count++] = Index4;
            OutIndices.Ptr[OutIndices.Count++] = Index5;
		}

		Assert(OutVertices.Count <= NewVertexCount);
		Assert(OutIndices.Count == NewIndexCount);

		InVertices = OutVertices;
		InIndices = OutIndices;
	}

	editable_mesh* Result = Create_Editable_Mesh_With_Size((u32)InVertices.Count, (u32)InIndices.Count);
	Commit_All(&Result->PositionReserve);
	Commit_All(&Result->IndicesReserve);

	Memory_Copy(Result->Positions, InVertices.Ptr, sizeof(v3)*InVertices.Count);
	Memory_Copy(Result->Indices, InIndices.Ptr, InIndices.Count * sizeof(u32));
	Result->VertexCount = (u32)InVertices.Count;
	Result->IndexCount = (u32)InIndices.Count;
	
	Scratch_Release();

	return Result;
}

function editable_mesh* Create_Box_Line_Editable_Mesh() {
    editable_mesh* Result = Create_Editable_Mesh_With_Size(8, 24);

    Add_Position(Result, V3(-1.0f, -1.0f,  1.0f));
    Add_Position(Result, V3( 1.0f, -1.0f,  1.0f));
    Add_Position(Result, V3( 1.0f,  1.0f,  1.0f));
    Add_Position(Result, V3(-1.0f,  1.0f,  1.0f));
    Add_Position(Result, V3( 1.0f, -1.0f, -1.0f));
    Add_Position(Result, V3(-1.0f, -1.0f, -1.0f));
    Add_Position(Result, V3(-1.0f,  1.0f, -1.0f));
    Add_Position(Result, V3( 1.0f,  1.0f, -1.0f));

    Add_Segment(Result, 0, 1);
    Add_Segment(Result, 1, 2);
    Add_Segment(Result, 2, 3);
    Add_Segment(Result, 3, 0);
    Add_Segment(Result, 4, 5);
    Add_Segment(Result, 5, 6);
    Add_Segment(Result, 6, 7);
    Add_Segment(Result, 7, 4);
    Add_Segment(Result, 0, 5);
    Add_Segment(Result, 3, 6);
    Add_Segment(Result, 1, 4);
    Add_Segment(Result, 2, 7);

	return Result;
}

function editable_mesh* Create_Sphere_Line_Editable_Mesh(u32 CircleSampleCount) {
	editable_mesh* Mesh = Create_Editable_Mesh_With_Size(CircleSampleCount * 3, (CircleSampleCount * 3) * 2);
	f32 CircleRadOffset = Get_Circle_Rad_Offset(CircleSampleCount);

    f32 Radians = 0.0f;
    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++, Radians += CircleRadOffset) {
        Add_Position(Mesh, V3(Cos_F32(Radians), Sin_F32(Radians), 0.0f));
    }

    Radians = 0.0f;
    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++, Radians += CircleRadOffset) {
        Add_Position(Mesh, V3(0.0f, Sin_F32(Radians), Cos_F32(Radians)));
    }

    Radians = 0.0f;
    for(u32 SampleIndex = 0; SampleIndex < CircleSampleCount; SampleIndex++, Radians += CircleRadOffset) {
        Add_Position(Mesh, V3(Cos_F32(Radians), 0.0f, Sin_F32(Radians)));
    }

	Add_Circle_Line_Indices(Mesh, 0, CircleSampleCount, 0);
	Add_Circle_Line_Indices(Mesh, 0, CircleSampleCount, CircleSampleCount);
	Add_Circle_Line_Indices(Mesh, 0, CircleSampleCount, CircleSampleCount * 2);

	return Mesh;
}