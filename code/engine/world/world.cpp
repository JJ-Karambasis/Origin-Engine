function inline world* World_Get() {
	return Get_Engine()->World;
}

function entity_id Create_Entity(const entity_create_info& CreateInfo) {
	world* World = World_Get();

	u64 EntityHash = U64_Hash_String(CreateInfo.Name);
	entity_slot* EntitySlot = NULL;
	{
		u64 SlotIndex = (EntityHash  & ENTITY_SLOT_MASK);
		EntitySlot = World->EntitySlots + SlotIndex;
		for (entity* HashEntry = EntitySlot->First; HashEntry; HashEntry = HashEntry->Next) {
			if (HashEntry->Hash == EntityHash) {
				Debug_Log("Duplicate entity names");
				return Empty_Pool_ID();
			}
		}
	}

	entity_id ID = (entity_id)Pool_Allocate(&World->Entities);
	entity* Entity = (entity*)Pool_Get(&World->Entities, ID);

	Entity->ID = ID;
	Entity->Name = String_Copy((allocator*)World->Heap, CreateInfo.Name);
	Entity->Hash = EntityHash ;
	Entity->Type = CreateInfo.Type;

	Entity->Position = CreateInfo.Position;
	Entity->Orientation = CreateInfo.Orientation;
	Entity->Scale = CreateInfo.Scale;

	engine* Engine = Get_Engine();

	mesh* Mesh = NULL;
	{
		u64 Hash = U64_Hash_String(CreateInfo.MeshName);
		u64 SlotIndex = (Hash & MESH_SLOT_MASK);
		mesh_slot* Slot = Engine->MeshSlots + SlotIndex;
		for (mesh* HashMesh = Slot->First; HashMesh; HashMesh = HashMesh->Next) {
			if (HashMesh->Hash == Hash) {
				Mesh = HashMesh;
				break;
			}
		}

		if (!Mesh) {
			arena* Scratch = Scratch_Get();
			string FilePath = String_Format((allocator*)Scratch, "meshes/%.*s.fbx", CreateInfo.MeshName.Size, CreateInfo.MeshName.Ptr);
			editable_mesh* EditableMesh = Create_Editable_Mesh_From_File(FilePath);

			if (EditableMesh) {
				Mesh = Arena_Push_Struct(Engine->Arena, mesh);
				Mesh->Hash = Hash;
				DLL_Push_Back(Slot->First, Slot->Last, Mesh);
				Mesh->GfxMesh = Create_GFX_Mesh(EditableMesh, CreateInfo.MeshName);
				Delete_Editable_Mesh(EditableMesh);
			}
			Scratch_Release();
		}
	}

	Entity->GfxComponent = Create_GFX_Component( {
		.Transform = M4_Affine_Transform_Quat(Entity->Position, Entity->Orientation, Entity->Scale),
		.Mesh = Mesh->GfxMesh,
		.Material = CreateInfo.Material
	});

	Entity->Collider = CreateInfo.Collider;

	DLL_Push_Back(EntitySlot->First, EntitySlot->Last, Entity);

	sim_message_create_entity SimCreateEntity = {
		.ID = ID,
		.Type = Entity->Type,
		.Position = Entity->Position,
		.Orientation = Entity->Orientation,
		.Scale = Entity->Scale,
		.Collider = Entity->Collider,
		.BodyType = CreateInfo.BodyType,
		.Friction = CreateInfo.Friction
	};
	Sim_Add_Message(SIM_MESSAGE_TYPE_CREATE_ENTITY, &SimCreateEntity);

	return ID;
}

function entity* Get_Entity(entity_id ID) {
	world* World = World_Get();
	entity* Result = (entity*)Pool_Get(&World->Entities, ID);
	return Result;
}

