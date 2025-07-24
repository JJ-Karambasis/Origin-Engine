function void OS_Add_Event(os_event_type Type, void* Data) {
	engine* Engine = Get_Engine();
	os_event_queue* Queues[] = { &Engine->UpdateOSEvents, &Engine->SimOSEvents };

	for (u32 i = 0; i < Array_Count(Queues); i++) {
		os_event_queue* Queue = Queues[i];

		OS_Mutex_Lock(Queue->Lock);
		u32 EntryToWrite = Queue->EntryToWrite;
		Queue->EntryToWrite = (Queue->EntryToWrite + 1) % OS_EVENT_QUEUE_COUNT;

		//todo: Should we block if we don't have any space? Is this even reasonably possible?
		Assert(Queue->EntryToWrite != Queue->EntryToRead);

		os_event_entry* Event = Queue->Queue + EntryToWrite;
		Event->Type = Type;

		switch (Type) {
			case OS_EVENT_TYPE_KEYBOARD_DOWN:
			case OS_EVENT_TYPE_KEYBOARD_UP: {
				Event->Keyboard = *(os_keyboard_event *)Data;
			} break;

			case OS_EVENT_TYPE_MOUSE_DOWN:
			case OS_EVENT_TYPE_MOUSE_UP: {
				Event->Mouse = *(os_mouse_event *)Data;
			} break;

			case OS_EVENT_TYPE_MOUSE_MOVE: 
			case OS_EVENT_TYPE_MOUSE_DELTA: {
				Event->MouseMove = *(os_mouse_move_event*)Data;
			} break;

			case OS_EVENT_TYPE_MOUSE_SCROLL: {
				Event->MouseScroll = *(os_mouse_scroll_event*)Data;
			} break;

			case OS_EVENT_TYPE_CHAR: {
				Event->Char = *(os_char_event*)Data;
			} break;
		}

		OS_Mutex_Unlock(Queue->Lock);
	}
}

function b32 OS_Get_Event(os_event_queue* Queue, os_event_entry* Event) {
	OS_Mutex_Lock(Queue->Lock);
	u32 EntryToRead = Queue->EntryToRead;
	b32 IsEmpty = EntryToRead == Queue->EntryToWrite;
	if (!IsEmpty) {
		*Event = Queue->Queue[EntryToRead];
		Queue->EntryToRead = (Queue->EntryToRead + 1) % OS_EVENT_QUEUE_COUNT;
	}
	OS_Mutex_Unlock(Queue->Lock);
	return !IsEmpty;
}