#pragma once
#include "FilePath.h"

class File
{
public:
	enum EOpenMode
	{
		kOpenRead = 0,
		kOpenWrite = 1
	};

	File() = delete;
	File(const char* filename, EOpenMode mode);
	File(const wchar_t* filename, EOpenMode mode);
	~File();

	bool IsOpened() const;
	uint32_t GetSize() const;

	void SetPos(uint32_t pos) const;
	uint32_t GetPos() const;
	uint32_t Read(void* buffer, uint32_t bytesToRead) const;
	uint32_t Write(const void* buffer, uint32_t bytesToWrite) const;

private:
	FILE* m_file = nullptr;
	uint32_t m_size = 0;
	EOpenMode m_openMode = kOpenRead;
};


inline bool File::IsOpened() const
{
	return m_file != nullptr;
}


inline uint32_t File::GetSize() const
{
	return m_size;
}