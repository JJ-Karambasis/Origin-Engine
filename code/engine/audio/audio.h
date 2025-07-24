#ifndef AUDIO_H
#define AUDIO_H

#define SOUND_NUM_CHANNELS 2
#define SOUND_SAMPLES_PER_SEC 44100

enum {
	SOUND_FLAG_NONE = 0,
	SOUND_FLAG_LOOPING = (1 << 0)
};
typedef u64 sound_flags;

struct sound_samples {
	u64  Hash;
	u64  SampleCount;
	s16* LeftChannel;
	s16* RightChannel;

	sound_samples* Next;
	sound_samples* Prev;
};

struct sound_samples_slot {
	sound_samples* First;
	sound_samples* Last;
};

struct playing_sound {
	sound_samples* Samples;
	sound_flags    Flags;
	f32 		   Volume;

	u64 PlayingIndex;
	b32 Finished;

	playing_sound* Next;
	playing_sound* Prev;
};

#define MAX_SOUND_SAMPLES_SLOTS 4096
#define SOUND_SAMPLES_SLOT_MASK (MAX_SOUND_SAMPLES_SLOTS-1)
struct audio_manager {
	arena*  Arena;

	os_rw_mutex* SlotLock;
	sound_samples_slot SamplesSlots[MAX_SOUND_SAMPLES_SLOTS];
	
	os_mutex* AllocatingLock;
	playing_sound* FreeSounds;

	os_mutex* QueueLock;
	playing_sound* QueuedSounds;

	playing_sound* FirstPlayingSound;
	playing_sound* LastPlayingSound;

	f32 Volume;
};

#endif