function void Sync_Simulation() {
	simulation* Simulation = Simulation_Get();

	//Get the old frame and the new frame while locking to prevent the simulation
	//from starting or finishing a frame while this occurs
	OS_Mutex_Lock(Simulation->FrameLock);
	sim_push_frame* OldFrame = NULL;
	sim_push_frame* NewFrame = NULL;
	for (;;) {
		b32 FreeFrame = false;

		OldFrame = Simulation->FirstFrame;
		if (!OldFrame) break;

		if (Time_Is_Newer(Simulation->UpdateTime, OldFrame->Time)) {
			sim_push_frame* NextFrame = OldFrame->Next;
			if (!NextFrame) {
				break;
			}

			if (Time_Is_Newer(NextFrame->Time, Simulation->UpdateTime)) {
				NewFrame = NextFrame;
				break;
			}

			SLL_Pop_Front(Simulation->FirstFrame);
			if (!Simulation->FirstFrame) Simulation->LastFrame = NULL;
			OldFrame->Next = NULL;
			SLL_Push_Front(Simulation->FreeFrames, OldFrame);
		} Invalid_Else;
	}
	OS_Mutex_Unlock(Simulation->FrameLock);

	//Set the new frame to let the update frame know if its still simulating or not
	engine* Engine = Get_Engine();
	Engine->OldFrame = OldFrame;
	Engine->NewFrame = NewFrame;

	if (OldFrame) {
		world* World = World_Get();

		//If there is at least an old frame, update the entities with the old
		//simulation frame to at least keep the rendering in sync and to assist
		//in interpolation if there is a new frame
		sim_entity_storage* SimEntities = &Simulation->Entities;

		if (Is_Update_Simulating()) {
			for (size_t i = 0; i < OldFrame->CmdCount; i++) {
				sim_push_cmd* Cmd = OldFrame->Cmds + i;
				entity* Entity = Get_Entity(Cmd->ID);
				if (Entity) {
					//Store old transforms in current transforms
					Entity->Position = Cmd->Position;
					Entity->Orientation = Cmd->Orientation;
				}
			}
		} else {
			for (pool_iter Iter = Pool_Begin_Iter(&World->Entities); Iter.IsValid; Pool_Iter_Next(&Iter)) {
				entity* Entity = (entity*)Iter.Data;
				if (Entity->ID.Index < SimEntities->Capacity) {
					sim_entity* SimEntity = SimEntities->Entities + Entity->ID.Index;
					if(SimEntity->IsAllocated) {
						Entity->Position = SimEntity->Position;
						Entity->Orientation = SimEntity->Orientation;
					}
				}
			}
		}

		if (NewFrame) {
			//If there is a new frame, interpolate between the two frames to get
			//the entities actual position in the frame

			Assert(Simulation->UpdateTime.Interval == OldFrame->Time.Interval &&
				   (OldFrame->Time.Interval == NewFrame->Time.Interval ||
				   OldFrame->Time.Interval == NewFrame->Time.Interval - 1));
		
			f64 tAt = Simulation->UpdateTime.dt - OldFrame->Time.dt;
			f64 tInterp = tAt / SIM_HZ;

			Assert(tInterp >= 0 && tInterp <= 1);

			for (size_t i = 0; i < NewFrame->CmdCount; i++) {
				sim_push_cmd* Cmd = NewFrame->Cmds + i;
				entity* Entity = Get_Entity(Cmd->ID);
				if (Entity) {
					Entity->Position = V3_Lerp(Entity->Position, (f32)tInterp, Cmd->Position);
					Entity->Orientation = Quat_Lerp(Entity->Orientation, (f32)tInterp, Cmd->Orientation);
				}
			}
		}

		if (NewFrame) {
			Time_Increment(&Simulation->UpdateTime, Get_Engine()->dt);
		}
	}
}

function world* World_Create(string WorldName) {
	heap* Heap = Heap_Create();
	world* World = Heap_Alloc_Struct(Heap, world);
	World->Heap = Heap;
	World->Name = String_Copy((allocator*)World->Heap, WorldName);
	World->Entities = Pool_Init(sizeof(entity));
	return World;
}