#include "Precompiled.h"
#include "File.h"


File::File(const char* filename, EOpenMode mode)
{
	char modeStr[32] = {};
	if (mode == kOpenRead)
		strcat(modeStr, "r");
	else if (mode == kOpenWrite)
		strcat(modeStr, "w");
	strcat(modeStr, "b");

	m_file = fopen(filename, modeStr);
	if (!m_file)
		return;

	fseek(m_file, 0, SEEK_END);
	m_size = ftell(m_file);
	fseek(m_file, 0, SEEK_SET);
}


File::File(const wchar_t* filename, EOpenMode mode)
{
	wchar_t modeStr[32] = {};
	if (mode == kOpenRead)
		wcscat(modeStr, L"r");
	else if (mode == kOpenWrite)
		wcscat(modeStr, L"w");
	wcscat(modeStr, L"b");

	m_file = _wfopen(filename, modeStr);
	if (!m_file)
		return;

	fseek(m_file, 0, SEEK_END);
	m_size = ftell(m_file);
	fseek(m_file, 0, SEEK_SET);
}


File::~File()
{
	if (m_file)
		fclose(m_file);
}


void File::SetPos(uint32_t pos) const
{
	fseek(m_file, pos, SEEK_SET);
}


uint32_t File::GetPos() const
{
	return ftell(m_file);
}


uint32_t File::Read(void* buffer, uint32_t bytesToRead) const
{
	if (!m_file)
		return ~0u;

	return (uint32_t)fread(buffer, 1, bytesToRead, m_file);
}


uint32_t File::Write(const void* buffer, uint32_t bytesToWrite) const
{
	if (!m_file)
		return ~0u;

	return (uint32_t)fwrite(buffer, 1, bytesToWrite, m_file);
}
