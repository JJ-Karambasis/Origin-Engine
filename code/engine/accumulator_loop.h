#ifndef ACCUMULATOR_LOOP_H
#define ACCUMULATOR_LOOP_H

struct accumulator_loop {
	u64 ClockFrequency;
	f64 dtTarget;
	u64 dtPerFrameTicks;
	u64 VeryCloseTicks;
	u64 MaxTicks;

	u64 LastClockTime;
	u64 DeltaTicks;
	u64 AccumulatorTicks;

	u64 CommonTicks[40];
};

#endif