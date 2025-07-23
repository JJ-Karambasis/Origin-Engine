#include <stb_vorbis.c>

function void Write_Sine_Wave(sound_samples* Samples) {
	const int ToneHz = 254;
	const f32 ToneVolume = 3000;
	const int WavePeriod = SOUND_SAMPLES_PER_SEC/ToneHz;
	local f32 tSine = 0.0f;
	for (u32 i = 0; i < Samples->SampleCount; i++) {
		f32 SineValue = (f32)sin(tSine);
		s16 SampleValue = (s16)(SineValue * ToneVolume);
		Samples->LeftChannel[i] = SampleValue;
		Samples->RightChannel[i] = SampleValue;
		tSine += TAU*1.0f / (f32)WavePeriod;
		if (tSine > TAU) {
			tSine -= TAU;
		}
	}
}

function sound_samples* Load_Sound_From_File(allocator* Allocator, string FilePath) {
	sound_samples* Result = NULL;

	arena* Scratch = Scratch_Get();
	buffer Buffer = Read_Entire_File((allocator*)Scratch, FilePath);
	
	if (!Buffer_Is_Empty(Buffer)) {
		int NumChannels, SampleRate;
		short* Samples;
		int NumSamples = stb_vorbis_decode_memory(Buffer.Ptr, Buffer.Size, &NumChannels, &SampleRate, &Samples);
		if (NumSamples != -1) {
			if (NumChannels == SOUND_NUM_CHANNELS && SampleRate == SOUND_SAMPLES_PER_SEC) {
				Result = (sound_samples*)Allocator_Allocate_Memory(Allocator, sizeof(sound_samples)+(NumSamples * SOUND_NUM_CHANNELS * sizeof(s16)));
				Result->SampleCount = NumSamples;
				Result->LeftChannel = (s16*)(Result + 1);
				Result->RightChannel = Result->LeftChannel + Result->SampleCount;

				s16* LeftChannel = Result->LeftChannel;
				s16* RightChannel = Result->RightChannel;
				short* SrcSamples = Samples;
				for (int i = 0; i < NumSamples; i++) {
					*LeftChannel++ = *SrcSamples++;
					*RightChannel++ = *SrcSamples++;
				}
			}

			free(Samples);
		}
	}

	Scratch_Release();
	return Result;
}

function audio_manager* Audio_Manager_Get() {
	return &Get_Engine()->Audio;
}

function playing_sound* Playing_Sound_Allocate() {
	audio_manager* AudioManager = Audio_Manager_Get();
	OS_Mutex_Lock(AudioManager->AllocatingLock);
	playing_sound* Sound = AudioManager->FreeSounds;
	if (Sound) SLL_Pop_Front(AudioManager->FreeSounds);
	else Sound = Arena_Push_Struct_No_Clear(AudioManager->Arena, playing_sound);
	OS_Mutex_Unlock(AudioManager->AllocatingLock);
	Memory_Clear(Sound, sizeof(playing_sound));
	return Sound;
}

function void Playing_Sound_Free(playing_sound* Sound) {
	audio_manager* AudioManager = Audio_Manager_Get();
	OS_Mutex_Lock(AudioManager->AllocatingLock);
	SLL_Push_Front(AudioManager->FreeSounds, Sound);
	OS_Mutex_Unlock(AudioManager->AllocatingLock);
}

function void Queue_Playing_Sound(playing_sound* Sound) {
	audio_manager* AudioManager = Audio_Manager_Get();
	OS_Mutex_Lock(AudioManager->QueueLock);
	SLL_Push_Front(AudioManager->QueuedSounds, Sound);
	OS_Mutex_Unlock(AudioManager->QueueLock);
}

function void Add_Playing_Sounds() {
	audio_manager* AudioManager = Audio_Manager_Get();
	OS_Mutex_Lock(AudioManager->QueueLock);
	playing_sound* Sound = AudioManager->QueuedSounds;
	while (Sound) {
		playing_sound* SoundToAdd = Sound;
		Sound = Sound->Next;

		SoundToAdd->Next = NULL;
		DLL_Push_Back(AudioManager->FirstPlayingSound, AudioManager->LastPlayingSound, SoundToAdd);
	}
	AudioManager->QueuedSounds = NULL;
	OS_Mutex_Unlock(AudioManager->QueueLock);
}

