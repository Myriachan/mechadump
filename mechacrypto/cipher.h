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

#ifndef __CIPHER_H__
#define __CIPHER_H__

#include <memory.h>
#include <stdlib.h>
#include <inttypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

int cipherCbcEncrypt(uint8_t *Result, const uint8_t *Data, size_t Length, 
                     const uint8_t *Keys, int KeyCount, const uint8_t IV[8]);
int cipherCbcDecrypt(uint8_t *Result, const uint8_t *Data, size_t Length,
                     const uint8_t *Keys, int KeyCount,	const uint8_t IV[8]);

#ifdef __cplusplus
}
#endif

#endif
