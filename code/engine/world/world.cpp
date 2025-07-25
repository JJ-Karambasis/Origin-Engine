function inline world* World_Get() {
	return Get_Engine()->World;
}

function void Sim_Add_Message(sim_message_type Type, void* Data) {
	sim_message_queue* Queue = &World_Get()->SimMessageQueue; 

	OS_Mutex_Lock(Queue->Lock);
	u32 EntryToWrite = Queue->EntryToWrite;
	Queue->EntryToWrite = (Queue->EntryToWrite + 1) % SIM_MESSAGE_QUEUE_COUNT;

	//todo: Should we block if we don't have any space? Is this even reasonably possible?
	Assert(Queue->EntryToWrite != Queue->EntryToRead);

	sim_message* Event = Queue->Queue + EntryToWrite;
	Event->Type = Type;

	switch (Type) {
		case SIM_MESSAGE_TYPE_CREATE_ENTITY: {
			Event->CreateEntity = *(sim_message_create_entity*)Data;
		} break;
	}

	OS_Mutex_Unlock(Queue->Lock);
}

function entity_id Create_Entity(const entity_create_info& CreateInfo) {
	u64 Hash = U64_Hash_String(CreateInfo.Name);
	u64 SlotIndex = (Hash & ENTITY_SLOT_MASK);

	world* World = World_Get();
	entity_slot* Slot = World->EntitySlots + SlotIndex;
	for (entity* HashEntry = Slot->First; HashEntry; HashEntry = HashEntry->Next) {
		if (HashEntry->Hash == Hash) {
			Debug_Log("Duplicate entity names");
			return Empty_Pool_ID();
		}
	}

	entity_id ID = (entity_id)Pool_Allocate(&World->Entities);
	entity* Entity = (entity*)Pool_Get(&World->Entities, ID);

	Entity->ID = ID;
	Entity->Name = String_Copy((allocator*)World->Heap, CreateInfo.Name);
	Entity->Hash = Hash;
	Entity->Type = CreateInfo.Type;

	Entity->Position = CreateInfo.Position;
	Entity->Orientation = CreateInfo.Orientation;
	Entity->Scale = CreateInfo.Scale;

	Entity->GfxComponent = Create_GFX_Component( {
		.Transform = M4_Affine_Transform_Quat(Entity->Position, Entity->Orientation, Entity->Scale),
		.Color = CreateInfo.Color,
		.MeshName = CreateInfo.MeshName
	});

	DLL_Push_Back(Slot->First, Slot->Last, Entity);

	sim_message_create_entity SimCreateEntity = {
		.ID = ID,
		.Type = Entity->Type,
		.Position = Entity->Position,
		.Orientation = Entity->Orientation,
		.Scale = Entity->Scale
	};
	Sim_Add_Message(SIM_MESSAGE_TYPE_CREATE_ENTITY, &SimCreateEntity);

	return ID;
}

function entity* Get_Entity(entity_id ID) {
	world* World = World_Get();
	entity* Result = (entity*)Pool_Get(&World->Entities, ID);
	return Result;
}

function b32 Sim_Get_Message(sim_message* Message) {
	sim_message_queue* Queue = &World_Get()->SimMessageQueue; 
	OS_Mutex_Lock(Queue->Lock);
	u32 EntryToRead = Queue->EntryToRead;
	b32 IsEmpty = EntryToRead == Queue->EntryToWrite;
	if (!IsEmpty) {
		*Message = Queue->Queue[EntryToRead];
		Queue->EntryToRead = (Queue->EntryToRead + 1) % SIM_MESSAGE_QUEUE_COUNT;
	}
	OS_Mutex_Unlock(Queue->Lock);
	return !IsEmpty;
}

function void Sim_Process_Messages() {
	world* World = World_Get();
	sim_message Message;
	while (Sim_Get_Message(&Message)) {
		switch (Message.Type) {
			case SIM_MESSAGE_TYPE_CREATE_ENTITY: {
				sim_message_create_entity* CreateEntity = &Message.CreateEntity;
				sim_entity_storage* SimEntityStorage = &World->SimEntities;

				if ((CreateEntity->ID.Index+1) * sizeof(sim_entity) > SimEntityStorage->Storage.CommitSize) {
					Commit_New_Size(&SimEntityStorage->Storage, (CreateEntity->ID.Index+1) * sizeof(sim_entity));
					SimEntityStorage->Capacity = SimEntityStorage->Storage.CommitSize / sizeof(sim_entity);
				}

				sim_entity* Entity = SimEntityStorage->Entities + CreateEntity->ID.Index;
				Entity->IsAllocated = true;
				Entity->ID = CreateEntity->ID;
				Entity->Type = CreateEntity->Type;
				Entity->Position = CreateEntity->Position;
				Entity->Orientation = CreateEntity->Orientation;
				Entity->Scale = CreateEntity->Scale;
			} break;
		}
	}
}

function void Sim_Begin_Frame() {
	engine* Engine = Get_Engine();

	OS_Mutex_Lock(Engine->SimFrameLock);
	push_frame* Frame = Engine->FreeFrames;
	if (Frame) SLL_Pop_Front(Engine->FreeFrames);
	else Frame = Arena_Push_Struct_No_Clear(Engine->SimArena, push_frame);
	OS_Mutex_Unlock(Engine->SimFrameLock);
	Memory_Clear(Frame, sizeof(push_frame));
	Assert(Frame->CmdCount == 0);

	Engine->CurrentFrame = Frame;
}

function void Sim_End_Frame() {
	engine* Engine = Get_Engine();

	push_frame* CurrentFrame = Engine->CurrentFrame;
	CurrentFrame->Time = Engine->SimTime;

	OS_Mutex_Lock(Engine->SimFrameLock);
	SLL_Push_Back(Engine->FirstFrame, Engine->LastFrame, CurrentFrame);
	Time_Increment(&Engine->SimTime, SIM_HZ);
	OS_Mutex_Unlock(Engine->SimFrameLock);

	Engine->CurrentFrame = NULL;
}

function void Sim_Push_Entity(sim_entity* SimEntity) {
	engine* Engine = Get_Engine();
	push_frame* PushFrame = Engine->CurrentFrame;
	Assert(PushFrame);

	Assert(PushFrame->CmdCount < MAX_PUSH_CMD_COUNT);

	push_cmd* Cmd = PushFrame->Cmds + PushFrame->CmdCount++;
	Cmd->ID = SimEntity->ID;
	Cmd->Position = SimEntity->Position;
	Cmd->Orientation = SimEntity->Orientation;
}

function world* World_Create(string WorldName) {
	heap* Heap = Heap_Create();
	world* World = Heap_Alloc_Struct(Heap, world);
	World->Heap = Heap;
	World->Name = String_Copy((allocator*)World->Heap, WorldName);
	World->Entities = Pool_Init(sizeof(entity));
	World->SimEntities.Storage = Make_Memory_Reserve(GB(1));
	World->SimEntities.Entities = (sim_entity*)World->SimEntities.Storage.BaseAddress;
	World->SimMessageQueue.Lock = OS_Mutex_Create();
	return World;
}