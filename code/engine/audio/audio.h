#ifndef AUDIO_H
#define AUDIO_H

#define SOUND_NUM_CHANNELS 2
#define SOUND_SAMPLES_PER_SEC 44100

struct sound_samples {
	u64  SampleCount;
	s16* LeftChannel;
	s16* RightChannel;
};

struct audio_manager {
	
};

#endif