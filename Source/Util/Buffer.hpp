/*
 * Copyright (c) 2025, Alex <uni@vrsal.xyz>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <cstddef>
#include <cstring>
#include <vector>
#include <cassert>
#include <algorithm>
#include <limits>

class WBuffer
{
	std::vector<char> Data{};
	std::size_t       ReadPos{};
	std::size_t       WritePos{};

	void EnsureCapacity(std::size_t Needed)
	{
		// grow to at least 'needed' bytes
		if (Needed <= Data.size())
			return;

		std::size_t NewCap = !Data.empty() ? Data.size() : static_cast<std::size_t>(1024);
		// grow geometrically to avoid frequent reallocations
		while (NewCap < Needed)
		{
			if (NewCap > (std::numeric_limits<std::size_t>::max)() / 2)
			{
				NewCap = Needed; // last jump to exactly needed to avoid overflow
				break;
			}
			NewCap *= 2;
		}
		Data.resize(NewCap);
	}

public:
	explicit WBuffer(std::size_t Size = 1024) : Data(Size) {}

	~WBuffer() = default;

	[[nodiscard]] std::size_t GetSize() const { return Data.size(); }
	[[nodiscard]] std::size_t GetReadPos() const { return ReadPos; }
	[[nodiscard]] std::size_t GetWritePos() const { return WritePos; }
	[[nodiscard]] bool        HasDataToRead() const { return ReadPos < WritePos; }

	// Number of bytes available to read (unconsumed)
	[[nodiscard]] std::size_t GetReadableSize() const { return WritePos - ReadPos; }
	// Pointer to next readable byte (do not write through it)
	[[nodiscard]] char const* PeekReadPtr() const { return Data.data() + ReadPos; }
	// Consume N bytes from the readable range
	void Consume(std::size_t N)
	{
		std::size_t Adv = std::min(N, GetReadableSize());
		ReadPos += Adv;
		if (ReadPos == WritePos)
		{
			// fully consumed; reset to start to avoid growth
			Reset();
		}
	}
	// Optionally compact buffer by moving unread data to the start
	void Compact()
	{
		if (ReadPos == 0 || ReadPos >= WritePos)
			return;
		size_t Remaining = WritePos - ReadPos;
		std::memmove(Data.data(), Data.data() + ReadPos, Remaining);
		ReadPos = 0;
		WritePos = Remaining;
	}

	void SetWritingPos(std::size_t Pos)
	{
		// allow moving within current size, grow if needed
		if (Pos > Data.size())
			EnsureCapacity(Pos);
		WritePos = Pos;
		if (ReadPos > WritePos)
			ReadPos = WritePos;
	}

	std::size_t Read(char* Buf, std::size_t Len)
	{
		if (Buf == nullptr || Len == 0)
			return 0;

		if (ReadPos > WritePos) // invariant guard
			ReadPos = WritePos;

		std::size_t const Available = WritePos - ReadPos;
		std::size_t const N = std::min(Len, Available);

		if (N > 0)
		{
			std::memcpy(Buf, Data.data() + ReadPos, N);
			ReadPos += N;
		}
		return N;
	}

	void Write(char const* Buf, std::size_t BytesToWrite)
	{
		if (Buf == nullptr || BytesToWrite == 0)
			return;

		// overflow guard on addition
		if (BytesToWrite > (std::numeric_limits<std::size_t>::max)() - WritePos)
		{
			assert(false && "WBuffer::Write overflow");
			return;
		}

		std::size_t const Needed = WritePos + BytesToWrite;
		EnsureCapacity(Needed);

		std::memcpy(Data.data() + WritePos, Buf, BytesToWrite);
		WritePos += BytesToWrite;
	}

	void Reset()
	{
		ReadPos = 0;
		WritePos = 0;
	}

	void Clear()
	{
		// optional zeroing (not required for correctness)
		std::memset(Data.data(), 0, Data.size());
		Reset();
	}

	void Resize(std::size_t NewSize)
	{
		Data.resize(NewSize);
		if (WritePos > NewSize)
			WritePos = NewSize;
		if (ReadPos > WritePos)
			ReadPos = WritePos;
	}

	char*                     GetData() { return Data.data(); }
	[[nodiscard]] char const* GetData() const { return Data.data(); }

	template <typename T>
	void Write(T const& Value)
	{
		static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
		Write(reinterpret_cast<char const*>(&Value), sizeof(T));
	}

	template <typename T>
	bool Read(T& Value)
	{
		static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");
		return Read(reinterpret_cast<char*>(&Value), sizeof(T)) == sizeof(T);
	}
};