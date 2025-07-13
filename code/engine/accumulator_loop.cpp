function void Accumulator_Loop_Start(accumulator_loop* AccumulatorLoop, f64 dtTarget) {
	AccumulatorLoop->ClockFrequency = OS_Query_Performance_Frequency();
	AccumulatorLoop->dtTarget = dtTarget;
	AccumulatorLoop->dtPerFrameTicks = (u64)(AccumulatorLoop->dtTarget*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->VeryCloseTicks = (u64)(0.0002*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->MaxTicks = (u64)((1.0 / 3.0)*(f64)AccumulatorLoop->ClockFrequency);

	//Big giant list of common refresh rates seen in the wild (can grow if I find other rates)
	AccumulatorLoop->CommonTicks[0]  = (u64)((1.0/540.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[1]  = (u64)((1.0/500.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[2]  = (u64)((1.0/390.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[3]  = (u64)((1.0/380.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[4]  = (u64)((1.0/360.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[5]  = (u64)((1.0/300.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[6]  = (u64)((1.0/280.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[7]  = (u64)((1.0/275.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[8]  = (u64)((1.0/260.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[9]  = (u64)((1.0/250.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[10] = (u64)((1.0/240.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[11] = (u64)((1.0/220.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[12] = (u64)((1.0/192.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[13] = (u64)((1.0/180.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[14] = (u64)((1.0/175.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[15] = (u64)((1.0/170.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[16] = (u64)((1.0/165.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[17] = (u64)((1.0/160.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[18] = (u64)((1.0/150.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[19] = (u64)((1.0/144.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[20] = (u64)((1.0/138.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[21] = (u64)((1.0/120.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[22] = (u64)((1.0/100.0)*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[23] = (u64)((1.0/90.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[24] = (u64)((1.0/86.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[25] = (u64)((1.0/85.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[26] = (u64)((1.0/80.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[27] = (u64)((1.0/77.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[28] = (u64)((1.0/76.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[29] = (u64)((1.0/75.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[30] = (u64)((1.0/72.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[31] = (u64)((1.0/70.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[32] = (u64)((1.0/63.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[33] = (u64)((1.0/61.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[34] = (u64)((1.0/60.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[35] = (u64)((1.0/50.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[36] = (u64)((1.0/48.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[37] = (u64)((1.0/30.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[38] = (u64)((1.0/25.0 )*(f64)AccumulatorLoop->ClockFrequency);
	AccumulatorLoop->CommonTicks[39] = (u64)((1.0/24.0 )*(f64)AccumulatorLoop->ClockFrequency);

	AccumulatorLoop->LastClockTime = OS_Query_Performance_Counter();
}

function void Accumulator_Loop_Increment(accumulator_loop* AccumulatorLoop) {
	u64 ClockTime = OS_Query_Performance_Counter();
	AccumulatorLoop->DeltaTicks = ClockTime - AccumulatorLoop->LastClockTime;
	AccumulatorLoop->LastClockTime = ClockTime;

	//Snap to one of the refresh rates if our timing is very close to one of the refresh rates
	for (u32 i = 0; i < Array_Count(AccumulatorLoop->CommonTicks); i++) {
		if ((u64)Abs((s64)AccumulatorLoop->DeltaTicks - (s64)AccumulatorLoop->CommonTicks[i]) < AccumulatorLoop->VeryCloseTicks) {
			AccumulatorLoop->DeltaTicks = AccumulatorLoop->CommonTicks[i];
			break;
		}
	}

	if (AccumulatorLoop->DeltaTicks >= AccumulatorLoop->MaxTicks) {
		AccumulatorLoop->DeltaTicks = AccumulatorLoop->MaxTicks;
	}

	AccumulatorLoop->AccumulatorTicks += AccumulatorLoop->DeltaTicks;
}

function inline b32 Accumulator_Loop_Should_Update(accumulator_loop* AccumulatorLoop) {
	bool Result = AccumulatorLoop->AccumulatorTicks >= AccumulatorLoop->dtPerFrameTicks;
	return Result;
}

function inline b32 Accumulator_Loop_Update(accumulator_loop* AccumulatorLoop) {
	b32 Result = Accumulator_Loop_Should_Update(AccumulatorLoop);
	if (Result) {
		AccumulatorLoop->AccumulatorTicks -= AccumulatorLoop->dtPerFrameTicks;
	}
	return Result;
}

function inline f64 Accumulator_Loop_Get_DT(accumulator_loop* AccumulatorLoop) {
	f64 Result = (f64)AccumulatorLoop->DeltaTicks / (f64)AccumulatorLoop->ClockFrequency;
	return Result;
}

function inline f64 Accumulator_Loop_Get_Interp(accumulator_loop* AccumulatorLoop) {
	f64 tInterp = ((f64)AccumulatorLoop->AccumulatorTicks/(f64)AccumulatorLoop->ClockFrequency) / AccumulatorLoop->dtTarget;
	return tInterp;
}