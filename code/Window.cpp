#include "Precompiled.h"
#include "Device.h"
#include "Window.h"
#include "imgui/imgui.h"


LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	Window* window = (Window*)GetWindowLongPtr(hWnd, 0);
	if (window && window->m_onWndProcCallback)
		if (window->m_onWndProcCallback(hWnd, message, wParam, lParam))
			return true;

	InputEvent inputEvent;
	inputEvent.event = ~0u;
	switch (message)
	{
		case WM_SIZE:
			window->OnResize((uint32_t)(UINT64)lParam & 0xFFFF, (UINT)(uint32_t)lParam >> 16);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		case WM_KEYDOWN:
		case WM_KEYUP:
			inputEvent.event = message;
			inputEvent.key = (uint32_t)wParam;
			window->m_inputState.keyPressed[wParam] = message == WM_KEYDOWN;
			break;

		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_LBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MBUTTONUP:
		case WM_LBUTTONUP: {
			inputEvent.event = message;
			POINT cursorPos;
			GetCursorPos(&cursorPos);
			inputEvent.cursorX = (uint16_t)cursorPos.x;
			inputEvent.cursorY = (uint16_t)cursorPos.y;

			InputState& inputState = window->m_inputState;
			switch (message)
			{
				case WM_RBUTTONDOWN:
					inputState.keyPressed[VK_RBUTTON] = true;
					inputState.mouseState |= kMouseRButton;
					SetCapture(hWnd);
					break;
				case WM_MBUTTONDOWN:
					inputState.keyPressed[VK_MBUTTON] = true;
					inputState.mouseState |= kMouseMButton;
					SetCapture(hWnd);
					break;
				case WM_LBUTTONDOWN:
					inputState.keyPressed[VK_LBUTTON] = true;
					inputState.mouseState |= kMouseLButton;
					SetCapture(hWnd);
					break;
				case WM_RBUTTONUP:
					inputState.keyPressed[VK_RBUTTON] = false;
					inputState.mouseState &= ~kMouseRButton;
					ReleaseCapture();
					break;
				case WM_MBUTTONUP:
					inputState.keyPressed[VK_MBUTTON] = false;
					inputState.mouseState &= ~kMouseMButton;
					ReleaseCapture();
					break;
				case WM_LBUTTONUP:
					inputState.keyPressed[VK_LBUTTON] = false;
					inputState.mouseState &= ~kMouseLButton;
					ReleaseCapture();
					break;
				default:
					break;
			}
			break;
		}

		case WM_CAPTURECHANGED:
			if ((HWND)lParam != hWnd)
			{
				InputState& inputState = window->m_inputState;
				if (inputState.mouseState)
				{
					inputState.mouseState = 0;
					inputState.keyPressed[VK_RBUTTON] = false;
					inputState.keyPressed[VK_MBUTTON] = false;
					inputState.keyPressed[VK_LBUTTON] = false;
					ReleaseCapture();
				}
			}
			break;

		case WM_MOUSEWHEEL:
			inputEvent.event = message;
			inputEvent.mouseWheel = HIWORD(wParam);
			break;

		case WM_MOUSEMOVE:
			POINT cursorPos;
			GetCursorPos(&cursorPos);
			window->m_inputState.cursorX = (uint16_t)cursorPos.x;
			window->m_inputState.cursorY = (uint16_t)cursorPos.y;
			// TODO event
			break;
	}

	if (inputEvent.event != ~0u)
		window->m_inputEvents.push(inputEvent);

	return DefWindowProc(hWnd, message, wParam, lParam);
}


