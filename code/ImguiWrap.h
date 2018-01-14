#pragma once

class Window;
class Device;
struct ImDrawData;

class ImguiWrap
{
public:
	bool Init(Window& window, Device& device);
	void Shutdown();

	void OnNewFrame();
	void RenderDrawData();

	bool WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

	bool WantMouseAndKeyboard() const;

private:
	struct FrameResources
	{
		ID3D12Resource* indexBuffer = nullptr;
		ID3D12Resource* vertexBuffer = nullptr;
		int indexBufferSize = 0;
		int vertexBufferSize = 0;

		bool Prepare(ID3D12Device5* d3dDevice, ImDrawData* draw_data);
		void Shutdown();
	};

	Window* m_window = nullptr;
	Device* m_device = nullptr;

	ID3D12PipelineState* m_pso = nullptr;
	ID3D12Resource* m_fonstTexture = nullptr;
	SRVHandle m_fontsTextureSRV;
	FrameResources m_frameResources[Device::kFramesNum];

	bool CreatePSO();
	bool CreateFontsTextures();
	void SetupRenderState(ImDrawData* draw_data);
};