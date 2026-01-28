/*
 * Copyright (c) 2025, Alex <uni@vrsal.cc>
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "validate.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef __linux__
	#include <sys/random.h>
#endif
#define KEY_USERNAME_CAPACITY 15
#define KEY_SECTION_COUNT 5
#define KEY_SECTION_CHARS 4
#define KEY_TOTAL_CHARS (KEY_SECTION_COUNT * KEY_SECTION_CHARS)
#define KEY_SALT_CHUNKS 4
#define KEY_PAYLOAD_CHUNKS (KEY_TOTAL_CHARS - KEY_SALT_CHUNKS)
#define VALID_KEY_CHAR_COUNT 32
typedef unsigned char keychar_t;

// hey! no peeking >:(

enum key_status
{
	KEY_STATUS_INVALID = 0,
	KEY_STATUS_VALID = 1,
};

struct keyblock
{
	keychar_t data[3];
};

struct key
{
	keychar_t username[15];
	union
	{
		struct keyblock block[5];
		keychar_t       raw[15];
	};
};

enum key_status validate_key(const struct key* k);

static char const* valid_key_chars = "ABCDEFGHJKLMNPQRSTVWXZ0123456789";
static char const* valid_user_chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ -";

int str_find_char(char const* str, char c)
{
	int index = 0;
	while (str[index] != '\0')
	{
		if (str[index] == c)
			return index;
		++index;
	}
	return -1;
}

int str_len(char const* str)
{
	int len = 0;
	while (str[len] != '\0')
		++len;
	return len;
}

static int str_copy(keychar_t* dest, char const* src, int max_len)
{
	int i = 0;
	for (; i < max_len - 1 && src[i] != '\0'; ++i)
	{
		dest[i] = (keychar_t)src[i];
	}
	dest[i] = '\0';
	return i;
}

enum key_status is_valid_string(char const* str, int len, char const* valid_chars, int iskey)
{
	for (int i = 0; i < len; ++i)
	{
		if (str[i] == '\0')
			return KEY_STATUS_INVALID;

		if (iskey)
		{
			if (i == 4 || i == 9 || i == 14 || i == 19)
			{
				if (str[i] != '-')
					return KEY_STATUS_INVALID;
				continue;
			}
		}

		char const* p = valid_chars;
		int         found = 0;
		while (*p)
		{
			if (str[i] == *p)
			{
				found = 1;
				break;
			}
			++p;
		}
		if (!found)
			return KEY_STATUS_INVALID;
	}
	return KEY_STATUS_VALID;
}

static void compute_payload_chunks(
	char const* username, keychar_t const* salt_chunks, keychar_t payload[KEY_PAYLOAD_CHUNKS])
{
	unsigned int hash = 0x811C9DC5u;
	if (!username)
		username = "";
	for (int i = 0; username[i] != '\0'; ++i)
	{
		hash ^= (unsigned char)username[i];
		hash *= 0x01000193u;
		hash += (unsigned int)i + 0x9E3779B9u;
	}
	for (int i = 0; i < KEY_SALT_CHUNKS; ++i)
	{
		hash ^= salt_chunks[i] + 0xA5u;
		hash *= 0x01000193u;
		hash += 0x7F4A7C15u + (unsigned int)i;
	}
	for (int i = 0; i < KEY_PAYLOAD_CHUNKS; ++i)
	{
		hash ^= (hash << 13);
		hash ^= (hash >> 17);
		hash ^= (hash << 5);
		payload[i] = (keychar_t)(hash & 0x1Fu);
	}
}

static int decode_chunks_from_key(const struct key* k, keychar_t chunks[KEY_TOTAL_CHARS])
{
	if (!k || !chunks)
		return 0;
	int out_index = 0;
	for (int block = 0; block < KEY_SECTION_COUNT; ++block)
	{
		keychar_t byte1 = k->block[block].data[0];
		keychar_t byte2 = k->block[block].data[1];
		keychar_t byte3 = k->block[block].data[2];

		int A = (byte1 >> 2) & 0x3F;
		int B = ((byte1 & 0x03) << 4) | ((byte2 >> 4) & 0x0F);
		int C = ((byte2 & 0x0F) << 2) | ((byte3 >> 6) & 0x03);
		int D = byte3 & 0x3F;

		if (A >= VALID_KEY_CHAR_COUNT || B >= VALID_KEY_CHAR_COUNT || C >= VALID_KEY_CHAR_COUNT
			|| D >= VALID_KEY_CHAR_COUNT)
		{
			return 0;
		}

		chunks[out_index++] = (keychar_t)A;
		chunks[out_index++] = (keychar_t)B;
		chunks[out_index++] = (keychar_t)C;
		chunks[out_index++] = (keychar_t)D;
	}
	return 1;
}

// Validates the key and the username
enum key_status validate_key(const struct key* k)
{
	if (!k)
		return KEY_STATUS_INVALID;

	int uname_len = 0;
	while (uname_len < KEY_USERNAME_CAPACITY && k->username[uname_len] != '\0')
		++uname_len;
	if (uname_len <= 0 || uname_len >= KEY_USERNAME_CAPACITY)
		return KEY_STATUS_INVALID;
	if (is_valid_string((char const*)k->username, uname_len, valid_user_chars, 0) == KEY_STATUS_INVALID)
		return KEY_STATUS_INVALID;

	keychar_t decoded_chunks[KEY_TOTAL_CHARS] = { 0 };
	if (!decode_chunks_from_key(k, decoded_chunks))
		return KEY_STATUS_INVALID;

	keychar_t salt_chunks[KEY_SALT_CHUNKS] = { 0 };
	for (int i = 0; i < KEY_SALT_CHUNKS; ++i)
		salt_chunks[i] = decoded_chunks[i];

	keychar_t expected_payload[KEY_PAYLOAD_CHUNKS] = { 0 };
	compute_payload_chunks((char const*)k->username, salt_chunks, expected_payload);

	for (int i = 0; i < KEY_PAYLOAD_CHUNKS; ++i)
	{
		if (decoded_chunks[KEY_SALT_CHUNKS + i] != expected_payload[i])
			return KEY_STATUS_INVALID;
	}
	return KEY_STATUS_VALID;
}

enum key_status parse_key(char const* key_str, struct key* k)
{
	// we have 20 characters in the key string, i.e. 5 blocks of 4 characters
	// each block of 4 characters maps to 3 bytes in the key struct
	// so we need to convert each block of 4 characters to 3 bytes
	// we merge them like this
	//     A        B        C        D
	// 000000   000000   000000   000000
	//
	// 000000   00 0000  0000 00  000000
	//     \   /    \   /      \  /
	//      \ /      \ /        \/
	//   00000000   00000000  00000000

	char clean_key_str[20];
	int  ci = 0;
	for (int i = 0; key_str[i] != '\0' && ci < 20; ++i)
	{
		if (key_str[i] != '-')
		{
			clean_key_str[ci++] = key_str[i];
		}
	}
	if (ci != 20)
		return KEY_STATUS_INVALID;
	for (int i = 0; i < 5; ++i)
	{
		int A = str_find_char(valid_key_chars, clean_key_str[i * 4 + 0]);
		int B = str_find_char(valid_key_chars, clean_key_str[i * 4 + 1]);
		int C = str_find_char(valid_key_chars, clean_key_str[i * 4 + 2]);
		int D = str_find_char(valid_key_chars, clean_key_str[i * 4 + 3]);
		if (A == -1 || B == -1 || C == -1 || D == -1)
			return KEY_STATUS_INVALID;

		keychar_t byte1 = (keychar_t)((A << 2) | (B >> 4));
		keychar_t byte2 = (keychar_t)(((B & 0x0F) << 4) | (C >> 2));
		keychar_t byte3 = (keychar_t)(((C & 0x03) << 6) | D);
		k->block[i].data[0] = byte1;
		k->block[i].data[1] = byte2;
		k->block[i].data[2] = byte3;
	}
	return KEY_STATUS_VALID;
}

void print_key(const struct key* k)
{
	printf("Username: %s\n", (char*)k->username);
	printf("Key data: ");
	for (int i = 0; i < 5; ++i)
	{
		// untangle the 3 bytes back to 4 characters
		keychar_t byte1 = k->block[i].data[0];
		keychar_t byte2 = k->block[i].data[1];
		keychar_t byte3 = k->block[i].data[2];

		int A = (byte1 >> 2) & 0x3F;
		int B = ((byte1 & 0x03) << 4) | ((byte2 >> 4) & 0x0F);
		int C = ((byte2 & 0x0F) << 2) | ((byte3 >> 6) & 0x03);
		int D = byte3 & 0x3F;
		if (A >= 32 || B >= 32 || C >= 32 || D >= 32)
		{
			printf("Invalid key data\n");
			return;
		}
		char a = valid_key_chars[A];
		char b = valid_key_chars[B];
		char c = valid_key_chars[C];
		char d = valid_key_chars[D];

		printf("%c%c%c%c", a, b, c, d);
		if (i < 4)
			printf("-");
	}
	printf("\n");
}

int is_valid_key(char const* username, char const* key)
{
	struct key k = { 0 };
	if (is_valid_string(key, str_len(key) - 1, valid_key_chars, /*iskey*/ 1) == KEY_STATUS_INVALID)
		return KEY_STATUS_INVALID;
	if (is_valid_string(username, str_len(username) - 1, valid_user_chars, /*iskey*/ 0) == KEY_STATUS_INVALID)
		return KEY_STATUS_INVALID;
	// copy username
	str_copy(k.username, username, sizeof(k.username));

	// convert key string to key struct
	if (parse_key(key, &k) == KEY_STATUS_INVALID)
		return KEY_STATUS_INVALID;
	return validate_key(&k);
}
