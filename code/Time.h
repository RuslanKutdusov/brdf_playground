#pragma once

namespace Time {
	void Init();
	void NewFrame();
	float GetFrameStartTimestamp();
	float GetFrameDeltaTime();
}