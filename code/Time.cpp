#include "Precompiled.h"
#include "Time.h"


static float GInvFrequency = 0.0f;
static uint64_t GLastFrameCounter = 0;
static float GFrameStartTimestamp = 0.0f;
static float GFrameDeltaTime = 0.0f;


void Time::Init()
{
	LARGE_INTEGER frequency;
	QueryPerformanceFrequency(&frequency);
	GInvFrequency = 1.0f / frequency.QuadPart;

	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);
	GLastFrameCounter = count.QuadPart;
}


void Time::NewFrame()
{
	LARGE_INTEGER count;
	QueryPerformanceCounter(&count);

	GFrameStartTimestamp = GInvFrequency * count.QuadPart;

	uint64_t delta = count.QuadPart - GLastFrameCounter;
	GFrameDeltaTime = GInvFrequency * delta;
	GLastFrameCounter = count.QuadPart;
}


float Time::GetFrameStartTimestamp()
{
	return GFrameStartTimestamp;
}


float Time::GetFrameDeltaTime()
{
	return GFrameDeltaTime;
}