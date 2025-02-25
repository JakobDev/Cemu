#pragma once
#include "Common/precompiled.h"
#include <boost/nowide/convert.hpp>

#ifdef _WIN32

class FileStream
{
public:
	static FileStream* openFile(std::string_view path)
	{
		HANDLE hFile = CreateFileW(boost::nowide::widen(path.data(), path.size()).c_str(), FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		if (hFile == INVALID_HANDLE_VALUE)
			return nullptr;
		return new FileStream(hFile);
	}

	static FileStream* openFile(const wchar_t* path, bool allowWrite = false)
	{
		HANDLE hFile = CreateFileW(path, allowWrite ? (FILE_GENERIC_READ | FILE_GENERIC_WRITE) : FILE_GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
		if (hFile == INVALID_HANDLE_VALUE)
			return nullptr;
		return new FileStream(hFile);
	}

	static FileStream* openFile2(const fs::path& path, bool allowWrite = false)
	{
		return openFile(path.generic_wstring().c_str(), allowWrite);
	}

	static FileStream* createFile(const wchar_t* path)
	{
		HANDLE hFile = CreateFileW(path, FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, 0);
		if (hFile == INVALID_HANDLE_VALUE)
			return nullptr;
		return new FileStream(hFile);
	}

	static FileStream* createFile(std::string_view path)
	{
		auto w = boost::nowide::widen(path.data(), path.size());
		HANDLE hFile = CreateFileW(w.c_str(), FILE_GENERIC_READ | FILE_GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, 0, 0);
		if (hFile == INVALID_HANDLE_VALUE)
			return nullptr;
		return new FileStream(hFile);
	}

	static FileStream* createFile2(const fs::path& path)
	{
		return createFile(path.generic_wstring().c_str());
	}
	
	// helper function to load a file into memory
	static std::optional<std::vector<uint8>> LoadIntoMemory(const fs::path& path)
	{
		FileStream* fs = openFile2(path);
		if (!fs)
			return std::nullopt;
		uint64 fileSize = fs->GetSize();
		if(fileSize > 0xFFFFFFFFull)
		{
			delete fs;
			return std::nullopt;
		}
		std::optional<std::vector<uint8>> v(fileSize);
		if (fs->readData(v->data(), (uint32)fileSize) != (uint32)fileSize)
		{
			delete fs;
			return std::nullopt;
		}
		delete fs;
		return v;
	}

	// size and seek
	void SetPosition(uint64 pos)
	{
		LONG posHigh = (LONG)(pos >> 32);
		LONG posLow = (LONG)(pos);
		SetFilePointer(m_hFile, posLow, &posHigh, FILE_BEGIN);
	}

	uint64 GetSize()
	{
		DWORD fileSizeHigh = 0;
		DWORD fileSizeLow = 0;
		fileSizeLow = GetFileSize(m_hFile, &fileSizeHigh);
		return ((uint64)fileSizeHigh << 32) | (uint64)fileSizeLow;
	}

	bool SetEndOfFile()
	{
		return ::SetEndOfFile(m_hFile) != 0;
	}

	// reading
	void extract(std::vector<uint8>& data)
	{
		DWORD fileSize = GetFileSize(m_hFile, nullptr);
		data.resize(fileSize);
		SetFilePointer(m_hFile, 0, 0, FILE_BEGIN);
		DWORD bt;
		ReadFile(m_hFile, data.data(), fileSize, &bt, nullptr);
	}

	uint32 readData(void* data, uint32 length)
	{
		DWORD bytesRead = 0;
		ReadFile(m_hFile, data, length, &bytesRead, NULL);
		return bytesRead;
	}

	bool readU64(uint64& v)
	{
		return readData(&v, sizeof(uint64)) == sizeof(uint64);
	}

	bool readU32(uint32& v)
	{
		return readData(&v, sizeof(uint32)) == sizeof(uint32);
	}

	bool readU8(uint8& v)
	{
		return readData(&v, sizeof(uint8)) == sizeof(uint8);
	}

	bool readLine(std::string& line)
	{
		line.clear();
		uint8 c;
		bool isEOF = true;
		while (readU8(c))
		{
			isEOF = false;
			if(c == '\r')
				continue;
			if (c == '\n')
				break;
			line.push_back((char)c);
		}
		return !isEOF;
	}

	// writing (binary)
	sint32 writeData(const void* data, sint32 length)
	{
		DWORD bytesWritten = 0;
		WriteFile(m_hFile, data, length, &bytesWritten, NULL);
		return bytesWritten;
	}

	void writeU64(uint64 v)
	{
		writeData(&v, sizeof(uint64));
	}

	void writeU32(uint32 v)
	{
		writeData(&v, sizeof(uint32));
	}

	void writeU8(uint8 v)
	{
		writeData(&v, sizeof(uint8));
	}

	// writing (strings)
	void writeStringFmt(const char* format, ...)
	{
		char buffer[2048];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		writeData(buffer, (sint32)strlen(buffer));
	}

	void writeString(const char* str)
	{
		writeData(str, (sint32)strlen(str));
	}

	void writeLine(const char* str)
	{
		writeData(str, (sint32)strlen(str));
		writeData("\r\n", 2);
	}

	~FileStream()
	{
		if(m_isValid)
			CloseHandle(m_hFile);
	}

	FileStream() {};

private:
	FileStream(HANDLE hFile)
	{
		m_hFile = hFile;
		m_isValid = true;
	}

	bool m_isValid{};
	HANDLE m_hFile;
};

#else

#include <fstream>

class FileStream
{
public:
	static FileStream* openFile(std::string_view path)
	{
		return openFile2(path, false);
	}

	static FileStream* openFile(const wchar_t* path, bool allowWrite = false)
	{
		return openFile2(path, allowWrite);
	}

	static FileStream* openFile2(const fs::path& path, bool allowWrite = false)
	{
		//return openFile(path.generic_wstring().c_str(), allowWrite);
		FileStream* fs = new FileStream(path, true, allowWrite);
		if (fs->m_isValid)
			return fs;
		delete fs;
		return nullptr;
	}

	static FileStream* createFile(const wchar_t* path)
	{
		return createFile2(path);
	}

	static FileStream* createFile(std::string_view path)
	{
		return createFile2(path);
	}

	static FileStream* createFile2(const fs::path& path)
	{
		FileStream* fs = new FileStream(path, false, false);
		if (fs->m_isValid)
			return fs;
		delete fs;
		return nullptr;
	}

	// helper function to load a file into memory
	static std::optional<std::vector<uint8>> LoadIntoMemory(const fs::path& path)
	{
		FileStream* fs = openFile2(path);
		if (!fs)
			return std::nullopt;
		uint64 fileSize = fs->GetSize();
		if (fileSize > 0xFFFFFFFFull)
		{
			delete fs;
			return std::nullopt;
		}
		std::optional<std::vector<uint8>> v(fileSize);
		if (fs->readData(v->data(), (uint32)fileSize) != (uint32)fileSize)
		{
			delete fs;
			return std::nullopt;
		}
		delete fs;
		return v;
	}

	// size and seek
	void SetPosition(uint64 pos)
	{
		cemu_assert(m_isValid);
		if (m_prevOperationWasWrite)
			m_fileStream.seekp((std::streampos)pos);
		else
			m_fileStream.seekg((std::streampos)pos);
	}

	uint64 GetSize()
	{
		cemu_assert(m_isValid);
		auto currentPos = m_fileStream.tellg();
		m_fileStream.seekg(0, std::ios::end);
		auto fileSize = m_fileStream.tellg();
		m_fileStream.seekg(currentPos, std::ios::beg);
		uint64 fs = (uint64)fileSize;
		return fs;
	}

	bool SetEndOfFile()
	{
		assert_dbg();
		return true;
		//return ::SetEndOfFile(m_hFile) != 0;
	}

	// reading
	void extract(std::vector<uint8>& data)
	{
		uint64 fileSize = GetSize();
		SetPosition(0);
		data.resize(fileSize);
		readData(data.data(), fileSize);
	}

	uint32 readData(void* data, uint32 length)
	{
		SyncReadWriteSeek(false);
		m_fileStream.read((char*)data, length);
		size_t bytesRead = m_fileStream.gcount();
		return (uint32)bytesRead;
	}

	bool readU64(uint64& v)
	{
		return readData(&v, sizeof(uint64)) == sizeof(uint64);
	}

	bool readU32(uint32& v)
	{
		return readData(&v, sizeof(uint32)) == sizeof(uint32);
	}

	bool readU8(uint8& v)
	{
		return readData(&v, sizeof(uint8)) == sizeof(uint8);
	}

	bool readLine(std::string& line)
	{
		line.clear();
		uint8 c;
		bool isEOF = true;
		while (readU8(c))
		{
			isEOF = false;
			if (c == '\r')
				continue;
			if (c == '\n')
				break;
			line.push_back((char)c);
		}
		return !isEOF;
	}

	// writing (binary)
	sint32 writeData(const void* data, sint32 length)
	{
		SyncReadWriteSeek(true);
		m_fileStream.write((const char*)data, length);
		return length;
	}

	void writeU64(uint64 v)
	{
		writeData(&v, sizeof(uint64));
	}

	void writeU32(uint32 v)
	{
		writeData(&v, sizeof(uint32));
	}

	void writeU8(uint8 v)
	{
		writeData(&v, sizeof(uint8));
	}

	// writing (strings)
	void writeStringFmt(const char* format, ...)
	{
		char buffer[2048];
		va_list args;
		va_start(args, format);
		vsnprintf(buffer, sizeof(buffer), format, args);
		writeData(buffer, (sint32)strlen(buffer));
	}

	void writeString(const char* str)
	{
		writeData(str, (sint32)strlen(str));
	}

	void writeLine(const char* str)
	{
		writeData(str, (sint32)strlen(str));
		writeData("\r\n", 2);
	}

	~FileStream()
	{
		if (m_isValid)
		{
			m_fileStream.close();
		}
		//	CloseHandle(m_hFile);
	}

	FileStream() {};

private:
	FileStream(const fs::path& path, bool isOpen, bool isWriteable)
	{
		if (isOpen)
		{
			m_fileStream.open(path, isWriteable ? (std::ios_base::in | std::ios_base::out | std::ios_base::binary) : (std::ios_base::in | std::ios_base::binary));
			m_isValid = m_fileStream.is_open();
		}
		else
		{
			m_fileStream.open(path, std::ios_base::in | std::ios_base::out | std::ios_base::binary | std::ios_base::trunc);
			m_isValid = m_fileStream.is_open();
		}
	}

	void SyncReadWriteSeek(bool nextOpIsWrite)
	{
		// nextOpIsWrite == false -> read. Otherwise write
		if (nextOpIsWrite == m_prevOperationWasWrite)
			return;
		if (nextOpIsWrite)
			m_fileStream.seekp(m_fileStream.tellg(), std::ios::beg);
		else
			m_fileStream.seekg(m_fileStream.tellp(), std::ios::beg);

		m_prevOperationWasWrite = nextOpIsWrite;
	}

	bool m_isValid{};
	std::fstream m_fileStream;
	bool m_prevOperationWasWrite{false};
}; 

#endif