bool Window::Init(Device* device, uint32_t width, uint32_t height, const wchar_t* name)
{
	PIXScopedEvent(0, "Window::Init");
	m_device = device;

	WCHAR szExePath[MAX_PATH];
	GetModuleFileName(nullptr, szExePath, MAX_PATH);
	HINSTANCE hInstance = GetModuleHandle(szExePath);
	HICON hIcon = ExtractIcon(hInstance, szExePath, 0);

	// Register class
	WNDCLASSEX wcex;
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.cbClsExtra = 0;
	wcex.cbWndExtra = sizeof(uintptr_t);
	wcex.hInstance = hInstance;
	wcex.hIcon = hIcon;
	wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wcex.lpszMenuName = nullptr;
	wcex.lpszClassName = name;
	wcex.hIconSm = hIcon;
	if (RegisterClassEx(&wcex) == 0)
	{
		LogStdErr("Failed to register window '%s'", name);
		return false;
	}

	// Create window
	RECT rc = {0, 0, (LONG)width, (LONG)height};
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

	m_hwnd = CreateWindow(name, name, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance,
	                      nullptr);

	SetWindowLongPtr(m_hwnd, 0, (LONG_PTR)this);
	ShowWindow(m_hwnd, SW_SHOWDEFAULT);

	RECT rect;
	::GetClientRect(m_hwnd, &rect);

	BOOL allowTearing = false;
	HRESULT hr = device->GetDXGIFactory()->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
	m_tearingSupport = SUCCEEDED(hr) && allowTearing;

	m_swapChainDesc.Width = rect.right;
	m_swapChainDesc.Height = rect.bottom;
	m_swapChainDesc.Format = kFormat;
	m_swapChainDesc.Scaling = DXGI_SCALING_NONE;
	m_swapChainDesc.SampleDesc.Quality = 0;
	m_swapChainDesc.SampleDesc.Count = 1;
	m_swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	m_swapChainDesc.BufferCount = 3;
	m_swapChainDesc.Flags = m_tearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
	m_swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;

	ComPtr<IDXGISwapChain1> swapChain;
	if (!m_device->CreateSwapChain(m_swapChainDesc, m_hwnd, &m_swapChain))
		return false;

	m_buffers.resize(m_swapChainDesc.BufferCount);
	m_rtv.resize(m_swapChainDesc.BufferCount);
	for (uint32_t i = 0; i < m_swapChainDesc.BufferCount; ++i)
	{
		hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_buffers[i]));
		if (FAILED(hr))
		{
			LogStdErr("IDXGISwapChain3::GetBuffer failed: %x\n", hr);
			return false;
		}

		m_rtv[i] = m_device->CreateRTV(m_buffers[i], nullptr);
	}

	return true;
}


void Window::Shutdown()
{
	for (uint32_t i = 0; i < m_buffers.size(); i++)
	{
		m_device->DestroyRTV(m_rtv[i]);
		SafeRelease(m_buffers[i]);
	}
	SafeRelease(m_swapChain);
}


void Window::SetOnResizeCallback(const OnResizeCallback& callback)
{
	m_onResizeCallback = callback;
}


void Window::SetOnWndProcCallback(const OnWndProcCallback& callback)
{
	m_onWndProcCallback = callback;
}


bool Window::PeekAndDispatchMessages()
{
	MSG msg = {};
	if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if (msg.message == WM_QUIT)
			return false;
	}

	return true;
}


RECT Window::GetClientRect() const
{
	RECT rect;
	::GetClientRect(m_hwnd, &rect);
	return rect;
}


float Window::GetAspectRatio() const
{
	RECT rect = GetClientRect();
	return (float)rect.right / (float)rect.bottom;
}


ID3D12Resource* Window::GetCurrentBuffer() const
{
	uint32_t bufIdx = m_swapChain->GetCurrentBackBufferIndex();
	return m_buffers[bufIdx];
}


RTVHandle Window::GetCurrentRTV() const
{
	uint32_t bufIdx = m_swapChain->GetCurrentBackBufferIndex();
	return m_rtv[bufIdx];
}


void Window::Present(uint32_t interval)
{
	PIXScopedEvent(0xffffff00, "Present buf=%u", m_swapChain->GetCurrentBackBufferIndex());
	uint32_t flags = m_tearingSupport && interval == 0 ? DXGI_PRESENT_ALLOW_TEARING : 0;
	m_swapChain->Present(interval, flags);
}


void Window::OnResize(uint32_t newWidth, uint32_t newHeight)
{
	if (!m_swapChain || (newWidth == 0 && newHeight == 0))
		return;

	m_device->GetCommandQueue().WaitForIdle();

	// buffers must be released before resize
	for (uint32_t i = 0; i < m_swapChainDesc.BufferCount; ++i)
		SafeRelease(m_buffers[i]);

	HRESULT hr = m_swapChain->ResizeBuffers(m_swapChainDesc.BufferCount, newWidth, newHeight, m_swapChainDesc.Format, m_swapChainDesc.Flags);
	Assert(SUCCEEDED(hr));

	for (uint32_t i = 0; i < m_swapChainDesc.BufferCount; ++i)
	{
		SafeRelease(m_buffers[i]);

		hr = m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_buffers[i]));
		if (FAILED(hr))
		{
			LogStdErr("IDXGISwapChain3::GetBuffer failed: %x\n", hr);
			Assert(false);
			return;
		}

		m_device->DestroyRTV(m_rtv[i]);
		m_rtv[i] = m_device->CreateRTV(m_buffers[i], nullptr);
	}

	if (m_onResizeCallback)
		m_onResizeCallback(newWidth, newHeight);
}