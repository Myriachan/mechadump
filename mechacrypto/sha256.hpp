// This implementation is not very fast; it's meant for a simple
// implementation that can be used for doing SHA-256 at compile time,
// but also has a runtime implementation available.

#pragma once

#include <cstddef>
#include <cstdint>


namespace sha256_internal
{
	using sha256_word = std::uint32_t;


	inline constexpr sha256_word sha256_shift_left(sha256_word value, unsigned shift)
	{
		if (shift >= 32u)
		{
			return 0u;
		}

		// Mask out the high bits to avoid issues with promotion and
		// compile-time overflow warnings.
		value &= sha256_word(-1) >> shift;

		return static_cast<sha256_word>(value << shift);
	}

	inline constexpr sha256_word sha256_shift_right(sha256_word value, unsigned shift)
	{
		if (shift >= 32u)
		{
			return 0u;
		}

		return static_cast<sha256_word>(value >> shift);
	}

	inline constexpr sha256_word sha256_rotate_right(sha256_word value, unsigned shift)
	{
		shift %= 32u;

		return sha256_shift_left(value, 32u - shift) | sha256_shift_right(value, shift);
	}

	inline constexpr sha256_word sha256_ch(sha256_word x, sha256_word y, sha256_word z)
	{
		return (x & y) ^ ((~x) & z);
	}

	inline constexpr sha256_word sha256_maj(sha256_word x, sha256_word y, sha256_word z)
	{
		return (x & y) ^ (x & z) ^ (y & z);
	}

	inline constexpr sha256_word sha256_ep0(sha256_word x)
	{
		return sha256_rotate_right(x, 2) ^ sha256_rotate_right(x, 13) ^ sha256_rotate_right(x, 22);
	}

	inline constexpr sha256_word sha256_ep1(sha256_word x)
	{
		return sha256_rotate_right(x, 6) ^ sha256_rotate_right(x, 11) ^ sha256_rotate_right(x, 25);
	}

	inline constexpr sha256_word sha256_sigma0(sha256_word x)
	{
		return sha256_rotate_right(x, 7) ^ sha256_rotate_right(x, 18) ^ sha256_shift_right(x, 3);
	}

	inline constexpr sha256_word sha256_sigma1(sha256_word x)
	{
		return sha256_rotate_right(x, 17) ^ sha256_rotate_right(x, 19) ^ sha256_shift_right(x, 10);
	}

	// The main implementation
	inline constexpr void sha256_transform(sha256_word(&state)[8], const sha256_word(&input)[16])
	{
		constexpr sha256_word k[64] =
		{
			0x428A2F98, 0x71374491, 0xB5C0FBCF, 0xE9B5DBA5, 0x3956C25B, 0x59F111F1, 0x923F82A4, 0xAB1C5ED5,
			0xD807AA98, 0x12835B01, 0x243185BE, 0x550C7DC3, 0x72BE5D74, 0x80DEB1FE, 0x9BDC06A7, 0xC19BF174,
			0xE49B69C1, 0xEFBE4786, 0x0FC19DC6, 0x240CA1CC, 0x2DE92C6F, 0x4A7484AA, 0x5CB0A9DC, 0x76F988DA,
			0x983E5152, 0xA831C66D, 0xB00327C8, 0xBF597FC7, 0xC6E00BF3, 0xD5A79147, 0x06CA6351, 0x14292967,
			0x27B70A85, 0x2E1B2138, 0x4D2C6DFC, 0x53380D13, 0x650A7354, 0x766A0ABB, 0x81C2C92E, 0x92722C85,
			0xA2BFE8A1, 0xA81A664B, 0xC24B8B70, 0xC76C51A3, 0xD192E819, 0xD6990624, 0xF40E3585, 0x106AA070,
			0x19A4C116, 0x1E376C08, 0x2748774C, 0x34B0BCB5, 0x391C0CB3, 0x4ED8AA4A, 0x5B9CCA4F, 0x682E6FF3,
			0x748F82EE, 0x78A5636F, 0x84C87814, 0x8CC70208, 0x90BEFFFA, 0xA4506CEB, 0xBEF9A3F7, 0xC67178F2,
		};

		sha256_word a = state[0];
		sha256_word b = state[1];
		sha256_word c = state[2];
		sha256_word d = state[3];
		sha256_word e = state[4];
		sha256_word f = state[5];
		sha256_word g = state[6];
		sha256_word h = state[7];

		sha256_word w[64]{};

		for (int i = 0; i < 16; ++i)
		{
			w[i] = input[i];
		}

		for (int i = 16; i < 64; ++i)
		{
			w[i] = sha256_sigma1(w[i - 2]) + w[i - 7] + sha256_sigma0(w[i - 15]) + w[i - 16];
		}

		for (int i = 0; i < 64; ++i)
		{
			sha256_word temp1 = h + sha256_ep1(e) + sha256_ch(e, f, g) + k[i] + w[i];
			sha256_word temp2 = sha256_ep0(a) + sha256_maj(a, b, c);

			h = g;
			g = f;
			f = e;
			e = d + temp1;
			d = c;
			c = b;
			b = a;
			a = temp1 + temp2;
		}

		state[0] += a;
		state[1] += b;
		state[2] += c;
		state[3] += d;
		state[4] += e;
		state[5] += f;
		state[6] += g;
		state[7] += h;
	}

	inline constexpr void sha256_init(sha256_word(&state)[8])
	{
		state[0] = 0x6A09E667;
		state[1] = 0xBB67AE85;
		state[2] = 0x3C6EF372;
		state[3] = 0xA54FF53A;
		state[4] = 0x510E527F;
		state[5] = 0x9B05688C;
		state[6] = 0x1F83D9AB;
		state[7] = 0x5BE0CD19;
	}
}


// A traditional SHA-256 implementation for non-compile-time.
class sha256
{
public:
	using digest = uint8_t[32];

	sha256() { reset(); }

	sha256(const sha256&) = default;
	sha256& operator =(const sha256&) = default;

	void reset();
	void process(const void* data, std::size_t size);
	void finish(digest& dig);  // can't reuse after

private:
	void process_block(const void* block);

	std::uint8_t m_buffer[64];
	sha256_internal::sha256_word m_state[8];
	std::uint64_t m_size;
};
