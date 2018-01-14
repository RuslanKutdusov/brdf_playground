#include "Precompiled.h"
#include "ShaderCompiler.h"

#pragma comment(lib, "dxcompiler.lib")


static const char* kShaderTypeStr[kShaderTypesCount] = {"vertex", "geometry", "pixel"};


class DxcIncludeHandler : public IDxcIncludeHandler
{
public:
	ULONG STDMETHODCALLTYPE AddRef() override
	{
		return ++refCount;
	}

	ULONG STDMETHODCALLTYPE Release() override
	{
		return --refCount;
	}

	DxcIncludeHandler(IDxcLibrary* dxcLibrary) : refCount(0), dxcLibrary(dxcLibrary)
	{
		dxcLibrary->CreateIncludeHandler(&defaultHandler);
	}

	~DxcIncludeHandler()
	{
		if (defaultHandler)
			defaultHandler->Release();
	}

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** ppvObject) override
	{
		return defaultHandler->QueryInterface(iid, ppvObject);
	}

	HRESULT STDMETHODCALLTYPE LoadSource(_In_ LPCWSTR pFilename, _COM_Outptr_ IDxcBlob** ppIncludeSource) override
	{
		FilePathW fullPath(L"data\\shaders\\");
		fullPath /= pFilename;
		File file(fullPath.c_str(), File::kOpenRead);
		if (!file.IsOpened())
			return E_INVALIDARG;

		std::unique_ptr<char[]> source(new char[file.GetSize()]);
		if (file.Read(source.get(), file.GetSize()) != file.GetSize())
			return E_INVALIDARG;

		dxcLibrary->CreateBlobWithEncodingOnHeapCopy(source.get(), file.GetSize(), CP_UTF8, (IDxcBlobEncoding**)ppIncludeSource);

		return S_OK;
	}

private:
	volatile ULONG refCount = 0;
	IDxcLibrary* dxcLibrary = nullptr;
	IDxcIncludeHandler* defaultHandler = nullptr;
};


struct DxcCompiler
{
	DxcCompiler()
	{
		HRESULT hr = DxcCreateInstance(CLSID_DxcLibrary, __uuidof(IDxcLibrary), (void**)library.GetAddressOf());
		if (FAILED(hr))
		{
			LogStdErr("DxcCreateInstance failed");
			return;
		}

		hr = DxcCreateInstance(CLSID_DxcCompiler, __uuidof(IDxcCompiler), (void**)compiler.GetAddressOf());
		if (FAILED(hr))
		{
			LogStdErr("DxcCreateInstance failed");
			return;
		}

#if _DEBUG
		arguments.push_back(L"-Zi");
		arguments.push_back(L"-Qembed_debug");
#endif
		arguments.push_back(L"-Zpr");
		arguments.push_back(L"-HV");
		arguments.push_back(L"2018");

		includeHandler = new DxcIncludeHandler(library.Get());
	}

	ComPtr<IDxcLibrary> library;
	ComPtr<IDxcCompiler> compiler;
	std::vector<const wchar_t*> arguments;
	DxcIncludeHandler* includeHandler = nullptr;
};


static DxcCompiler GCompiler;


IDxcBlob* CompileShader(const wchar_t* filename, EShaderType type, const std::vector<const wchar_t*>& defines)
{
	PIXScopedEvent(0, "CompileShader '%S'", filename);

	FilePathW fullFilename = L"data\\shaders";
	fullFilename /= filename;
	File file(fullFilename.c_str(), File::kOpenRead);
	if (!file.IsOpened())
		return nullptr;

	std::unique_ptr<char[]> source(new char[file.GetSize()]);
	file.Read(source.get(), file.GetSize());

	HRESULT hr;
	ComPtr<IDxcBlobEncoding> sourceBlob;
	hr = GCompiler.library->CreateBlobWithEncodingFromPinned(source.get(), file.GetSize(), CP_UTF8, sourceBlob.GetAddressOf());
	if (FAILED(hr))
	{
		LogStdErr("Failed to compile %s shader '%S': failed encode to utf8, error code = %x\n", kShaderTypeStr[type], filename, hr);
		return nullptr;
	}

	const wchar_t* entryPoint = nullptr;
	const wchar_t* profile = nullptr;
	if (type == kShaderVertex)
	{
		entryPoint = L"vs_main";
		profile = L"vs_6_2";
	}
	else if (type == kShaderGeometry)
	{
		entryPoint = L"gs_main";
		profile = L"gs_6_2";
	}
	else if (type == kShaderPixel)
	{
		entryPoint = L"ps_main";
		profile = L"ps_6_2";
	}
	else if (type == kShaderCompute)
	{
		entryPoint = L"cs_main";
		profile = L"cs_6_2";
	}

	DxcDefine* dxcDefines = nullptr;
	if (!defines.empty())
	{
		dxcDefines = (DxcDefine*)alloca(defines.size() * sizeof(DxcDefine));
		for (size_t i = 0; i < defines.size(); i++)
		{
			dxcDefines[i].Name = defines[i];
			dxcDefines[i].Value = L"1";
		}
	}

	ComPtr<IDxcOperationResult> operationResult;
	hr = GCompiler.compiler->Compile(sourceBlob.Get(), filename, entryPoint, profile, GCompiler.arguments.data(), (UINT)GCompiler.arguments.size(), dxcDefines,
	                                 (UINT32)defines.size(), GCompiler.includeHandler, &operationResult);
	if (FAILED(hr))
	{
		LogStdErr("Failed to compile %s shader '%S': IDxcCompiler::Compile failed, error code = %x\n", kShaderTypeStr[type], filename, hr);
		return nullptr;
	}

	operationResult->GetStatus(&hr);
	ComPtr<IDxcBlobEncoding> errorMsgs;
	operationResult->GetErrorBuffer((IDxcBlobEncoding**)errorMsgs.GetAddressOf());
	ComPtr<IDxcBlobEncoding> errorMsgsUtf8;
	GCompiler.library->GetBlobAsUtf8(errorMsgs.Get(), (IDxcBlobEncoding**)errorMsgsUtf8.GetAddressOf());
	char* err = (char*)errorMsgsUtf8->GetBufferPointer();
	size_t errLen = errorMsgsUtf8->GetBufferSize();
	if (SUCCEEDED(hr))
	{
		if (errLen)
			LogStdErr("Warnings in %s shader '%S': %s\n", kShaderTypeStr[type], filename, err);

		IDxcBlob* blob = nullptr;
		hr = operationResult->GetResult(&blob);
		Assert(SUCCEEDED(hr));
		return blob;
	}
	else
	{
		if (errLen)
			LogStdErr("Failed to compile %s shader '%S': %s\n", kShaderTypeStr[type], filename, err);
	}
	return nullptr;
}