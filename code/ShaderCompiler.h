#pragma once
#include <dxcapi.h>


enum EShaderType
{
	kShaderVertex = 0,
	kShaderGeometry,
	kShaderPixel,
	kShaderCompute,
	kShaderTypesCount
};


IDxcBlob* CompileShader(const wchar_t* filename, EShaderType type, const std::vector<const wchar_t*>& defines);