function void Play_Sound(string SoundName, sound_flags Flags, f32 Volume) {
	audio_manager* AudioManager = Audio_Manager_Get();
	
	u64 Hash = U64_Hash_String(SoundName);
	u64 SlotIndex = (Hash & SOUND_SAMPLES_SLOT_MASK);
	sound_samples_slot* Slot = AudioManager->SamplesSlots + SlotIndex;

	sound_samples* Samples = NULL;
	for (sound_samples* HashSamples = Slot->First; HashSamples; HashSamples->Next) {
		if (HashSamples->Hash == Hash) {
			Samples = HashSamples;
			break;
		}
	}

	if (!Samples) {
		arena* Scratch = Scratch_Get();
		string FilePath = String_Format((allocator*)Scratch, "audio/%.*s.ogg", SoundName.Size, SoundName.Ptr);
		Samples = Load_Sound_From_File((allocator*)AudioManager->Arena, FilePath);
		Scratch_Release();

		Samples->Hash = Hash;

		if (Samples) {
			DLL_Push_Back(Slot->First, Slot->Last, Samples);
		}
	}

	if (Samples) {
		playing_sound* Sound = Playing_Sound_Allocate();
		Sound->Samples = Samples;
		Sound->Volume = Volume;
		Sound->Flags = Flags;
		Queue_Playing_Sound(Sound);
	}
}

function void Play_Simple_Sound(string SoundName) {
	Play_Sound(SoundName, 0, 1.0f);
}

function void Audio_Init(audio_manager* Manager) {
	Manager->Arena = Arena_Create();
	Manager->AllocatingLock = OS_Mutex_Create();
	Manager->QueueLock = OS_Mutex_Create();
	Manager->Volume = 1.0f;
}

function ENGINE_AUDIO_STREAM_DEFINE(Engine_Audio_Stream_Impl) {
	Set_Engine(Engine);

	Add_Playing_Sounds();

	audio_manager* AudioManager = Audio_Manager_Get();
	AudioManager->Volume = 1.0f;

	arena* Scratch = Scratch_Get();

	f32* LeftChannel = Arena_Push_Array(Scratch, OutSamples->SampleCount, f32);
	f32* RightChannel = Arena_Push_Array(Scratch, OutSamples->SampleCount, f32);

	for (playing_sound* Sound = AudioManager->FirstPlayingSound; Sound; Sound = Sound->Next) {
		for (u64 i = 0; i < OutSamples->SampleCount; i++) {
			f32 LeftSample = SNorm(Sound->Samples->LeftChannel[Sound->PlayingIndex]);
			f32 RightSample = SNorm(Sound->Samples->RightChannel[Sound->PlayingIndex]);

			LeftSample  *= (AudioManager->Volume * Sound->Volume);
			RightSample *= (AudioManager->Volume * Sound->Volume);

			LeftChannel[i] += LeftSample;
			RightChannel[i] += RightSample;
			
			Sound->PlayingIndex++;
			if (Sound->PlayingIndex >= Sound->Samples->SampleCount) {
				if (Sound->Flags & SOUND_FLAG_LOOPING) {
					Sound->PlayingIndex = 0;
				} else {
					Sound->Finished = true;
					break;
				}
			}
		}
	}

	for (u64 i = 0; i < OutSamples->SampleCount; i++) {
		OutSamples->LeftChannel[i] = SNorm_S16(LeftChannel[i]);
		OutSamples->RightChannel[i] = SNorm_S16(RightChannel[i]);
	}

	playing_sound* Sound = AudioManager->FirstPlayingSound;
	while (Sound) {
		playing_sound* SoundToCheck = Sound;
		Sound = Sound->Next;

		if (SoundToCheck->Finished) {
			DLL_Remove(AudioManager->FirstPlayingSound, AudioManager->LastPlayingSound, SoundToCheck);
			Playing_Sound_Free(SoundToCheck);
		}
	}

	Scratch_Release();
}