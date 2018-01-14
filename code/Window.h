#pragma once
#include "Input.h"

class Window
{
public:
	static const DXGI_FORMAT kFormat = DXGI_FORMAT_B8G8R8A8_UNORM;

	using OnResizeCallback = std::function<void(uint32_t, uint32_t)>;
	using OnWndProcCallback = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;

	bool Init(Device* device, uint32_t width, uint32_t height, const wchar_t* name);
	void Shutdown();

	void SetOnResizeCallback(const OnResizeCallback& callback);
	void SetOnWndProcCallback(const OnWndProcCallback& callback);

	bool PeekAndDispatchMessages();
	bool FetchInputEvent(InputEvent* event);
	void ClearInputEventsQueue();
	const InputState& GetInputState() const;

	HWND GetHWND() const;
	RECT GetClientRect() const;
	float GetAspectRatio() const;
	bool IsMinimized() const;

	ID3D12Resource* GetCurrentBuffer() const;
	RTVHandle GetCurrentRTV() const;
	void Present(uint32_t interval);

private:
	static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
	void OnResize(uint32_t newWidth, uint32_t newHeight);

	HWND m_hwnd = 0;
	std::queue<InputEvent> m_inputEvents;
	InputState m_inputState;

	Device* m_device = nullptr;
	DXGI_SWAP_CHAIN_DESC1 m_swapChainDesc = {};
	bool m_tearingSupport = false;
	IDXGISwapChain3* m_swapChain = nullptr;
	// array of back buffers
	std::vector<ID3D12Resource*> m_buffers;
	// RTV descriptors to back buffers
	std::vector<RTVHandle> m_rtv;

	OnResizeCallback m_onResizeCallback;
	OnWndProcCallback m_onWndProcCallback;
};


inline bool Window::FetchInputEvent(InputEvent* event)
{
	if (m_inputEvents.empty())
		return false;

	*event = m_inputEvents.front();
	m_inputEvents.pop();
	return true;
}


inline void Window::ClearInputEventsQueue()
{
	while (!m_inputEvents.empty())
		m_inputEvents.pop();
}


inline const InputState& Window::GetInputState() const
{
	return m_inputState;
}


inline HWND Window::GetHWND() const
{
	return m_hwnd;
}


inline bool Window::IsMinimized() const
{
	return IsIconic(m_hwnd);
}