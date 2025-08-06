function void Sim_Pause() {
	simulation* Simulation = Simulation_Get();
	OS_Event_Reset(Simulation->WaitEvent);
	Atomic_Store_B32(&Simulation->IsSimulating, false);
	OS_Event_Wait(Simulation->WaitEvent);
}

function void Sim_Resume() {
	simulation* Simulation = Simulation_Get();
	Atomic_Store_B32(&Simulation->IsSimulating, true);
}

function void Sim_Add_Message(sim_message_type Type, void* Data) {
	sim_message_queue* Queue = &Simulation_Get()->MessageQueue; 

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

function b32 Sim_Get_Message(sim_message* Message) {
	sim_message_queue* Queue = &Simulation_Get()->MessageQueue; 
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
	simulation* Simulation = Simulation_Get();

	sim_message Message = {};
	while (Sim_Get_Message(&Message)) {
		switch (Message.Type) {
			case SIM_MESSAGE_TYPE_CREATE_ENTITY: {
				sim_message_create_entity* CreateEntity = &Message.CreateEntity;
				sim_entity_storage* SimEntityStorage = &Simulation->Entities;

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
				Entity->Collider = CreateEntity->Collider;
				Entity->BodyType = CreateEntity->BodyType;
				Entity->Friction = CreateEntity->Friction;

				Entity->Body = Simulation_Create_Body(Simulation, {
					.Position = Entity->Position,
					.Orientation = Entity->Orientation,
					.Scale = Entity->Scale,
					.Collider = Entity->Collider,
					.BodyType = Entity->BodyType,
					.Friction = Entity->Friction,
					.UserData = Entity
				});
			} break;
		}
	}
}

function void Sim_Begin_Frame() {
	simulation* Simulation = Simulation_Get();

	OS_Mutex_Lock(Simulation->FrameLock);
	sim_push_frame* Frame = Simulation->FreeFrames;
	if (Frame) SLL_Pop_Front(Simulation->FreeFrames);
	else {
		Frame = Arena_Push_Struct_No_Clear(Simulation->Arena, sim_push_frame);
		Frame->Arena = Arena_Create();
	}
	OS_Mutex_Unlock(Simulation->FrameLock);
	Arena_Clear(Frame->Arena);
	Memory_Clear(Offset_Pointer(Frame, sizeof(arena*)), sizeof(sim_push_frame)-sizeof(arena*));
	Assert(Frame->CmdCount == 0);

	Simulation->CurrentFrame = Frame;
}

function void Sim_End_Frame() {
	simulation* Simulation = Simulation_Get();

	sim_push_frame* CurrentFrame = Simulation->CurrentFrame;
	CurrentFrame->Time = Simulation->Time;

	OS_Mutex_Lock(Simulation->FrameLock);
	SLL_Push_Back(Simulation->FirstFrame, Simulation->LastFrame, CurrentFrame);
	Time_Increment(&Simulation->Time, SIM_HZ);
	OS_Mutex_Unlock(Simulation->FrameLock);

	Simulation->CurrentFrame = NULL;
}

function void Sim_Push_Entity(sim_entity* SimEntity) {
	simulation* Simulation = Simulation_Get();
	sim_push_frame* PushFrame = Simulation->CurrentFrame;
	Assert(PushFrame);

	Assert(PushFrame->CmdCount < MAX_SIM_PUSH_CMD_COUNT);

	sim_push_cmd* Cmd = PushFrame->Cmds + PushFrame->CmdCount++;
	Cmd->ID = SimEntity->ID;
	Cmd->Position = SimEntity->Position;
	Cmd->Orientation = SimEntity->Orientation;
}

function void Sim_Init(simulation* Simulation) {
	Simulation->Arena = Arena_Create();
	Simulation->WaitEvent = OS_Event_Create();
	Simulation->FrameLock = OS_Mutex_Create();

	Simulation->Entities.Storage = Make_Memory_Reserve(GB(1));
	Simulation->Entities.Entities = (sim_entity*)Simulation->Entities.Storage.BaseAddress;
	Simulation->MessageQueue.Lock = OS_Mutex_Create();

	Simulation_Init_Backend(Simulation);

	Simulation_Set_Gravity(Simulation, V3(0.0f, 0.0f, -9.8f));
}

#if defined(SIMULATION_FLOW)
#include "backend/flow_simulation.cpp"
#endif

#if defined(SIMULATION_JOLT)
#include "backend/jolt_simulation.cpp"
#endif