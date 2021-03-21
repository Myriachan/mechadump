// This implementation is not very fast; it's meant for a simple
// implementation that can be used for doing SHA-256 at compile time,
// but also has a runtime implementation available.

#include <algorithm>
#include <cassert>
#include <cstring>

#include "sha256.hpp"
#include "util.h"


void sha256::reset()
{
	std::memset(m_buffer, 0, sizeof(m_buffer));
	sha256_internal::sha256_init(m_state);
	m_size = 0;
}


void sha256::process(const void* data, std::size_t size)
{
	const unsigned char* d = static_cast<const unsigned char*>(data);

	// Try to use what's left in the buffer.
	std::size_t remaining = 64 - (m_size % 64);
	if ((remaining < 64) || (size <= remaining))
	{
		std::size_t now = std::min<std::size_t>(size, remaining);
		std::memcpy(&m_buffer[m_size % 64], d, now);
		d += now;
		m_size += now;
		size -= now;
		
		if (remaining == now)
		{
			process_block(m_buffer);
		}
	}
	
	// Handle full blocks.
	while (size >= 64)
	{
		process_block(d);
		d += 64;
		m_size += 64;
		size -= 64;
	}
	
	// Put the rest into the buffer.
	if (size > 0)
	{
		std::memcpy(m_buffer, d, size);
		d += size;
		m_size += size;
		size = 0;
	}
}


void sha256::process_block(const void* block)
{
	using namespace sha256_internal;

	const std::uint8_t* b = static_cast<const std::uint8_t*>(block);
	sha256_word input[16];

	for (int i = 0; i < 16; ++i)
	{
		input[i] = read_be_uint32(b + (i * 4));
	}

	sha256_transform(m_state, input);
}


void sha256::finish(digest& dig)
{
	std::uint64_t savedSize = m_size;
	
	process("\x80", 1);
	
	while ((m_size % 64) != 64 - sizeof(uint64_t))
	{
		process("", 1);
	}
	
	std::uint8_t beSize[sizeof(std::uint64_t)];
	write_be_uint64(beSize, savedSize * 8);
	process(beSize, sizeof(beSize));
	assert(m_size % 64 == 0);
	
	for (int i = 0; i < 8; ++i)
	{
		write_be_uint32(&dig[i * sizeof(std::uint32_t)], m_state[i]);
	}
}
