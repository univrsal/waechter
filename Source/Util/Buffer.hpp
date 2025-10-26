#pragma once

#include <cstddef>
#include <cstring>
#include <vector>
#include <cassert>

class WBuffer
{
	std::vector<char> Data{};
	std::size_t       ReadPos{};
	std::size_t       WritePos{};

public:
	explicit WBuffer(std::size_t Size = 1024)
		: Data(Size)
	{
	}

	~WBuffer() = default;

	[[nodiscard]] std::size_t GetSize() const
	{
		return Data.size();
	}

	[[nodiscard]] std::size_t GetReadPos() const
	{
		return ReadPos;
	}

	void SetWritingPos(std::size_t Pos)
	{
		WritePos = Pos;
	}

	[[nodiscard]] std::size_t GetWritePos() const
	{
		return WritePos;
	}

	[[nodiscard]] bool HasDataToRead() const
	{
		return ReadPos < WritePos;
	}

	std::size_t Read(char* Buf, size_t Len)
	{
		size_t BytesToRead = Len;
		if (ReadPos + Len >= Data.size())
		{
			BytesToRead = Data.size() - ReadPos;
		}

		if (BytesToRead > 0)
		{
			memcpy(Buf, Data.data() + ReadPos, BytesToRead);
			ReadPos += BytesToRead;
		}

		return BytesToRead;
	}

	void Write(const char* Buf, size_t BytesToWrite)
	{
		if (WritePos + BytesToWrite > GetSize())
		{
			Resize(GetSize() * 2);
		}

		if (BytesToWrite > 0)
		{
			memcpy(Data.data() + WritePos, Buf, BytesToWrite);
			WritePos += BytesToWrite;
		}
	}

	void Reset()
	{
		ReadPos = 0;
		WritePos = 0;
	}

	void Clear()
	{
		Reset();
		memset(Data.data(), 0, GetSize());
	}

	void Resize(size_t NewSize)
	{
		assert(NewSize < 0xffff); // arbitrary but just to catch wrap arounds
		Data.resize(NewSize);
	}

	char* GetData()
	{
		return Data.data();
	}

	[[nodiscard]] char const* GetData() const
	{
		return Data.data();
	}

	template <typename T>
	void Write(T const& Value)
	{
		Write(reinterpret_cast<char const*>(&Value), sizeof(T));
	}

	template <typename T>
	bool Read(T& Value)
	{
		return Read(reinterpret_cast<char*>(&Value), sizeof(T)) == sizeof(T);
	}

	template <typename T>
	T* Read()
	{
		T* Value = reinterpret_cast<T*>(Data.data() + ReadPos);
		ReadPos += sizeof(T);
		return Value;
	}
};
