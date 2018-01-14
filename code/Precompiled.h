#pragma once
#define WINVER _WIN32_WINNT_WIN10
#include <windows.h>
#include <WinUser.h>
#include <wrl.h>

#include <d3d12.h>
#include <dxgi1_6.h>

#include <vector>
#include <queue>
#include <algorithm>
#include <functional>

#include "Log.h"

#include "DirectXTex.h"
using namespace DirectX;

#include "File.h"

#include "Device.h"
#include "Helpers.h"
