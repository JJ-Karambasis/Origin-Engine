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

function ENGINE_AUDIO_STREAM_DEFINE(Engine_Audio_Stream_Impl) {
	Set_Engine(Engine);
	//Write_Sine_Wave(OutSamples);
}