/*
 * ps3mca-tool - PlayStation 3 Memory Card Adaptor Software
 * Copyright (C) 2011 - jimmikaelkael <jimmikaelkael@wanadoo.fr>
 * Copyright (C) 2011 - "someone who wants to stay anonymous"
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "util.h"

void memrcpy(void *dst, void *src, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++) {
		((uint8_t *)dst)[i] = ((uint8_t *)src)[len-1-i];
	}
}

/*
 * memxor: perform exclusive-or of memory buffers.
 */
void memxor(const void *a, const void *b, void *Result, size_t Length)
{
	size_t i;
	for (i = 0; i < Length; i++) {
		((uint8_t *)Result)[i] = ((uint8_t *)a)[i] ^ ((uint8_t *)b)[i];
	}
}

/*
 * write_le_uint16: write an unsigned 16 bits Little Endian
 * value to a buffer
 */
void write_le_uint16(uint8_t *buf, uint16_t val)
{
	buf[0] = (uint8_t)val;
	buf[1] = (uint8_t)(val >> 8);
}

/*
 * write_le_uint32: write an unsigned 32 bits Little Endian
 * value to a buffer
 */
void write_le_uint32(uint8_t *buf, uint32_t val)
{
	buf[0] = (uint8_t)val;
	buf[1] = (uint8_t)(val >> 8);
	buf[2] = (uint8_t)(val >> 16);
	buf[3] = (uint8_t)(val >> 24);
}

/*
 * write_le_uint64: write an unsigned 64 bits Little Endian
 * value to a buffer
 */
void write_le_uint64(uint8_t *buf, uint64_t val)
{
	buf[0] = (uint8_t)val;
	buf[1] = (uint8_t)(val >> 8);
	buf[2] = (uint8_t)(val >> 16);
	buf[3] = (uint8_t)(val >> 24);
	buf[4] = (uint8_t)(val >> 32);
	buf[5] = (uint8_t)(val >> 40);
	buf[6] = (uint8_t)(val >> 48);
	buf[7] = (uint8_t)(val >> 56);
}

/*
 * read_le_uint16: read an unsigned 16 bits Little Endian
 * value from a buffer
 */
uint16_t read_le_uint16(const uint8_t *buf)
{
	register uint16_t val;

	val = buf[0];
	val |= ((uint16_t)buf[1] << 8);

	return val;
}

/*
 * read_le_uint32: read an unsigned 32 bits Little Endian
 * value from a buffer
 */
uint32_t read_le_uint32(const uint8_t *buf)
{
	register uint32_t val;

	val = buf[0];
	val |= ((uint32_t)buf[1] << 8);
	val |= ((uint32_t)buf[2] << 16);
	val |= ((uint32_t)buf[3] << 24);

	return val;
}

/*
 * read_le_uint64: read an unsigned 64 bits Little Endian
 * value from a buffer
 */
uint64_t read_le_uint64(const uint8_t *buf)
{
	register uint64_t val;

	val = buf[0];
	val |= ((uint64_t)buf[1] << 8);
	val |= ((uint64_t)buf[2] << 16);
	val |= ((uint64_t)buf[3] << 24);
	val |= ((uint64_t)buf[4] << 32);
	val |= ((uint64_t)buf[5] << 40);
	val |= ((uint64_t)buf[6] << 48);
	val |= ((uint64_t)buf[7] << 56);

	return val;
}

/*
 * write_be_uint16: write an unsigned 16 bits Big Endian
 * value to a buffer
 */
void write_be_uint16(uint8_t* buf, uint16_t val)
{
	buf[0] = (uint8_t)(val >> 8);
	buf[1] = (uint8_t)val;
}

/*
 * write_be_uint32: write an unsigned 32 bits Big Endian
 * value to a buffer
 */
void write_be_uint32(uint8_t* buf, uint32_t val)
{
	buf[0] = (uint8_t)(val >> 24);
	buf[1] = (uint8_t)(val >> 16);
	buf[2] = (uint8_t)(val >> 8);
	buf[3] = (uint8_t)val;
}

/*
 * write_be_uint64: write an unsigned 64 bits Big Endian
 * value to a buffer
 */
void write_be_uint64(uint8_t* buf, uint64_t val)
{
	buf[0] = (uint8_t)(val >> 56);
	buf[1] = (uint8_t)(val >> 48);
	buf[2] = (uint8_t)(val >> 40);
	buf[3] = (uint8_t)(val >> 32);
	buf[4] = (uint8_t)(val >> 24);
	buf[5] = (uint8_t)(val >> 16);
	buf[6] = (uint8_t)(val >> 8);
	buf[7] = (uint8_t)val;
}

/*
 * read_be_uint16: read an unsigned 16 bits Big Endian
 * value from a buffer
 */
uint16_t read_be_uint16(const uint8_t* buf)
{
	register uint16_t val;

	val = ((uint16_t)buf[0] << 8);
	val |= buf[1];

	return val;
}

/*
 * read_be_uint32: read an unsigned 32 bits Big Endian
 * value from a buffer
 */
uint32_t read_be_uint32(const uint8_t* buf)
{
	register uint32_t val;

	val = ((uint32_t)buf[0] << 24);
	val |= ((uint32_t)buf[1] << 16);
	val |= ((uint32_t)buf[2] << 8);
	val |= buf[3];

	return val;
}

/*
 * read_be_uint64: read an unsigned 64 bits Big Endian
 * value from a buffer
 */
uint64_t read_be_uint64(const uint8_t* buf)
{
	register uint64_t val;

	val = ((uint64_t)buf[0] << 56);
	val |= ((uint64_t)buf[1] << 48);
	val |= ((uint64_t)buf[2] << 40);
	val |= ((uint64_t)buf[3] << 32);
	val |= ((uint64_t)buf[4] << 24);
	val |= ((uint64_t)buf[5] << 16);
	val |= ((uint64_t)buf[6] << 8);
	val |= buf[7];

	return val;
}

