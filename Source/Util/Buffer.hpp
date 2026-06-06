/*
 * Copyright (c) 2025-2026, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once
#include <cstddef>
#include <cstring>
#include <vector>
#include <cassert>
#include <algorithm>
#include <limits>
#include <span>
#include <type_traits>

class WBuffer
{
	std::vector<std::byte> Data{};
	std::size_t       ReadPos{};
	std::size_t       WritePos{};

	template <typename TByte>
	using TCharType = std::conditional_t<std::is_const_v<TByte>, char const, char>;

	template <typename TByte>
	static std::span<TCharType<TByte>> ByteSpanToCharSpan(std::span<TByte> Bytes)
	{
		return { reinterpret_cast<TCharType<TByte>*>(Bytes.data()), Bytes.size() };
	}

	void EnsureCapacity(std::size_t const Needed)
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
	explicit WBuffer(std::size_t const Size = 1024) : Data(Size) {}

	~WBuffer() = default;

	[[nodiscard]] std::size_t GetSize() const { return Data.size(); }
	[[nodiscard]] std::size_t GetReadPos() const { return ReadPos; }
	[[nodiscard]] std::size_t GetWritePos() const { return WritePos; }
	[[nodiscard]] bool        HasDataToRead() const { return ReadPos < WritePos; }

	// Number of bytes available to read (unconsumed)
	[[nodiscard]] std::size_t GetReadableSize() const { return WritePos - ReadPos; }

	[[nodiscard]] std::span<std::byte const> GetReadableData() const
	{
		return { Data.data() + ReadPos, GetReadableSize() };
	}
	[[nodiscard]] std::span<char const> GetReadableChars() const { return ByteSpanToCharSpan(GetReadableData()); }

	[[nodiscard]] std::span<std::byte const> GetWrittenData() const { return { Data.data(), WritePos }; }
	[[nodiscard]] std::span<char const>      GetWrittenChars() const { return ByteSpanToCharSpan(GetWrittenData()); }

	// Consume N bytes from the readable range
	void Consume(std::size_t const N)
	{
		std::size_t const Adv = std::min(N, GetReadableSize());
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
		size_t const Remaining = WritePos - ReadPos;
		std::memmove(Data.data(), Data.data() + ReadPos, Remaining);
		ReadPos = 0;
		WritePos = Remaining;
	}

	void SetWritingPos(std::size_t const Pos)
	{
		// allow moving within current size, grow if needed
		if (Pos > Data.size())
			EnsureCapacity(Pos);
		WritePos = Pos;
		if (ReadPos > WritePos)
			ReadPos = WritePos;
	}

	[[nodiscard]] std::size_t Read(std::span<std::byte> Buf)
	{
		if (Buf.empty())
			return 0;

		if (ReadPos > WritePos) // invariant guard
			ReadPos = WritePos;

		std::size_t const Available = WritePos - ReadPos;
		std::size_t const N = std::min(Buf.size(), Available);

		if (N > 0)
		{
			std::memcpy(Buf.data(), Data.data() + ReadPos, N);
			ReadPos += N;
		}
		return N;
	}

	[[nodiscard]] std::size_t Read(std::span<char> const Buf) { return Read(std::as_writable_bytes(Buf)); }

	void Write(std::span<std::byte const> const Buf)
	{
		if (Buf.empty())
			return;

		// overflow guard on addition
		if (Buf.size() > (std::numeric_limits<std::size_t>::max)() - WritePos)
		{
			assert(false && "WBuffer::Write overflow");
#if !WDEBUG
			return;
#endif
		}

		std::size_t const Needed = WritePos + Buf.size();
		EnsureCapacity(Needed);

		std::memcpy(Data.data() + WritePos, Buf.data(), Buf.size());
		WritePos += Buf.size();
	}

	void Write(std::span<char const> const Buf) { Write(std::as_bytes(Buf)); }

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

	void Resize(std::size_t const NewSize)
	{
		Data.resize(NewSize);
		if (WritePos > NewSize)
			WritePos = NewSize;
		if (ReadPos > WritePos)
			ReadPos = WritePos;
	}

	[[nodiscard]] std::span<std::byte>       GetData() { return Data; }
	[[nodiscard]] std::span<std::byte const> GetData() const { return Data; }

	template <typename T>
	void Write(T const& Value)
	{
		static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
		Write(std::as_bytes(std::span{ &Value, static_cast<std::size_t>(1) }));
	}

	template <typename T>
	bool Read(T& Value)
	{
		static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
		return Read(std::as_writable_bytes(std::span{ &Value, static_cast<std::size_t>(1) })) == sizeof(T);
	}
};