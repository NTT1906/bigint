#ifndef _BIGINT_H_
#define _BIGINT_H_
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <future>
#include <string>
#include <random>
#include <cctype>
#include <cstring>
#include <iostream>
#include <vector>

typedef uint32_t u32;
typedef uint64_t u64;

// #if !defined(DEBUG) && !defined(NDEBUG)
// #define NDEBUG
// #endif

// MACRO DETAIL:
// BI_BIT: fixed size of bigint in bit
// BI_N: size of bigint in limb (1 limb = u32 = 32 bit)
// BI_FORCE_UNROLL: force some loop to unroll when optimize
// BI_UNROLL_THRESHOLD: unroll threshold
// BI_UNROLL(n): unroll pragma
// BI_NFORCE_UNROLL: force not to unroll

#define BI_SU32 sizeof(u32) // 4
#define BI_SBU32 32

#ifndef BI_BIT
#define BI_BIT 512
#endif
#ifndef BI_N
#define BI_N (BI_BIT / 32)
#endif

#define BI_2N (BI_N * 2)

static_assert(BI_BIT > 0 && BI_BIT % 32 == 0, "BI_BIT must be positive and divisible by 32");

#define BI_FORCE_UNROLL
#ifdef BI_FORCE_UNROLL
#ifndef BI_UNROLL_THRESHOLD
#define BI_UNROLL_THRESHOLD 16
#endif
#if defined(_MSC_VER)
#define BI_DO_PRAGMA(x) __pragma(x)
#define BI_UNROLL(n) BI_DO_PRAGMA(loop(unroll, n))
#elif defined(__clang__)
#define BI_DO_PRAGMA(x) _Pragma(#x)
#define BI_UNROLL(n) BI_DO_PRAGMA(clang loop unroll_count(n))
#elif defined(__GNUC__)
#define BI_DO_PRAGMA(x) _Pragma(#x)
#define BI_UNROLL(n) BI_DO_PRAGMA(GCC unroll n)
#else
#define BI_DO_PRAGMA(x)
#define BI_UNROLL(n)
#endif
#endif

#ifdef BI_NFORCE_UNROLL
#define BI_UNROLL(n)
#endif

#if defined(_MSC_VER)
#define BI_ALWAYS_INLINE __forceinline
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#define BI_ALWAYS_INLINE inline __attribute__((always_inline))
#else
#define BI_ALWAYS_INLINE inline
#endif

// Hardware intrinsics setup
#if defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_IX86))
// MSVC on x86/x64
#include <intrin.h>
#define BI_USE_HW_INTRIN 1
#elif (defined(__GNUC__) || defined(__clang__)) && (defined(__x86_64__) || defined(__i386__))
// GCC or Clang on x86/x64
#include <x86intrin.h>
#define BI_USE_HW_INTRIN 1
#else
// Fallback for ARM (Apple Silicon), embedded, or unknown architectures
#define BI_USE_HW_INTRIN 0
#endif

#if !defined(_MSC_VER) && defined(__cpp_constexpr)
#define BI_OP_CONSTEXPR constexpr
#else
#define BI_OP_CONSTEXPR
#endif

#ifdef BI_FORCE_NO_USE_HW_INTRIN
#define BI_USE_HW_INTRIN 0
#endif

// big endian: data[0] = MSW
// eg: assign 1 to bui: a[BI_N - 1] = 1;
// eg: assign 0x12345678'9ABCDEF0'11223344'55667788 to bui
// a[BI_N - 1] = 0x55667788u;
// a[BI_N - 2] = 0x11223344u;
// a[BI_N - 3] = 0x9ABCDEF0u;
// a[BI_N - 4] = 0x12345678u;
struct bui {
	std::array<u32, BI_N> limbs{};
	u32& operator[](const size_t i) { return limbs[i]; }
	const u32& operator[](const size_t i) const { return limbs[i]; }

	u32* data() { return limbs.data(); }
	const u32* data() const { return limbs.data(); }
	auto begin() { return limbs.begin(); }
	auto begin() const { return limbs.begin(); }
	auto end() { return limbs.end(); }
	auto end() const { return limbs.end(); }
	auto size() const { return limbs.size(); }

	BI_OP_CONSTEXPR static bui zero() { return {}; }
	BI_OP_CONSTEXPR static bui one() {
		bui r{}; r.limbs[BI_N - 1] = 1;
		return r;
	}
	BI_OP_CONSTEXPR static bui from_u32(const u32 x) {
		bui r{}; r.limbs[BI_N - 1] = x;
		return r;
	}
};

struct bul {
	std::array<u32, BI_N * 2> limbs{};
	bul() = default;
	bul(const bui& high, const bui& low) {
		std::copy_n(high.begin(), BI_N, limbs.begin());
		std::copy_n(low.begin(), BI_N, limbs.begin() + BI_N);
	}

	bui& high() { return *reinterpret_cast<bui*>(limbs.data()); }
	bui& low() { return *reinterpret_cast<bui*>(limbs.data() + BI_N); }
	const bui& high() const { return *reinterpret_cast<const bui*>(&limbs[0]); }
	const bui& low() const { return *reinterpret_cast<const bui*>(&limbs[BI_N]); }

	BI_ALWAYS_INLINE u32& operator[](const size_t i) { return limbs[i]; }
	BI_ALWAYS_INLINE const u32& operator[](const size_t i) const { return limbs[i]; }
	u32* data() { return limbs.data(); }
	const u32* data() const { return limbs.data(); }
	auto begin() { return limbs.begin(); }
	auto begin() const { return limbs.begin(); }
	auto end() { return limbs.end(); }
	auto end() const { return limbs.end(); }
	auto size() const { return limbs.size(); }

	BI_OP_CONSTEXPR static bul zero() { return {}; }
	BI_OP_CONSTEXPR static bul one() {
		bul r{}; r.limbs[BI_N * 2 - 1] = 1;
		return r;
	}
	BI_OP_CONSTEXPR static bul from_u32(const u32 x) {
		bul r{}; r.limbs[BI_N * 2 - 1] = x;
		return r;
	}
};

// -------------------- compile-time safety checks --------------------
static_assert(std::is_trivially_copyable_v<bui>);
static_assert(std::is_trivially_copyable_v<bul>);
static_assert(sizeof(bui) == BI_SU32 * BI_N);
static_assert(sizeof(bul) == BI_SU32 * BI_N * 2);

struct MontgomeryReducer;

std::string bui_to_dec(const bui& x);
std::string bui_to_hex(const bui &a, bool uppercase, bool split);
bui bui_from_dec(const std::string& s);
bui bui_from_hex(const std::string& s);
bul bui_to_bul(const bui& x);
bul bul_from_2bui(const bui& high, const bui& low);
bui bul_high(const bul& x);
bui bul_low(const bul& x);

u32 get_bit(u32 num, u32 pos);
u32 set_bit(u32 num, u32 pos, u32 val);
u32 get_bit(const bui &a, u32 pos);
void set_bit_ip(bui &a, u32 pos, u32 val);
void set_bit_ip(bul &a, u32 pos, u32 val);
bui set_bit(bui a, u32 pos, u32 val);

u32 highest_bit(u32 x);
u32 highest_bit(const bui &x);
u32 highest_bit(const bul &x);
u32 highest_limb(const bui &x);
u32 highest_limb(const bul &x);

void bitwise_and_ip(bui &a, const bui &b);
void bitwise_or_ip(bui &a, const bui &b);
void bitwise_xor_ip(bui &a, const bui &b);

void shift_limb_left(bui &x, u32 l);
void shift_limb_right(bui &x, u32 l);
void shift_limb_left(bul &x, u32 l);
void shift_limb_right(bul &x, u32 l);

void shift_left_ip(bui& x, u32 k);
void shift_left_ip(bul& x, u32 k);
void shift_right_ip(bui& x, u32 k);
void shift_right_ip(bul& x, u32 k);
bui shift_left(bui x, u32 k);
bul shift_left_expand(bui x, u32 k);
bui shift_left_mod(bui x, u32 k, const bui& m);
bui shift_left_mod_bulk(bui x, u32 k, const bui& m);

bool bui_is0(const bui& x);
bool bul_is0(const bul& x);

int cmp(const bui& a, const bui& b);
int cmp(const bul& a, const bul& b);
int cmp(const bul& a, const bui& b);
void add_ip(bui& a, const bui& b);
void add_ip(bul& a, const bul& b);
void sub_ip(bui& a, const bui& b);
void add_mod_ip(bui& a, const bui &b, const bui &m);
void sub_mod_ip(bui &a, const bui &b, const bui &m);
bui mod_native(bui x, const bui &m);
bui mod_native(bul x, const bui &m);
bui nmod_native(bui x, const bui &m);
bui nmod_native(bul x, const bui &m);
void divmod_knuth(const bui &a, const bui& b, bui& quot, bui& rem);
void divmod_knuth2(const bui &a, const bui& b, bui& quot, bui& rem);

void mul_mod_ip(bui &a, bui b, const bui &m);
void mul_ref(const bui &a, const bui &b, bul &r);
bui bui_pow2(u32 k);
bul bul_pow2(u32 k);
bui bui_binary_flood1(u32 k);
bul bul_binary_flood1(u32 k);

u32 dbl_ip_n_imp(u32* x, u32 n);
void dbl_ip(bui &x);
void dbl_ip(bul &x);

u32 u32_divmod_bul(const bul &a, u32 d, bul &q);
void u32_divmod(const bui &a, u32 b, bui &q, u32 &r);
void u32_divmod(const bul &a, u32 b, bul &q, u32 &r);
u32 u32_mod(bui x, u32 m);
u32 u32_mod(bul x, u32 m);

BI_ALWAYS_INLINE bui bui0() { return bui::zero(); }
BI_ALWAYS_INLINE bui bui1() { return bui::one(); }
BI_ALWAYS_INLINE bui bui_from_u32(const u32 x) { return bui::from_u32(x); }
BI_ALWAYS_INLINE bul bul0() { return bul::zero(); }
BI_ALWAYS_INLINE bul bul1() { return bul::one(); }
BI_ALWAYS_INLINE bul bul_from_u32(const u32 x) { return bul::from_u32(x); }

inline u32 get_bit(const u32 num, const u32 pos) { return num >> pos & 1; }

inline u32 set_bit(const u32 num, const u32 pos, const u32 val) {
	if (pos >= 32) return num;
	u32 mask = (u32)1 << pos;
	return (num & ~mask) | (val & 1u ? mask : 0u);
}

inline u32 get_bit(const bui &a, const u32 pos) {
	assert(pos < BI_N * BI_SBU32);
	u32 k = BI_N - 1 - pos / BI_SBU32;
	return get_bit(a[k], pos % BI_SBU32);
}

// set in-place
inline void set_bit_ip(bui &a, const u32 pos, const u32 val) {
	assert(pos < BI_N * BI_SBU32 && "Cannot set bit outside the scope of the big integer");
	u32 k = BI_N - 1 - pos / BI_SBU32;
	a[k] = set_bit(a[k], pos % 32, val);
}

inline void set_bit_ip(bul &a, const u32 pos, const u32 val) {
	assert(pos < BI_N * 2 * BI_SBU32 && "Cannot set bit outside the scope of the big integer");
	u32 k = BI_N * 2 - 1 - pos / BI_SBU32;
	a[k] = set_bit(a[k], pos % 32, val);
}

inline bui set_bit(bui a, const u32 pos, const u32 val) {
	set_bit_ip(a, pos, val);
	return a;
}

inline u32 highest_bit(u32 x) {
#if defined(__GNUC__) || defined(__clang__)
	if (x == 0) return 0;
	return BI_SBU32 - __builtin_clz(x); // GCC fallback
#elif defined(_MSC_VER) && defined(BI_USE_HW_INTRIN)
	return BI_SBU32 - __lzcnt(x);
#elif defined(_MSC_VER)
	unsigned long idx;
	if (_BitScanReverse(&idx, x)) return static_cast<u32>(idx + 1);
	return 0;
#else
	u32 pos = 0;
	if (x >= (1u << 16)) { x >>= 16; pos += 16; }
	if (x >= (1u << 8))  { x >>= 8;  pos += 8;  }
	if (x >= (1u << 4))  { x >>= 4;  pos += 4;  }
	if (x >= (1u << 2))  { x >>= 2;  pos += 2;  }
	if (x >= (1u << 1))  {           pos += 1;  }
	return pos + 1;
#endif
}

inline u32 highest_bit(const bui &x) {
	u32 i = 0;
	for (; i + 3 < BI_N; i += 4) {
		if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
			if (x[i  ]) return highest_bit(x[i  ]) + (BI_N - i - 1) * BI_SBU32;
			if (x[i+1]) return highest_bit(x[i+1]) + (BI_N - i - 2) * BI_SBU32;
			if (x[i+2]) return highest_bit(x[i+2]) + (BI_N - i - 3) * BI_SBU32;
			/* x[i+3] */return highest_bit(x[i+3]) + (BI_N - i - 4) * BI_SBU32;
		}
	}
	for (; i < BI_N; ++i)
		if (x[i] != 0)
			return highest_bit(x[i]) + (BI_N - i - 1) * BI_SBU32;
	return 0; // all limbs zero
}

inline u32 highest_bit(const bul &x) {
	u32 i = 0;
	for (; i + 3 < BI_N * 2; i += 4) {
		if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
			if (x[i  ]) return highest_bit(x[i  ]) + (BI_N * 2 - i - 1) * BI_SBU32;
			if (x[i+1]) return highest_bit(x[i+1]) + (BI_N * 2 - i - 2) * BI_SBU32;
			if (x[i+2]) return highest_bit(x[i+2]) + (BI_N * 2 - i - 3) * BI_SBU32;
			/* x[i+3] */return highest_bit(x[i+3]) + (BI_N * 2 - i - 4) * BI_SBU32;
		}
	}
	for (; i < BI_N * 2; ++i)
		if (x[i] != 0)
			return highest_bit(x[i]) + (BI_N * 2 - i - 1) * BI_SBU32;
	return 0; // all limbs zero
}

inline void bitwise_and_ip(bui &a, const bui &b) {
	for (u32 i = BI_N; i-- > 0;)
		a[i] &= b[i];
}

inline void bitwise_or_ip(bui &a, const bui &b) {
	for (u32 i = BI_N; i-- > 0;)
		a[i] |= b[i];
}

inline void bitwise_xor_ip(bui &a, const bui &b) {
	for (u32 i = BI_N; i-- > 0;)
		a[i] ^= b[i];
}

template <u32 n>
BI_ALWAYS_INLINE u32 highest_limb_template(const u32 *x) {
	u32 i = 0;
	if constexpr (n > 16) {
		for (; i + 3 < n; i += 4) {
			if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
				if (x[i  ]) return n - i - 1;
				if (x[i+1]) return n - i - 2;
				if (x[i+2]) return n - i - 3;
				/* x[i+3] */return n - i - 4;
			}
		}
	}
	for (; i < n; ++i)
		if (x[i] != 0) return n - i - 1;
	return 0;
}

BI_ALWAYS_INLINE u32 highest_limb_imp(const u32 *x, const u32 n) {
	u32 i = 0;
	if  (n > 16) {
		for (; i + 3 < n; i += 4) {
			if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
				if (x[i  ]) return n - i - 1;
				if (x[i+1]) return n - i - 2;
				if (x[i+2]) return n - i - 3;
				/* x[i+3] */return n - i - 4;
			}
		}
	}
	for (; i < n; ++i)
		if (x[i] != 0) return n - i - 1;
	return 0;
}

// find the highest (MSB) limb
inline u32 highest_limb(const bui &x) {
	return highest_limb_template<BI_N>(x.data());
// 	u32 i = 0;
// #if BI_N > 16
// 	for (; i + 3 < BI_N; i += 4) {
// 		if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
// 			if (x[i  ]) return BI_N - i - 1;
// 			if (x[i+1]) return BI_N - i - 2;
// 			if (x[i+2]) return BI_N - i - 3;
// 			/* x[i+3] */return BI_N - i - 4;
// 		}
// 	}
// #endif
// 	for (; i < BI_N; ++i)
// 		if (x[i] != 0) return BI_N - i - 1;
// 	return 0;
}

// find the highest (MSB) limb
inline u32 highest_limb(const bul &x) {
	return highest_limb_template<BI_N * 2>(x.data());
// 	u32 i = 0;
// #if (BI_N * 2) > 16
// 	for (; i + 3 < BI_N * 2; i += 4) {
// 		if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
// 			if (x[i  ]) return BI_N * 2 - i - 1;
// 			if (x[i+1]) return BI_N * 2 - i - 2;
// 			if (x[i+2]) return BI_N * 2 - i - 3;
// 			/* x[i+3] */return BI_N * 2 - i - 4;
// 		}
// 	}
// #endif
// 	for (; i < BI_N * 2; ++i)
// 		if (x[i] != 0) return BI_N * 2 - i - 1;
// 	return 0;
}

inline void shift_limb_left(bui &x, const u32 l) {
	if (l == 0) return;
	if (l >= BI_N) {
		x = {};
		return;
	}
	std::copy(x.begin() + l, x.end(), x.begin());
	std::fill(x.end() - l, x.end(), 0);
}

inline void shift_limb_right(bui &x, const u32 l) {
	if (l == 0) return;
	if (l >= BI_N) {
		x = {};
		return;
	}
	std::copy_backward(x.begin(), x.end() - l, x.end());
	std::fill_n(x.begin(), l, 0);
}

// Big long: shift left by l whole limbs (each limb is 32 bits) in big‑endian representation.
// Storage is [x[0] = MSW, ..., x[2*BI_N-1] = LSW].
// eg: n = 5, l = 1
//   before: index	0	1	2	3	4
//           value	a0	a1	a2	a3	a4
//   after:			a1	a2	a3	a4	0 // multiplied by 2^(32*l)
inline void shift_limb_left(bul &x, const u32 l) {
	if (l == 0) return;
	if (l >= BI_N * 2) {
		x = {};
		return;
	}
	std::copy(x.begin() + l, x.end(), x.begin());
	std::fill(x.end() - l, x.end(), 0);
}

// Big long: shift right by l whole limbs (each limb is 32 bits) in big-endian representation.
// Storage is [x[0] = MSW, ..., x[2*BI_N-1] = LSW].
// eg: n = 5, l = 1
//   before: index  0   1   2   3   4
//           value  a0  a1  a2  a3  a4
//   after:         0   a0  a1  a2  a3 // divided by 2^(32*l)
inline void shift_limb_right(bul &x, const u32 l) {
	if (l == 0) return;
	if (l >= BI_N * 2) {
		x = {};
		return;
	}
	std::copy_backward(x.begin(), x.end() - l, x.end());
	std::fill_n(x.begin(), l, 0);
}

// shift left in-place (x *= 2^k)
// @deprecated Use shift_left_ip_fused_imp() instead
BI_ALWAYS_INLINE void shift_left_ip_imp(u32 *x, const u32 n, const u32 k) {
	if (k == 0) return;
	const u32 limbs = k / BI_SBU32;
	if (limbs >= n) {
		memset(x, 0, n * BI_SU32);
		return;
	}
	const u32 bits = k % BI_SBU32;
	// limb-only move (toward MSW)
	if (limbs) {
		memmove(x, x + limbs, (n - limbs) * BI_SU32);
		memset(x + n - limbs, 0, limbs * BI_SU32);
	}
	// intra-word stitch (only if bits != 0)
	if (bits) {
		u32 c = 0, i = n;
		while (i-- > 0) {
			u32 tmp = x[i];
			x[i] = tmp << bits | c;
			c = tmp >> (BI_SBU32 - bits);
		}
	}
}

BI_ALWAYS_INLINE void shift_left_ip_fused_imp(u32 *x, const u32 n, const u32 k) {
	if (k == 0) return;
	const u32 limbs = k / BI_SBU32;
	if (limbs >= n) {
		memset(x, 0, n * BI_SU32);
		return;
	}
	const u32 bits = k % BI_SBU32;
	if (bits) {
		u32 inv_bits = BI_SBU32 - bits;
		for (u32 i = 0; i < n - limbs - 1; ++i)
			x[i] = (x[i + limbs] << bits) | (x[i + limbs + 1] >> inv_bits);
		x[n - limbs - 1] = x[n - 1] << bits;
		memset(x + n - limbs, 0, limbs * BI_SU32);
	} else {
		memmove(x, x + limbs, (n - limbs) * BI_SU32);
		memset(x + n - limbs, 0, limbs * BI_SU32);
	}
}

inline void shift_left_ip(bui &x, const u32 k) {
	shift_left_ip_fused_imp(x.data(), BI_N, k);
}

inline void shift_left_ip(bul &x, const u32 k) {
	shift_left_ip_fused_imp(x.data(), BI_N * 2, k);
}

// shift left (r = x * 2^k)
inline bui shift_left(bui x, const u32 k) {
	assert(k < BI_BIT - 1 && "Cannot shift left by big amount (k > BI_BIT - 1)");

	if (k == 0) return x;
	u32 limbs = k / BI_SBU32;
	if (limbs >= BI_N) return {};
	u32 bits = k % BI_SBU32;
	bui r{};
	// limb-only move (toward MSW)
	std::copy(x.begin() + limbs, x.end(), r.begin());
	// intra-word stitch (only if bits != 0)
	if (bits) {
		u32 c = 0, i = BI_N;
		while (i-- > 0) {
			u32 tmp = r[i];
			r[i] = tmp << bits | c;
			c = tmp >> (32 - bits);
		}
	}
	return r;
}

// Experiment: shift left expand from bui to bul (r = x * 2^k)
inline bul shift_left_expand(bui x, const u32 k) {
	assert(k < BI_BIT * 2 - 1 && "Cannot shift left by big amount (k > 2xBIN_N - 1)");
	if (k == 0) return bui_to_bul(x);
	u32 limbs = k / BI_SBU32;
	if (limbs >= BI_N * 2) return {};
	u32 bits = k % BI_SBU32;
	bul r{};
	// limb-only move (toward MSW)
	std::copy_backward(x.begin() + (limbs > BI_N) * (limbs - BI_N), x.end(), r.begin() + BI_N * 2 - limbs);
	// intra-word stitch (only if bits != 0)
	if (bits) {
		u32 c = 0, i = BI_N * 2;
		while (i-- > 0) {
			u32 tmp = r[i];
			r[i] = tmp << bits | c;
			c = tmp >> (32 - bits);
		}
	}
	return r;
}

// Fused Expand Shift: Reads from 'x' once, writes directly to final position in 'r'
inline bul shift_left_expand_fused(const bui& x, const u32 k) {
	assert(k < BI_BIT * 2 - 1 && "Cannot shift left by big amount (k > 2xBIN_N - 1)");
	if (k == 0) return bui_to_bul(x);
	bul r{};
	u32 limbs = k / BI_SBU32;
	if (limbs >= BI_N * 2) return r;
	u32 bits = k % BI_SBU32;

	if (bits) {
		u32 inv_bits = BI_SBU32 - bits;
		u32 c = 0;
		for (u32 i = BI_N; i-- > 0;) {
			// underflow-proof boundary check
			if (i + BI_N < limbs) break;
			u32 r_idx = i + BI_N - limbs;
			u32 v = x[i];
			r[r_idx] = (v << bits) | c;
			c = v >> inv_bits;
		}
		if (limbs < BI_N)
			r[BI_N - 1 - limbs] = c;
	} else {
		for (u32 i = BI_N; i-- > 0;) {
			if (i + BI_N < limbs) break;
			u32 r_idx = i + BI_N - limbs;
			r[r_idx] = x[i];
		}
	}

	return r;
}

// shift left mod (r = x * 2^k mod m)
inline bui shift_left_mod2(const bui& x, const u32 k, const bui& m) {
	assert(k < BI_BIT * 2 && "Cannot shift left by big amount (k > 2xBI_BIT - 1)");
	bul p2 = shift_left_expand_fused(x, k);
	return nmod_native(p2, m);
}

// shift left mod (r = x * 2^k mod m)
inline bui shift_left_mod_bulk(bui x, u32 k, const bui& m) {
	x = mod_native(x, m);
	while (k > 0) {
		u32 step = k > BI_BIT ? BI_BIT : k;
		bul p2 = shift_left_expand_fused(x, step);
		x = nmod_native(p2, m);
		k -= step;
	}
	return x;
}

// shift left mod (r = x * 2^k mod m)
// @deprecated Use shift_left_mod2() instead
inline bui shift_left_mod(bui x, const u32 k, const bui& m) {
	return shift_left_mod2(x, k, m);
	// assert(k < BI_BIT * 2 && "Cannot shift left by big amount (k > 2xBI_BIT - 1)");
	// bul p2 = bul_pow2(k);
	// bui p2m = mod_native(p2, m);
	// x = mod_native(x, m);
	// mul_ref(x, p2m, p2);
	// p2m = mod_native(p2, m);
	// return p2m;
}

// shift right in-place (x /= 2^k)
// @deprecated Use shift_right_ip_fused_imp() instead
BI_ALWAYS_INLINE void shift_right_ip_imp(u32 *x, const u32 n, const u32 k) {
	if (k == 0) return;
	const u32 limbs = k / BI_SBU32;
	if (limbs >= n) {
		memset(x, 0, n * BI_SU32);
		return;
	}
	const u32 bits = k % BI_SBU32;
	// limb-only move (toward MSW)
	if (limbs) {
		memmove(x + limbs, x, (n - limbs) * BI_SU32);
		memset(x, 0, limbs * BI_SU32);
	}
	// intra-word stitch (only if bits != 0)
	if (bits) {
		u32 carry = 0;
		for (u32 i = 0; i < n; ++i) {
			u32 v = x[i];
			u32 new_val = v >> bits | carry;
			carry = v << (BI_SBU32 - bits);
			x[i] = new_val;
		}
	}
}

BI_ALWAYS_INLINE void shift_right_ip_fused_imp(u32 *x, const u32 n, const u32 k) {
	if (k == 0) return;
	const u32 limbs = k / BI_SBU32;
	if (limbs >= n) {
		memset(x, 0, n * BI_SU32);
		return;
	}
	const u32 bits = k % BI_SBU32;
	if (bits) {
		u32 inv_bits = BI_SBU32 - bits;
		for (int i = (int)n - 1; i > (int)limbs; --i)
			x[i] = (x[i - limbs] >> bits) | (x[i - limbs - 1] << inv_bits);
		x[limbs] = x[0] >> bits;
		memset(x, 0, limbs * BI_SU32);
	} else {
		// fallback to memmove only if it's a perfect limb-boundary shift
		memmove(x + limbs, x, (n - limbs) * BI_SU32);
		memset(x, 0, limbs * BI_SU32);
	}
}

// Big int: shift right in-place (x /= 2^k)
inline void shift_right_ip(bui& x, const u32 k) {
	shift_right_ip_fused_imp(x.data(), BI_N, k);
}

// Big long: shift right in-place (x /= 2^k)
inline void shift_right_ip(bul& x, const u32 k) {
	shift_right_ip_fused_imp(x.data(), BI_N * 2, k);
}

// Checking if input bigint is zero
BI_ALWAYS_INLINE bool bu_is0_imp(const u32 *x, u32 n) {
	// some optimization bs (loop unroll)
	while (n >= 4) {
		n -= 4;
		if (x[n] | x[n+1] | x[n+2] | x[n+3]) return false;
	}
	while (n-- > 0)
		if (x[n] != 0) return false;
	return true;
}

// Checking if input bui is zero
inline bool bui_is0(const bui &x) { return bu_is0_imp(x.data(), BI_N); }

// Checking if input bui is zero
inline bool bul_is0(const bul &x) { return bu_is0_imp(x.data(), BI_N * 2); }

// Return low-part of bul as bui
inline bui bul_low(const bul& x) {
	bui r{};
	std::copy(x.begin() + BI_N, x.end(), r.begin());
	return r;
}

// Return high-part of bul as bui
inline bui bul_high(const bul& x) {
	bui r{};
	std::copy_n(x.begin(), BI_N, r.begin());
	return r;
}

// Return new bul with low-part being input bui x
inline bul bui_to_bul(const bui& x) {
	bul r{};
	std::copy(x.begin(), x.end(), r.begin() + BI_N);
	return r;
}

// Return new bul from two buis
inline bul bul_from_2bui(const bui& high, const bui& low) {
	bul r{};
	std::copy(high.begin(), high.end(), r.begin());
	std::copy(low.begin(), low.end(), r.begin() + BI_N);
	return r;
}

BI_ALWAYS_INLINE int cmp_imp(const u32* a, const u32* b, const u32 n) {
	for (u32 i = 0; i < n; ++i)
		if (a[i] != b[i])
			return a[i] > b[i] ? 1 : -1;
	return 0;
}

BI_ALWAYS_INLINE int cmp_imp_nab(const u32* a, const u32 na, const u32* b, const u32 nb) {
	const u32 *a_ptr{a}, *b_ptr{b};
	u32 len = na;
	if (na > nb) {
		u32 diff = na - nb;
		for (u32 i = 0; i < diff; ++i)
			if (a[i] != 0) return 1;
		a_ptr += diff;
		len = nb;
	} else if (nb > na) {
		u32 diff = nb - na;
		for (u32 i = 0; i < diff; ++i)
			if (b[i] != 0) return -1;
		b_ptr += diff;
		len = na;
	}
	return cmp_imp(a_ptr, b_ptr, len);
}

// Compare between two bui
inline int cmp(const bui &a, const bui &b) { return cmp_imp(a.data(), b.data(), BI_N); }
// Compare between two bul
inline int cmp(const bul &a, const bul &b) { return cmp_imp(a.data(), b.data(), BI_N * 2); }
// Compare between bul and bui
inline int cmp(const bul& a, const bui& b) { return cmp_imp_nab(a.data(), BI_N * 2, b.data(), BI_N); }
// Compare between bui and bul
inline int cmp(const bui& a, const bul& b) { return cmp_imp_nab(a.data(), BI_N, b.data(), BI_N * 2); }

BI_ALWAYS_INLINE void randomize_imp(u32* x, const u32 n) {
	thread_local std::mt19937 gen([]{
		std::random_device rd;
		std::seed_seq seq{
			rd(), rd(), rd(), rd(),
			rd(), rd(), rd(), rd()
		};
		return std::mt19937(seq);
	}());
	// std::random_device rd; std::mt19937 gen(rd());
	// static thread_local std::mt19937 gen(123456); // fixed seed
	std::uniform_int_distribution<u32> len_dist(1, n);
	u32 limbs = len_dist(gen);
	for (u32 i = limbs; i < n; ++i) x[i] = 0;
	for (u32 i = 0; i < limbs; ++i) x[i] = gen();
}

inline void randomize_ip(bui &x) { randomize_imp(x.data(), BI_N); }
inline void randomize_ip(bul &x) { randomize_imp(x.data(), BI_N * 2); }

inline bui random_odd() {
	bui x{}; randomize_ip(x);
	set_bit_ip(x, 0, 1);
	return x;
}

BI_ALWAYS_INLINE u32 add_ip_n_imp(u32* a, const u32* b, u32 n) {
#if BI_USE_HW_INTRIN
	unsigned char c = 0;
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	while (n-- > 0)
		c = _addcarry_u32(c, a[n], b[n], &a[n]);
	return c;
#else
	u32 c = 0;
	while (n-- > 0) {
		u64 s = (u64)a[n] + b[n] + c;
		a[n] = (u32)s;
		c = s >> BI_SBU32;
	}
	return c;
#endif
}

// Add 1 to big int
BI_ALWAYS_INLINE void add_one_ip(u32* x, u32 n) { while (n-- > 0 && !++x[n]); }
BI_ALWAYS_INLINE void sub_one_ip(u32* x, u32 n) { while (n-- > 0 && !x[n]--); }

inline void add_ip_n(u32* a, const u32* b, const u32 n) { add_ip_n_imp(a, b, n); }

// a += b;
inline void add_ip(bui& a, const bui& b) { add_ip_n_imp(a.data(), b.data(), BI_N); }

[[nodiscard]] inline u32 add_ip_carry(bui &a, const bui &b) { return add_ip_n_imp(a.data(), b.data(), BI_N); }
[[nodiscard]] inline u32 add_ip_carry(bul &a, const bul &b) { return add_ip_n_imp(a.data(), b.data(), BI_N * 2); }

// a += b
inline void add_ip(bul& a, const bul& b) { add_ip_n_imp(a.data(), b.data(), BI_N * 2); }

// r.size = 2n
inline void add_n(const u32* a, const u32* b, u32* r, const u32 n) {
	std::copy_n(a, n, r);
	add_ip_n_imp(r, b, n);
}

// r = a + b
inline bui add(bui a, const bui& b) {
	add_ip(a, b);
	return a;
}

// a = (a + b) % m
inline void add_mod_ip(bui &a, const bui &b, const bui &m) {
	if (add_ip_n_imp(a.data(), b.data(), BI_N) || cmp(a, m) >= 0) {
		sub_ip(a, m);
	}
}

BI_ALWAYS_INLINE u32 sub_ip_n_imp(u32* a, const u32* b, u32 n) {
#if BI_USE_HW_INTRIN
	unsigned char br = 0;
	while (n-- > 0)
		br = _subborrow_u32(br, a[n], b[n], &a[n]);
	return br;
#else
	u32 br = 0;
	while (n-- > 0) {
		u64 d = (u64)a[n] - b[n] - br;
		a[n] = (u32)d;
		br = d >> BI_SBU32 & 1; // br occurs if 32nd bit is 1
	}
	return br;
#endif
}

// a -= b; // assume a > b
inline void sub_ip(bui& a, const bui& b) {
	sub_ip_n_imp(a.data(), b.data(), BI_N);
}

inline void sub_ip(bul& a, const bul& b) {
	sub_ip_n_imp(a.data(), b.data(), BI_N * 2);
}

// a -= b; // assume a > b
inline void sub_n(const u32* a, const u32* b, u32* r, u32 n) {
	std::copy_n(a, n, r);
	sub_ip_n_imp(r, b, n);
 }

inline bui sub(bui a, const bui& b) {
	sub_ip(a, b);
	return a;
}

inline void sub_mod_ip(bui& a, const bui& b, const bui& m) {
	if (cmp(a, b) >= 0) {
		sub_ip(a, b);
	} else {
		bui t = m, bb = b;
		sub_ip(bb, a); // bb = b - a
		sub_ip(t, bb); // t  = m - (b - a)
		a = t;
	}
}

BI_ALWAYS_INLINE void mul_imp(const u32* a, const u32* b, u32* r, const u32 n) {
	std::fill_n(r, 2 * n, 0);
	for (u32 i = n; i-- > 0;) {
		if (!a[i]) continue;
		u32 c = 0, j = n;
		while (j-- > 0) {
			u64 p = (u64)a[i] * b[j] + r[i + j + 1] + c;
			r[i + j + 1] = (u32)p;
			c = p >> BI_SBU32;
		}
		r[i] = c;
		// if (c) r[i] = c;
		// u32 k = i;
		// u32 counter = 0;
		// while (c) {
		// 	counter++;
		// 	if (counter >= 2) {
		// 		printf("DEBUG: It did loop a lot!\n");
		// 	} else if (counter == 1) {
		// 		printf("DEBUG: It did loop for once\n");
		// 	}
		// 	u64 s = (u64)r[k] + c;
		// 	r[k--] = (u32)s; // k may underflow but carry will be 0 next iter
		// 	c = s >> BI_SBU32;
		// }
	}
}

BI_ALWAYS_INLINE void mul_imp_fast(const u32* a, const u32* b, u32* r, const u32 n) {
	std::fill_n(r, 2 * n, 0);
	u32 hla = highest_limb_imp(a, n);
	if (hla == 0 && a[n - 1] == 0) return;
	u32 hlb = highest_limb_imp(b, n);
	if (hlb == 0 && b[n - 1] == 0) return;
	u32 start_a = n - 1 - hla;
	u32 start_b = n - 1 - hlb;

	for (u32 i = n; i-- > start_a;) {
		u32 a_limb = a[i];
		if (!a_limb) continue;
		u32 c = 0, j = n;
		BI_UNROLL(BI_UNROLL_THRESHOLD)
		while (j-- > start_b) {
			u64 p = (u64)a_limb * b[j] + r[i + j + 1] + c;
			r[i + j + 1] = (u32)p;
			c = p >> BI_SBU32;
		}
		r[i + start_b] = c;
		// u32 k = i + start_b;
		// while (c) {
		// 	if (r[k]) {
		// 		printf("mul_imp_fast: r_k not 0 (%u)\n", r[k]);
		// 	}
		// 	u64 s = (u64)r[k] + c;
		// 	r[k--] = (u32)s;
		// 	c = s >> BI_SBU32;
		// }
	}
}

BI_ALWAYS_INLINE void mul_imp2(const u32* a, const u32* b, u32* r, const u32 n) {
	std::fill_n(r, 2 * n, 0);
	for (u32 i = 0; i < n; ++i) {
		if (a[n - 1 - i] == 0) continue;
		u64 c = 0;
		u32 k = 2 * n - 1 - i;
		BI_UNROLL(BI_UNROLL_THRESHOLD)
		for (u32 j = 0; j < n; ++j) {
			u64 p = (u64)a[n - 1 - i] * b[n - 1 - j] + r[k] + c;
			r[k--] = (u32)p;
			c = p >> BI_SBU32;
		}
		r[k] = c;
		// while (c) {
		// 	u64 s = (u64)r[k] + c;
		// 	r[k--] = (u32)s;
		// 	c = s >> BI_SBU32;
		// }
	}
}

inline void mul_ref(const bui &a, const bui &b, bul &r) {
	mul_imp(a.data(), b.data(), r.data(), BI_N);
}

inline void mul_ip(bui &a, const bui &b) {
	bul r{};
	mul_ref(a, b, r);
	a = r.low();
}

inline bul mul(const bui& a, const bui& b) {
	bul r{};
	mul_ref(a, b, r);
	return r;
}

inline bui mul_low(const bui& a, const bui& b) {
	bul r = mul(a, b);
	return r.low();
}

// inline bui mul_low_fast(const bui& a, const bui& b) {
// 	bui r{};
// 	for (u32 i = 0; i < BI_N; ++i) {
// 		if (!a[BI_N - 1 - i]) continue;
// 		u32 c = 0;
// 		for (u32 j = 0; j < BI_N; ++j) {
// 			if (i + j >= BI_N) continue;
// 			u64 p = (u64)a[BI_N - 1 - i] * b[BI_N - 1 - j] + r[BI_N - 1 - (i + j)] + c;
// 			r[BI_N - 1 - (i + j)] = (u32)p;
// 			c = p >> BI_SBU32;
// 		}
// 	}
// 	return r;
// }

// template <u32 N>
// BI_ALWAYS_INLINE void mul_low_fast_template(const u32 *a, const u32 *b, u32 *r) {
// 	std::fill_n(r, N, 0);
// 	for (u32 i = 0; i < N; ++i) {
// 		u32 ai = a[N - 1 - i];
// 		if (!ai) continue;
// 		u32 c{0}, ri{N - 1 - i};
// 		for (u32 j = 0; j < N - i; ++j) {
// 			u64 p = (u64)ai * b[N - 1 - j] + r[ri - j] + c;
// 			r[ri - j] = (u32)p;
// 			c = p >> BI_SBU32;
// 		}
// 	}
// }

inline bui mul_low_fast(const bui& a, const bui& b) {
	bui r{};
	for (u32 i = 0; i < BI_N; ++i) {
		u32 ai = a[BI_N - 1 - i];
		if (!ai) continue;
		u32 c{0}, ri{BI_N - 1 - i};
		for (u32 j = 0; j < BI_N - i; ++j) {
			u64 s = (u64)ai * b[BI_N - 1 - j] + r[ri - j] + c;
			r[ri - j] = (u32)s;
			c = s >> BI_SBU32;
		}
	}
	return r;
}

inline bul mul_low_fast(const bul& a, const bul& b) {
	bul r{};
	for (u32 i = 0; i < BI_2N; ++i) {
		u32 ai = a[BI_2N - 1 - i];
		if (!ai) continue;
		u32 c{}, ri = BI_2N - 1 - i;
		for (u32 j = 0; j < BI_2N - i; ++j) {
			u64 s = (u64)ai * b[BI_2N - 1 - j] + r[ri - j] + c;
			r[ri - j] = (u32)s;
			c = s >> 32;
		}
	}
	return r;
}

inline void mul_mod_ip(bui &a, bui b, const bui &m) {
	a = mod_native(a, m);
	b = mod_native(b, m);
	bul r{};
	mul_ref(a, b, r);
	a = mod_native(r, m);
}

inline bui mod_native(bui x, const bui& m) {
	long long shift = (long long) highest_bit(x) - highest_bit(m);
	if (shift < 0) return x;

	for (; shift >= 0; --shift) {
		bui tmp = m;
		shift_left_ip(tmp, shift);
		if (cmp(x, tmp) >= 0)
			sub_ip(x, tmp);
	}
	return x;
}

inline bui mod_native(bul x, const bui& m) {
	long long shift = (long long) highest_bit(x) - highest_bit(m);
	if (shift < 0) return x.low();

	for (; shift >= 0; --shift) {
		bul tmp = bui_to_bul(m);
		shift_left_ip(tmp, shift);
		if (cmp(x, tmp) >= 0)
			sub_ip(x, tmp);
	}
	return x.low();
}

inline void mod_native_ip(bui& x, const bui& m) {
	long long shift = (long long) highest_bit(x) - highest_bit(m);
	if (shift < 0) return;
	for (; shift >= 0; --shift) {
		bui tmp = m;
		shift_left_ip(tmp, shift);
		if (cmp(x, tmp) >= 0)
			sub_ip(x, tmp);
	}
}

inline void mod_native_ip(bul& x, const bui& m) {
	long long shift = (long long) highest_bit(x) - highest_bit(m);
	if (shift < 0) return;

	for (; shift >= 0; --shift) {
		bul tmp = bui_to_bul(m);
		shift_left_ip(tmp, shift);
		if (cmp(x, tmp) >= 0)
			sub_ip(x, tmp);
	}
}

inline bui nmod_native(bui x, const bui& m) {
	long long shift = (long long)highest_bit(x) - highest_bit(m);
	if (shift < 0) return x;

	bui shifted_m = m;
	shift_left_ip(shifted_m, shift);

	for (; shift >= 0; --shift) {
		if (cmp(x, shifted_m) >= 0)
			sub_ip(x, shifted_m);
		shift_right_ip(shifted_m, 1); // Slide it down by 1 bit!
	}
	return x;
}

inline bui nmod_native(bul x, const bui& m) {
	long long shift = (long long)highest_bit(x) - highest_bit(m);
	if (shift < 0) return x.low();

	bul shifted_m = bui_to_bul(m);
	shift_left_ip(shifted_m, shift); // Shift up ONCE

	for (; shift >= 0; --shift) {
		if (cmp(x, shifted_m) >= 0)
			sub_ip(x, shifted_m);
		shift_right_ip(shifted_m, 1); // Slide it down by 1 bit!
	}
	return x.low();
}

// Do the exact same sliding window trick for the _ip versions:
inline void nmod_native_ip(bui& x, const bui& m) {
	long long shift = (long long)highest_bit(x) - highest_bit(m);
	if (shift < 0) return;
	bui shifted_m = m;
	shift_left_ip(shifted_m, shift);
	for (; shift >= 0; --shift) {
	// while (shift-- > 0) {
		if (cmp(x, shifted_m) >= 0)
			sub_ip(x, shifted_m);
		shift_right_ip(shifted_m, 1);
	}
}

inline void nmod_native_ip(bul& x, const bui& m) {
	long long shift = (long long)highest_bit(x) - highest_bit(m);
	if (shift < 0) return;
	bul shifted_m = bui_to_bul(m);
	shift_left_ip(shifted_m, shift);
	for (; shift >= 0; --shift) {
	// while (shift-- > 0) {
		if (cmp(x, shifted_m) >= 0)
			sub_ip(x, shifted_m);
		shift_right_ip(shifted_m, 1);
	}
}

/// <r = x*x> Return squared result of input x
BI_ALWAYS_INLINE void sqr_imp(const u32* a, u32* r, const u32 n) {
	std::fill_n(r, 2 * n, 0);

	// 1. Calculate symmetrical cross-products only ONCE (i < j)
	for (u32 i = n; i-- > 1;) {
		if (!a[i]) continue;
		u64 c = 0;
		for (u32 j = i; j-- > 0;) { // Notice j strictly stops before i
			u64 p = (u64)a[i] * a[j] + r[i + j + 1] + c;
			r[i + j + 1] = (u32)p;
			c = p >> BI_SBU32;
		}
		u32 k = i;
		while (c) {
			u64 s = (u64)r[k] + c;
			r[k] = (u32)s;
			c = s >> BI_SBU32;
			if (k-- == 0) break;
		}
	}

	// 2. Double the cross-products (r = r * 2)
	dbl_ip_n_imp(r, 2 * n);

	// 3. Add the squares (a[i] * a[i]) down the center diagonal
	u64 c = 0;
	for (u32 i = n; i-- > 0;) {
		u64 p = (u64)a[i] * a[i] + r[2 * i + 1] + c;
		r[2 * i + 1] = (u32)p;
		c = p >> BI_SBU32;

		// Propagate the square's carry up one limb
		u64 s = (u64)r[2 * i] + c;
		r[2 * i] = (u32)s;
		c = s >> BI_SBU32;
	}
}

/// <r = x*x> Return squared result of input x
inline bul sqr(const bui& a) {
	bul r{};
	sqr_imp(a.data(), r.data(), BI_N);
	return r;
}

inline void divmod(const bui& a, const bui& b, bui &q, bui &r) {
	q = {};
	r = a;
	long long shift = (long long) highest_bit(a) - highest_bit(b);
	if (shift < 0) return;
	// while (shift-- > 0) {
	for (; shift >= 0; --shift) {
		bui tmp = b;
		shift_left_ip(tmp, shift);
		if (cmp(r, tmp) >= 0) {
			sub_ip(r, tmp);
			set_bit_ip(q, shift, 1);
		}
	}
}

inline void divmod(const bul& a, const bui& b, bui &q, bul &r) {
	q = {};
	r = a;
	long long shift = highest_bit(a) - highest_bit(b);
	if (shift < 0) return;
	bul bb = bui_to_bul(b);
	// while (shift-- > 0) {
	for (; shift >= 0; --shift) {
		bul tmp = bb;
		shift_left_ip(tmp, shift);
		if (cmp(r, tmp) >= 0) {
			sub_ip(r, tmp);
			set_bit_ip(q, shift, 1);
		}
	}
}

BI_ALWAYS_INLINE u32 u32_divmod_single(u32 hi, u32 lo, u32 b, u32* rem) {
#if (defined(__GNUC__) || defined(__clang__)) && (defined(__i386__) || defined(__x86_64__))
	u32 q, r;
	__asm__("divl %4"
			: "=a"(q), "=d"(r)
			: "0"(lo), "1"(hi), "rm"(b));
	*rem = r;
	return q;
#elif defined(_MSC_VER) && (defined(_M_AMD64) || defined(_M_IX86))
	return _udiv64((((u64)hi) << 32) | lo, b, rem);
#else
	u64 dividend = ((u64)hi << 32) | lo;
	*rem = (u32)(dividend % b);
	return (u32)(dividend / b);
#endif
}

inline void u32_divmod(const bui& a, const u32 b, bui& q, u32& r) {
	q = {};
	r = 0;
	u32 hl = highest_limb(a);
	if (hl == 0 && a[BI_N - 1] == 0) return;
	for (int i = BI_N - 1 - hl; i < BI_N; ++i)
		q[i] = u32_divmod_single(r, a[i], b, &r);
}

inline u32 u32_mod(bui x, const u32 m) {
	u32 hl = highest_limb(x);
	if (hl == 0 && x[BI_N - 1] == 0) return 0;
	u32 r = 0;
	for (int i = BI_N - 1 - hl; i < BI_N; ++i)
		x[i] = u32_divmod_single(r, x[i], m, &r);
	return r;
}

inline void u32_divmod(const bul &a, const u32 b, bul &q, u32& r) {
	q = {};
	r = 0;
	u32 hl = highest_limb(a);
	if (hl == 0 && a[BI_N * 2 - 1] == 0) return;
	for (u32 i = BI_N * 2 - 1 - hl; i < BI_N * 2; ++i)
		q[i] = u32_divmod_single(r, a[i], b, &r);
}

inline u32 u32_mod(bul x, const u32 m) {
	u32 r = 0;
	u32 hl = highest_limb(x);
	if (hl == 0 && x[BI_N * 2 - 1] == 0) return 0;
	for (u32 i = BI_N * 2 - 1 - hl; i < BI_N * 2; ++i)
		x[i] = u32_divmod_single(r, x[i], m, &r);
	return r;
}


// Big int: return 2^k
inline bui bui_pow2(const u32 k) {
	assert(k < BI_BIT && "Input size must be in data range!");
	bui r{};
	set_bit_ip(r, k, 1);
	return r;
}

// Big long: return 2^k
inline bul bul_pow2(const u32 k) {
	assert(k < BI_BIT * 2);
	bul r{};
	set_bit_ip(r, k, 1);
	return r;
}

// Return 2^k - 1, k <= BI_BIN
inline bui bui_binary_flood1(const u32 k) {
	assert(k <= BI_BIT && "bui_binary_flood1: input k must be smaller than BI_BIT");
	bui r{};
	u32 l = k / BI_SBU32;
	u32 b = k % BI_SBU32;
	if (l) std::fill_n(r.data() + BI_N - l, l, 0xffffffffu);
	if (l < BI_N) r[BI_N - 1 - l] = (1u << b) - 1;
	return r;
}

// Return 2^k - 1, k <= 2xBI_BIN
inline bul bul_binary_flood1(const u32 k) {
	assert(k <= BI_BIT * 2 && "bul_binary_flood1: input k must be smaller than 2xBI_BIT");
	bul r{};
	u32 l = k / BI_SBU32;
	u32 b = k % BI_SBU32;
	if (l) std::fill_n(r.data() + BI_2N - l, l, 0xffffffffu);
	if (l < BI_2N) r[BI_2N - 1 - l] = (1u << b) - 1;
	return r;
}

// Return pow_mod (x^e % m)
inline bui pow_mod(bui x, const bui& e, const bui &m) {
	bui r = bui1();
	u32 hb = highest_bit(e);
	for (u32 i = 0; i < hb; ++i) {
		if (get_bit(e, i))
			mul_mod_ip(r, x, m);
		mul_mod_ip(x, x, m);
	}
	return r;
}

// Knuth Algorithm D
// Donald E. Knuth, The Art of Computer Programming, Volume 2: Seminumerical Algorithms
// Section: 4.3.1, Algorithm D (Division of large integers).
// https://skanthak.hier-im-netz.de/division.html
inline void divmod_knuth(const bui& a, const bui& b, bui& quot, bui& rem) {
	assert(!bui_is0(b));
	int cm = cmp(a, b);
	if (cm < 0) {
		quot = {};
		rem = a;
		return;
	}
	if (cm == 0) {
		quot = bui1();
		rem = {};
		return;
	}

	// 2. Fast path for single-limb divisor (n = 1)
	if (highest_limb(b) == 0 && b[BI_N - 1] != 0) {
		u32 r32 = 0;
		u32_divmod(a, b[BI_N - 1], quot, r32);
		rem = {};
		rem[BI_N - 1] = r32;
		return;
	}

	// 1. Normalize
	bul r = bui_to_bul(a);
	bui d = b;
	u32 d_lead_pow = highest_limb(b);
	u32 d_msw_idx = BI_N - 1 - d_lead_pow;
	u32 d0 = d[d_msw_idx];
	const u32 norm_shift = d0 == 0 ? 0 : BI_SBU32 - highest_bit(d0);

	if (norm_shift > 0) {
		shift_left_ip(d, norm_shift);
		shift_left_ip(r, norm_shift);
	}

	// Recalculate divisor info after normalization
	d_lead_pow = highest_limb(d);
	d_msw_idx = BI_N - 1 - d_lead_pow;
	d0 = d[d_msw_idx];
	const u32 d1 = d_msw_idx + 1 < BI_N ? d[d_msw_idx + 1] : 0;
	const u32 n = d_lead_pow + 1; // number of limbs in divisor

	// 3. Knuth Division Loop
	quot = {};
	u32 r_lead_pow = highest_limb(r);
	int j = (int)r_lead_pow - (int)d_lead_pow + 1;
	while (j-- > 0) {
		u32 r_idx = BI_N * 2 - 1 - (j + n);

		u32 u_jn = r[r_idx];
		u32 u_jn1 = (r_idx + 1 < BI_N * 2) ? r[r_idx + 1] : 0;
		u32 u_jn2 = (r_idx + 2 < BI_N * 2) ? r[r_idx + 2] : 0;

		u64 r_top = ((u64)u_jn << BI_SBU32) | u_jn1;
		u64 qhat, rhat;

		// calculate initial guess
		if (u_jn == d0) {
			qhat = 0xFFFFFFFFULL;
			rhat = (u64)u_jn1 + d0;
		} else {
			qhat = r_top / d0;
			rhat = r_top % d0;
		}

		// Knuth's correction step
		while (rhat < (1ULL << BI_SBU32) && qhat * d1 > (rhat << BI_SBU32) + u_jn2) {
			--qhat;
			rhat += d0;
		}

		// multiply and subtract
		u64 borrow = 0;
		u32 d_lsw_idx = BI_N - 1;

		for (u32 i = 0; i < n; ++i) {
			u32 r_i = r_idx + n - i;
			u32 d_i = d_lsw_idx - i;

			u64 sub = qhat * d[d_i] + borrow;
			// safe subtraction prevents u64 underflow
			borrow = (sub >> BI_SBU32) + (r[r_i] < (u32)sub);
			r[r_i] -= (u32)sub;
		}

		bool is_negative = borrow > r[r_idx];
		r[r_idx] -= (u32)borrow;
		// store quotient digit
		u32 q_idx = BI_N - 1 - j;
		quot[q_idx] = (u32)qhat;

		// add back if guess was too high
		if (is_negative) {
			--quot[q_idx];
			u32 carry = add_ip_n_imp(r.data() + r_idx + 1, d.data() + (BI_N - n), n);
			r[r_idx] += carry;
		}
	}

	// 4. Denormalize remainder
	if (norm_shift > 0)
		shift_right_ip(r, norm_shift);
	rem = r.low();
}

inline void divmod_knuth2(const bui& a, const bui& b, bui& quot, bui& rem) {
	assert(!bui_is0(b));
	int cm = cmp(a, b);
	if (cm < 0) {
		quot = {};
		rem = a;
		return;
	}
	if (cm == 0) {
		quot = bui1();
		rem = {};
		return;
	}

	u32 d_lead_pow = highest_limb(b);
	if (d_lead_pow == 0 && b[BI_N - 1] != 0) {
		u32 r32 = 0;
		u32_divmod(a, b[BI_N - 1], quot, r32);
		rem = {};
		rem[BI_N - 1] = r32;
		return;
	}

	const u32 n = d_lead_pow + 1;
	const u32 d_start = BI_N - n;
	bui d = b;

	u32 d0 = d[d_start];
	const u32 norm_shift = d0 == 0 ? 0 : BI_SBU32 - highest_bit(d0);

	std::array<u32, BI_N + 2> u{};
	std::copy_n(a.begin(), BI_N, u.begin() + 2);

	if (norm_shift > 0) {
		shift_left_ip_fused_imp(d.data(), BI_N, norm_shift);
		shift_left_ip_fused_imp(u.data(), BI_N + 2, norm_shift);
	}

	d0 = d[d_start];
	const u32 d1 = n > 1 ? d[d_start + 1] : 0;

	quot = {};
	const u32 u_lead_pow = highest_limb_imp(u.data(), BI_N + 2);
	int j = (int)u_lead_pow - (int)d_lead_pow + 1;

	while (j-- > 0) {
		const u32 u_idx = BI_N + 1 - (j + n);

		u32 u_jn = u[u_idx];
		u32 u_jn1 = u[u_idx + 1];
		u32 u_jn2 = (u_idx + 2 < BI_N + 2) ? u[u_idx + 2] : 0;

		u64 u_top = ((u64)u_jn << BI_SBU32) | u_jn1;
		u64 qhat, rhat;

		if (u_jn == d0) {
			qhat = 0xffffffffULL;
			rhat = (u64)u_jn1 + d0;
		} else {
			qhat = u_top / d0;
			rhat = u_top % d0;
		}

		while (rhat < (1ULL << BI_SBU32) && qhat * d1 > (rhat << BI_SBU32) + u_jn2) {
			--qhat;
			rhat += d0;
		}

		u64 borrow = 0;
		const u32 d_lsw_idx = BI_N - 1;

		for (u32 i = 0; i < n; ++i) {
			u32 u_i = u_idx + n - i;
			u32 d_i = d_lsw_idx - i;

			u64 sub = qhat * d[d_i] + borrow;
			borrow = (sub >> BI_SBU32) + (u[u_i] < (u32)sub);
			u[u_i] -= (u32)sub;
		}

		bool is_negative = borrow > u[u_idx];
		u[u_idx] -= (u32)borrow;

		u32 q_idx = BI_N - 1 - j;
		quot[q_idx] = (u32)qhat;

		if (is_negative) {
			--quot[q_idx];
			u32 carry = add_ip_n_imp(u.data() + u_idx + 1, d.data() + d_start, n);
			u[u_idx] += carry;
		}
	}

	if (norm_shift > 0)
		shift_right_ip_fused_imp(u.data(), BI_N + 2, norm_shift);

	rem = {};
	std::copy_n(u.begin() + 2, BI_N, rem.begin());
}

/// Computes x = (2x) in-place.
BI_ALWAYS_INLINE u32 dbl_ip_n_imp(u32* x, u32 n) {
	assert(n != 0 && "Cannot double zero-limb.");
	u32 c = x[0] >> 31;
	for (u32 i = 0; i < n - 1; ++i)
		x[i] = x[i] << 1 | x[i + 1] >> 31;
	x[n - 1] = x[n - 1] << 1;
	return c;
}

/// Computes x = (2x) in-place.
inline void dbl_ip(bui &x) { dbl_ip_n_imp(x.data(), BI_N); }

/// Computes x = (2x) in-place.
inline void dbl_ip(bul &x) { dbl_ip_n_imp(x.data(), BI_N * 2); }

/// Computes x = (2x) % m in-place.
/// Requires: 0 <= x < m.
static void dbl_mod_ip(bui &x, const bui &m) {
	if (dbl_ip_n_imp(x.data(), BI_N) || cmp(x, m) >= 0)
		sub_ip(x, m);
}

inline int hex_val(const unsigned char c) {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
	if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
	return -1;
}

// Convert bul to decimal string using base 1e9 chunks.
inline std::string bui_to_dec(const bui& x) {
	if (bui_is0(x)) return "0";

	std::vector<u32> parts;
	parts.reserve(BI_N);
	bui n = x, q{};

	while (!bui_is0(n)) {
		BI_OP_CONSTEXPR u32 BASE = 1000000000u;
		u32 r;
		u32_divmod(n, BASE, q, r);
		parts.push_back(r);
		n = q;
	}

	std::string out;
	out.reserve(parts.size() * 9);
	out += std::to_string(parts.back());
	for (u32 i = parts.size() - 1; i-- > 0;) {
		std::string chunk = std::to_string(parts[i]);
		out.append(9 - chunk.length(), '0');
		out.append(chunk);
	}
	return out;
}

// Convert bul to decimal string using base 1e9 chunks.
inline std::string bul_to_dec(const bul& x) {
	if (bul_is0(x)) return "0";

	std::vector<u32> parts;
	parts.reserve(BI_N * 2);
	bul n = x, q{};

	while (!bul_is0(n)) {
		BI_OP_CONSTEXPR u32 BASE = 1000000000u;
		u32 r;
		u32_divmod(n, BASE, q, r);
		parts.push_back(r);
		n = q;
	}

	std::string out;
	out.reserve(parts.size() * 9);
	out += std::to_string(parts.back());
	for (u32 i = parts.size() - 1; i-- > 0;) {
		std::string chunk = std::to_string(parts[i]);
		out.append(9 - chunk.length(), '0');
		out.append(chunk);
	}
	return out;
}

inline std::string bui_to_hex(const bui &a, const bool uppercase = false, const bool split = false) {
	if (bui_is0(a)) return "0";
	const char* hex_chars = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
	std::string out;
	u32 hl = highest_limb(a);
	out.reserve(hl * (split ? 9 : 8));
	bool first_limb = true;
	for (u32 i = BI_N - hl - 1; i < BI_N; ++i) {
		u32 val = a[i];
		if (first_limb) {
			// strip leading zeros for the very first limb printed
			bool printing = false;
			for (int shift = BI_SBU32 - 4; shift >= 0; shift -= 4) {
				u32 nibble = val >> shift & 0xF;
				if (nibble > 0 || printing) {
					out.push_back(hex_chars[nibble]);
					printing = true;
				}
			}
			first_limb = false;
		} else {
			// print all 8 characters for inner limbs
			if (split) out.push_back(' ');
			out.push_back(hex_chars[val >> 28 & 0xF]);
			out.push_back(hex_chars[val >> 24 & 0xF]);
			out.push_back(hex_chars[val >> 20 & 0xF]);
			out.push_back(hex_chars[val >> 16 & 0xF]);
			out.push_back(hex_chars[val >> 12 & 0xF]);
			out.push_back(hex_chars[val >>  8 & 0xF]);
			out.push_back(hex_chars[val >>  4 & 0xF]);
			out.push_back(hex_chars[val	      & 0xF]);
		}
	}
	return out;
}

inline std::string bui_to_bin(const bui& x) {
	// u32 hb = highest_bit(x);
	// if (hb == 0 && x[BI_N - 1] == 0) return "0";
	u32 nhb = highest_bit(x) + 1;
	std::string out;
	out.reserve(nhb);
	while (nhb-- > 0)
		out.push_back(get_bit(x, nhb) ? '1' : '0');
	return out;
}

// Divide a double-width big-int (bul, MSW at index 0) by a 32-bit divisor.
// q := a / d (quotient), returns remainder r = a % d.
// Requires: d != 0
inline u32 u32_divmod_bul(const bul &a, u32 d, bul &q) {
	u64 rem = 0;
	for (u32 i = 0; i < BI_N * 2; ++i) q[i] = 0;
	for (u32 i = 0; i < BI_N * 2; ++i) {
		rem = (rem << 32) | (u64)a[i]; // bring down next limb
		// quotient limb fits in 32 bits because rem < d * 2^32 here
		u32 qi = (u32)(rem / d);
		q[i] = qi;
		rem = rem - (u64)qi * (u64)d; // rem = rem % d
	}
	return (u32)rem;
}

// Lightweight O(N) multiply and add for a 32-bit multiplier
inline void mul_u32_add_ip(bui& x, u32 multiplier, u32 addition) {
	u64 c = addition;
	u32 i = BI_N;
	while (i-- > 0) {
		u64 p = (u64)x[i] * multiplier + c;
		x[i] = (u32)p;
		c = p >> 32;
	}
}

// Big int: return bui from dec string
inline bui bui_from_dec(const std::string& s) {
	assert(!s.empty() && "bui_from_dec: empty string");
	bui out{};
	u32 chunk = 0;
	u32 chunk_multiplier = 1;
	u32 i = 0;
	// skip leading spaces and optional '+'
	while (isspace(s[i])) ++i;
	if (s[i] == '+') ++i;
	assert(s[i] != '-' && "bui_from_dec: negative not supported");
	// skip leading zeros, underscores, spaces
	while (s[i] == '0' || s[i] == '_') ++i;
	for (; i < s.size(); ++i) {
		char c = s[i];
		if (c < '0' || c > '9') continue;
		chunk = chunk * 10 + (c - '0');
		chunk_multiplier *= 10;
		//flush the chunk to bui when hit 1E9
		if (chunk_multiplier == 1000000000u) {
			mul_u32_add_ip(out, chunk_multiplier, chunk);
			chunk = 0;
			chunk_multiplier = 1;
		}
	}
	// flush remaining
	if (chunk_multiplier > 1) {
		mul_u32_add_ip(out, chunk_multiplier, chunk);
	}
	return out;
}

// Big int: return bui from hex string
inline bui bui_from_hex(const std::string& s) {
	assert(!s.empty() && "bui_from_hex: empty string");
	bui out{};
	int start_idx = 0;
	int len = (int)s.size();
	while (start_idx < len && isspace(s[start_idx])) ++start_idx;
	if (start_idx + 1 < len && s[start_idx] == '0' && (s[start_idx+1] == 'x' || s[start_idx+1] == 'X')) start_idx += 2;
	int str_idx = len - 1;
	int limb_idx = BI_N - 1;

	// chunks of 8 hex chars (32 bits)
	while (str_idx >= start_idx && limb_idx >= 0) {
		u32 limb_val = 0;
		u32 shift = 0;
		while (str_idx >= start_idx && shift < BI_SBU32) {
			char c = s[str_idx--];
			if (c == '_' || isspace(c)) continue;

			int val = hex_val(c);
			if (val >= 0) {
				limb_val |= (u32)val << shift;
				shift += 4;
			}
		}
		out[limb_idx--] = limb_val;
	}
	return out;
}

inline bui bui_from_bin(const std::string& s) {
	bui out{};
	int start_idx = 0;
	int len = (int)s.size();
	while (start_idx < len && isspace(s[start_idx])) ++start_idx;
	if (start_idx + 1 < len && s[start_idx] == '0' && (s[start_idx+1] == 'b' || s[start_idx+1] == 'B')) start_idx += 2;
	int str_idx = len - 1;
	int limb_idx = BI_N - 1;

	while (str_idx >= start_idx && limb_idx >= 0) {
		u32 limb_val = 0;
		u32 shift = 0;

		while (str_idx >= start_idx && shift < 32) {
			char c = s[str_idx--];
			if (c == '_' || isspace(c)) continue;
			if (c == '1') limb_val |= (1u << shift);
			if (c == '0' || c == '1') shift++;
		}
		out[limb_idx--] = limb_val;
	}
	return out;
}

// ALWAYS_INLINE void split_bui(const bui &x, bui &high, bui &low, u32 n) {
// 	std::copy_n(x.begin(), n, high.begin() + (BI_N - n));
// 	std::copy(x.begin() + n, x.end(), low.begin() + n);
// }
//
// inline bul karatsuba(const bui &a, const bui &b, const u32 n) {
// 	bul r{};
// 	if (n <= 16) return mul(a, b);
// 	u32 half = n / 2;
//
// 	bui a1{}, a0{}, b1{}, b0{};
// 	split_bui(a, a1, a0, half);
// 	split_bui(b, b1, b0, half);
//
// 	bul z2 = karatsuba(a1, b1, half);
// 	bul z0 = karatsuba(a0, b0, half);
//
// 	bui a_sum = add(a1, a0);
// 	bui b_sum = add(b1, b0);
// 	bul z1 = karatsuba(a_sum, b_sum, half);
// 	sub_ip(z1, z2);
// 	sub_ip(z1, z0);
//
// 	shift_limb_left(z2, 2 * half);
// 	shift_limb_left(z1, half);
//
// 	add_ip(r, z0);
// 	add_ip(r, z1);
// 	add_ip(r, z2);
// 	return r;
// }
//
// size_t KARATSUBA_CUTOFF = 2;
// // size_t KARATSUBA_CUTOFF = 4;
// // size_t KARATSUBA_CUTOFF = 8;
// // size_t KARATSUBA_CUTOFF = 16;
// // size_t KARATSUBA_CUTOFF = 32; // tune this experimentally
// inline void karatsuba_be_rec_old(const u32* a, const u32* b, u32* r, const u32 n, u32* scratch) {
//     if (n <= KARATSUBA_CUTOFF) {
//         mul_imp(a, b, r, n);
//         return;
//     }
//
//     u32 half = n / 2;
//     const u32* a1 = a;
//     const u32* a0 = a + half;
//     const u32* b1 = b;
//     const u32* b0 = b + half;
//
//     u32* z2 = r;       // high part
//     u32* z0 = r + n;   // low part
//     u32* z1 = scratch; // middle temp (2*half)
//
// 	const u32 maxlen = std::max(half, half);
//
//     u32* tmp_a = z1 + 2 * maxlen;
//     u32* tmp_b = tmp_a + maxlen;
//     u32* tmp_scratch = tmp_b + maxlen;
//
//     karatsuba_be_rec_old(a0, b0, z0, half, tmp_scratch); // z0 = a0 * b0
//     karatsuba_be_rec_old(a1, b1, z2, half, tmp_scratch); // z2 = a1 * b1
//
// 	add_n(a1, a0, tmp_a, half); // tmp_a = a1 + a0
// 	add_n(b1, b0, tmp_b, half); // tmp_b = b1 + b0
//     karatsuba_be_rec_old(tmp_a, tmp_b, z1, half, tmp_scratch); // z1 = (a1 + a0) * (b1 + b0)
//
//     // z1 = z1 - z2 - z0
// 	sub_n(z1, z2, z1, 2 * half);
// 	sub_n(z1, z0, z1, 2 * half);
//     // combine: r = z2 << (2*half*32) + z1 << (half*32) + z0
// 	add_n(r + half, z1, r + half, 2 * half);
// }
//
// inline void karatsuba_be_rec(const u32* a, const u32* b, u32* r, u32 n, u32* scratch) {
// 	if (bu_is0(a, n) || bu_is0(b, n)) {
// 		std::fill_n(r, 2 * n, 0);
// 		return;
// 	}
// 	if (n <= KARATSUBA_CUTOFF) {
// 		mul_imp(a, b, r, n);
// 		return;
// 	}
//
// 	const u32 half  = n / 2;
// 	const u32 other = n - half;       // may be half+1 if n is odd
//
// 	// Big-endian split
// 	const u32* a1 = a;           // high half
// 	const u32* a0 = a + half;    // low  half
// 	const u32* b1 = b;
// 	const u32* b0 = b + half;
//
// 	// workspace layout
// 	u32* z0 = scratch;                 // size 2*other
// 	u32* z1 = z0 + 2 * other;          // size 2*other
// 	u32* z2 = z1 + 2 * other;          // size 2*half
// 	u32* tmp_a = z2 + 2 * half;
// 	u32* tmp_b = tmp_a + other;
// 	u32* subscratch = tmp_b + other;
//
// 	// z0 = a0 * b0
// 	karatsuba_be_rec(a0, b0, z0, other, subscratch);
//
// 	// z2 = a1 * b1
// 	karatsuba_be_rec(a1, b1, z2, half, subscratch);
//
// 	// tmp_a = a0 + a1 (aligned to low indices)
// 	std::fill_n(tmp_a, other, 0);
// 	std::fill_n(tmp_b, other, 0);
// 	std::copy(a0 + (other - half), a0 + other, tmp_a + (other - half));
// 	for (u32 i = 0; i < half; ++i)
// 		tmp_a[i] += a1[i];
// 	std::copy(b0 + (other - half), b0 + other, tmp_b + (other - half));
// 	for (u32 i = 0; i < half; ++i)
// 		tmp_b[i] += b1[i];
//
// 	// z1 = (a0+a1)*(b0+b1)
// 	karatsuba_be_rec(tmp_a, tmp_b, z1, other, subscratch);
//
// 	// z1 = z1 - z2 - z0
// 	sub_n(z1 + (2 * other - 2 * half), z2, z1 + (2 * other - 2 * half), 2 * half);
// 	sub_n(z1, z0, z1, 2 * other);
//
// 	// clear result
// 	std::fill_n(r, 2 * n, 0);
//
// 	// combine (big-endian)
// 	// copy z0 → low end
// 	std::copy(z0 + 2 * other - n, z0 + 2 * other, r + 2 * n - 2 * other);
//
// 	// add z1 shifted by (other limbs)
// 	add_n(r + (n - other), z1 + 2 * other - n, r + (n - other), 2 * other);
//
// 	// add z2 shifted by (2*other limbs)
// 	add_n(r, z2 + 2 * half - n, r, 2 * half);
// }
//
// inline u32 next_pow2(u32 x) {
// 	if (x == 0) return 1;
// 	x--;
// 	x |= x >> 1;
// 	x |= x >> 2;
// 	x |= x >> 4;
// 	x |= x >> 8;
// 	x |= x >> 16;
// 	x++;
// 	return x;
// }
//
// inline bul karatsuba_be_top(const bui& a, const bui& b) {
//     size_t n = BI_N;
// 	// u32 n = std::max(highest_limb(a), highest_limb(b));
// 	// n = next_pow2(n) * 2;
//     bul r{};
//     std::array<u32, 6 * BI_N> scratch{};
//     // std::array<u32, 8 * BI_N> scratch{};
//     karatsuba_be_rec_old(a.data(), b.data(), r.data(), n, scratch.data());
//     return r;
// }
//
// // for compatibility with your test code
// inline bul karatsu_test(const bui& a, const bui& b) {
//     return karatsuba_be_top(a, b);
// }

// Extended Euclidean algorithm
inline bool mod_inverse_old(bui a, const bui &m, bui &inv_out) {
	// invalid modulus or zero
	if (bui_is0(m)) return false;
	if (cmp(a, m) >= 0) a = mod_native(a, m);
	if (bui_is0(a)) return false; // zero has no inverse
	bui r0 = m, r1 = a, t0{}, t1 = bui1();
	while (!bui_is0(r1)) {
		// q = r0 / r1, rem = r0 % r1
		bui q{}, rem{};
		divmod(r0, r1, q, rem);
		// r0, r1 = r1, rem
		r0 = r1, r1 = rem;
		// t_new = (t0 - q * t1) mod m
		// compute q * t1 -> bul, then reduce modulo m to get r_qt (bui)
		bul prod{};
		mul_ref(q, t1, prod);  // prod = q * t1 (2N words)
		auto qtm_rem = mod_native(prod, m); // qtm_rem = (prod) % m

		// t_new = t0 - qtm_rem mod m
		bui tnew = t0;
		if (cmp(tnew, qtm_rem) >= 0) {
			sub_ip(tnew, qtm_rem);
		} else {
			// tnew = (t0 - qtm_rem) mod m = m - (qtm_rem - t0)
			tnew = m;
			sub_ip(qtm_rem, t0);
			sub_ip(tnew, qtm_rem);
		}
		t0 = t1;
		t1 = tnew;
	}

	// r0 = gcd(a, m) so if gcd != 1 -> no inverse
	if (cmp(r0, bui1()) != 0) return false;
	inv_out = t0;
	return true;
}

// Extended Euclidean algorithm: find a^{-1} mod m
inline bool mod_inverse(const bui& a_in, const bui& m, bui& inv_out) {
	if (bui_is0(m)) return false; // invalid modulus
	bui a = a_in;
	if (cmp(a, m) >= 0) a = mod_native(a, m);      // reduce a mod m
	if (bui_is0(a)) return false; // 0 has no inverse

	bui r0 = m, r1 = a;
	bui t0{}, t1 = bui1();

	while (!bui_is0(r1)) {
		bui q{}, rem{};
		divmod(r0, r1, q, rem); // r0 = q*r1 + rem
		r0 = r1;
		r1 = rem;

		// t_new = (t0 - q * t1) mod m
		bui qt = t1;
		mul_mod_ip(qt, q, m); // qt = (q * t1) mod m

		bui tnew{};
		if (cmp(t0, qt) >= 0) {
			tnew = t0;
			sub_ip(tnew, qt); // tnew = t0 - qt
		} else {
			tnew = m;
			sub_ip(qt, t0);   // qt = qt - t0
			sub_ip(tnew, qt); // tnew = m - (qt - t0)
		}

		t0 = t1;
		t1 = tnew;
	}

	if (cmp(r0, bui1()) != 0) return false;        // gcd != 1 -> no inverse
	inv_out = t0;                                  // already in [0, m)
	return true;
}

inline bool mod_inverse_binary(bui a, const bui& m, bui& inv_out) {
	if (bui_is0(a) || bui_is0(m)) return false;
	if (cmp(a, m) >= 0) a = mod_native(a, m);
	if (bui_is0(a)) return false;

	bui u = a;
	bui v = m;
	bui x1 = bui1();
	bui x2 = bui0();

	// Loop strictly until u reaches 0
	while (!bui_is0(u)) {
		// While u is even
		while (!get_bit(u, 0)) {
			shift_right_ip(u, 1);
			if (get_bit(x1, 0)) {
				// x1 = (x1 + m) / 2
				// Catch the 513th carry bit!
				u32 carry = add_ip_n_imp(x1.data(), m.data(), BI_N);
				shift_right_ip(x1, 1);
				// Inject the lost carry back into the Most Significant Bit
				if (carry) set_bit_ip(x1, BI_BIT - 1, 1);
			} else {
				shift_right_ip(x1, 1);
			}
		}

		// While v is even
		while (!get_bit(v, 0)) {
			shift_right_ip(v, 1);
			if (get_bit(x2, 0)) {
				// x2 = (x2 + m) / 2
				u32 carry = add_ip_n_imp(x2.data(), m.data(), BI_N);
				shift_right_ip(x2, 1);
				if (carry) set_bit_ip(x2, BI_BIT - 1, 1);
			} else {
				shift_right_ip(x2, 1);
			}
		}

		// Subtract the smaller from the larger
		if (cmp(u, v) >= 0) {
			sub_ip(u, v);
			// x1 = x1 - x2 (modulo m)
			if (cmp(x1, x2) >= 0) {
				sub_ip(x1, x2);
			} else {
				bui tmp = x2;
				sub_ip(tmp, x1); // tmp = x2 - x1
				x1 = m;
				sub_ip(x1, tmp); // x1 = m - (x2 - x1)
			}
		} else {
			sub_ip(v, u);
			// x2 = x2 - x1 (modulo m)
			if (cmp(x2, x1) >= 0) {
				sub_ip(x2, x1);
			} else {
				bui tmp = x1;
				sub_ip(tmp, x2); // tmp = x1 - x2
				x2 = m;
				sub_ip(x2, tmp); // x2 = m - (x1 - x2)
			}
		}
	}

	// When u == 0, the GCD is in v.
	// If v == 1, then x2 is the modular inverse.
	if (cmp(v, bui1()) == 0) {
		inv_out = x2;
		return true;
	}

	return false; // GCD != 1, meaning 'a' and 'm' share a factor
}

/**
 * @brief Montgomery modular arithmetic helper for fixed-size big integers.
 *
 * This struct precomputes all constants needed to perform fast modular
 * multiplication and exponentiation modulo an odd big integer using the
 * Montgomery reduction algorithm in the bui/bul representation.
 *
 * Ref:
 * [1] https://en.wikipedia.org/wiki/Montgomery_modular_multiplication
 * [2] https://cp-algorithms.com/algebra/montgomery_multiplication.html
 * [3] https://en.algorithmica.org/hpc/number-theory/montgomery/
 * [4] MVP: https://www.nayuki.io/page/montgomery-reduction-algorithm
 */
struct MontgomeryReducer {
	bui modulus;        // must be odd >= 3
	bul reducer{};      // power of 2
	bui mask{};         // reducer - 1
	u32 reducerBits;    // log2(reducer)
	bui reciprocal{};   // reducer^-1 mod modulus
	bui factor{};       // (reducer * reciprocal - 1) / modulus
	bui convertedOne{}; // convertIn(1) aka reducer mod modulus
	static bui modInverse(const bui& a, const bui& m);

	MontgomeryReducer(const bui& modulus) : modulus(modulus) {
		assert(get_bit(modulus, 0) && cmp(modulus, bui1()) == 1);
		reducerBits = (highest_bit(modulus) / BI_SBU32 + 1) * BI_SBU32;
		if (reducerBits > BI_BIT) reducerBits = BI_BIT;
		reducer = bul_pow2(reducerBits);
		mask = bui_binary_flood1(reducerBits);
		convertedOne = mod_native(reducer, modulus);
		mod_inverse_old(convertedOne, modulus, reciprocal); // reducer^-1 mod modulus

		auto tmp = bui_to_bul(reciprocal);
		shift_left_ip(tmp, reducerBits);
		sub_ip(tmp, bul1());
		bul rem{};
		divmod(tmp, modulus, factor, rem);
		// std::cout << "modulus      = " << bui_to_dec(modulus)      << "\n";
		// std::cout << "reducer      = " << bul_to_dec(reducer)      << "\n";
		// std::cout << "mask         = " << bui_to_dec(mask)         << "\n";
		// std::cout << "reducerBits  = " << reducerBits              << "\n";
		// std::cout << "reciprocal   = " << bui_to_dec(reciprocal)   << "\n";
		// std::cout << "factor       = " << bui_to_dec(factor)       << "\n";
		// std::cout << "convertedOne = " << bui_to_dec(convertedOne) << "\n";
	}

	// convert a standard integer into Montgomery form
	bui convertIn(const bui& x) const {
		return shift_left_mod(x, reducerBits, modulus);
	}

	// convert a Montgomery form integer back to standard form
	bui convertOut(bui x) const {
		mul_mod_ip(x, reciprocal, modulus);
		return x;
	}

	// Multiply two Montgomery-form numbers
	bui multiply(const bui& x, const bui& y) const {
		assert(cmp(x, modulus) < 0 && cmp(y, modulus) < 0);
		bul product = mul(x, y);
		bui t_low = product.low();
		bitwise_and_ip(t_low, mask);
		t_low = mul_low_fast(t_low, factor);
		bitwise_and_ip(t_low, mask);
		auto tmp2 = mul(t_low, modulus);
		u32 c = add_ip_n_imp(product.data(), tmp2.data(), BI_N * 2);
		shift_right_ip(product, reducerBits);
		if (c) {
			bul carry = bul_pow2(BI_BIT * 2 - reducerBits);
			add_ip(product, carry);
		}
		if (cmp(product, modulus) >= 0)
			sub_ip(product, bui_to_bul(modulus));
		return product.low();
	}

	// Montgomery exponentiation: x^e (e standard, x and result in Montgomery form)
	bui pow(bui x, const bui& e) const {
		bui r = convertedOne;
		u32 hb = highest_bit(e);
		for (u32 i = 0; i < hb; ++i) {
			if (get_bit(e, i))
				r = multiply(r, x);
			x = multiply(x, x);
		}
		return r;
	}
};

// m > 1 and m is odd
inline bool is_valid_modulus(const bui &m) {
	return cmp(m, bui1()) > 0 && get_bit(m, 0);
}

// Montgomery power (faster than naive version for big num), m must be odd
inline bui mr_pow_mod(bui x, const bui& e, const bui& m) {
	if (!is_valid_modulus(m)) return pow_mod(x, e, m);
	MontgomeryReducer mr(m);
	x = mr.convertIn(x);
	bui r = mr.pow(x, e);
	return mr.convertOut(r);
}

struct MontgomeryReducer2 {
	bui modulus;        // must be odd >= 3
	bul reducer{};      // power of 2
	bui mask{};         // reducer - 1
	u32 reducerBits;    // log2(reducer)
	bui reciprocal{};   // reducer^-1 mod modulus
	bui factor{};       // (reducer * reciprocal - 1) / modulus
	bui convertedOne{}; // convertIn(1) aka reducer mod modulus
	static bui modInverse(const bui& a, const bui& m);

	MontgomeryReducer2(const bui& modulus) : modulus(modulus) {
		assert(get_bit(modulus, 0) && cmp(modulus, bui1()) == 1);
		reducerBits = BI_BIT; // 512
		reducer = bul_pow2(reducerBits); // 2^512
		mask = bui_binary_flood1(reducerBits); // 2^512 - 1
		convertedOne = mod_native(reducer, modulus);
		mod_inverse_binary(convertedOne, modulus, reciprocal); // reducer^-1 mod modulus

		bul tmp{};
		std::copy_n(reciprocal.begin(), BI_N, tmp.begin());
		sub_ip(tmp, bul1());
		bul rem{};
		divmod(tmp, modulus, factor, rem);
		// std::cout << "modulus      = " << bui_to_dec(modulus)      << "\n";
		// std::cout << "reducer      = " << bul_to_dec(reducer)      << "\n";
		// std::cout << "mask         = " << bui_to_dec(mask)         << "\n";
		// std::cout << "reducerBits  = " << reducerBits              << "\n";
		// std::cout << "reciprocal   = " << bui_to_dec(reciprocal)   << "\n";
		// std::cout << "factor       = " << bui_to_dec(factor)       << "\n";
		// std::cout << "convertedOne = " << bui_to_dec(convertedOne) << "\n";
	}

	// convert a standard integer into Montgomery form
	bui convertIn(const bui& x) const {
		bul p2{};
		auto t = mod_native(x, modulus);
		mul_ref(t, convertedOne, p2);
		bui p2m = mod_native(p2, modulus);
		return p2m;
	}

	// convert a Montgomery form integer back to standard form
	bui convertOut(bui x) const {
		mul_mod_ip(x, reciprocal, modulus);
		return x;
	}

	// Multiply two Montgomery-form numbers
	bui multiply(const bui& x, const bui& y) const {
		assert(cmp(x, modulus) < 0 && cmp(y, modulus) < 0);
		bul product = mul(x, y);
		bui t_low = product.low();
		t_low = mul_low_fast(t_low, factor);
		auto tmp2 = mul(t_low, modulus);
		u32 c = add_ip_n_imp(product.data(), tmp2.data(), BI_N * 2);
		bui result = bul_high(product);
		// 5. The Carry Resolution & Final Reduction
		// If c == 1, the true value is >= 2^512, so it MUST be >= M.
		// The CPU underflow automatically absorbs the c=1 carry!
		if (c || cmp(result, modulus) >= 0)
			sub_ip(result, modulus);

		return result;
	}

	// Montgomery exponentiation: x^e (e standard, x and result in Montgomery form)
	bui pow(const bui& x, const bui& e) const {
		bui r = convertedOne;
		for (long long i = highest_bit(e); i >= 0; --i) {
			r = multiply(r, r);
			if (get_bit(e, i))
				r = multiply(r, x);
		}
		return r;
	}
};

// Montgomery power (faster than naive version for big num), m must be odd
inline bui mr2_pow_mod(bui x, const bui& e, const bui& m) {
	if (!is_valid_modulus(m)) {
		return pow_mod(x, e, m);
	}
	MontgomeryReducer2 mr(m);
	x = mr.convertIn(x);
	bui r = mr.pow(x, e);
	return mr.convertOut(r);
}

/**
 * @brief Barrett modular arithmetic helper for fixed-size big integers.
 * Precomputes mu = floor(2^1024 / m) to replace expensive division
 * with multiplication and bit shifts. Ideal for when the modulus
 * is fixed across many operations.
 */
struct BarrettReducer {
	bui modulus;
	bul mu{}; // Precomputed 2^1024 / m
	BarrettReducer(const bui& m) : modulus(m) {
		assert(cmp(m, bui1()) > 0 && "Modulus must be >= 2 for Barrett reduction");
		mu = compute_mu(m);
	}

	// Precomputes \mu = 2^{1024} / m
	static bul compute_mu(const bui& m) {
		bul q{};
		bul r{}; // Using bul for remainder to prevent overflow during shift_left_ip
		bul m_bul = bui_to_bul(m);

		// 1. Calculate exactly how many bits we can safely skip
		u32 m_bits = highest_bit(m) + 1;

		// 2. Fast-forward the remainder to 2^(m_bits)
		set_bit_ip(r, m_bits, 1);

		// 3. Run the loop only for the remaining bits (usually ~512 iterations instead of 1024)
		for (int i = BI_BIT * 2 - m_bits; i >= 0; --i) {
			if (cmp(r, m_bul) >= 0) {
				sub_ip(r, m_bul);
				set_bit_ip(q, i, 1);
			}
			if (i > 0)
				shift_left_ip(r, 1);
		}
		return q;
	}

	// 1024-bit x 1024-bit -> Returns the Top 1024-bits
	static bul mul_top_1024(const bul& a, const bul& b) {
		BI_OP_CONSTEXPR u32 N2 = BI_N * 2;
		std::array<u32, N2 * 2> r_full{};

		// Standard O(n^2) multiply, but calculating out to 2048 bits
		for (u32 i = 0; i < N2; ++i) {
			if (!a[N2 - 1 - i]) continue;
			u32 c = 0;
			for (u32 j = 0; j < N2; ++j) {
				u64 p = (u64)a[N2 - 1 - i] * b[N2 - 1 - j] + r_full[N2 * 2 - 1 - (i + j)] + c;
				r_full[N2 * 2 - 1 - (i + j)] = (u32)p;
				c = (u32)(p >> BI_SBU32);
			}
			r_full[N2 * 2 - 1 - (i + N2)] = c;
		}

		bul r{};
		// Extract the upper 1024 bits (the high N2 limbs)
		for (u32 i = 0; i < N2; ++i) {
			r[i] = r_full[i];
		}
		return r;
	}

	// Reduces a double-width integer (bul) down to single-width (bui) mod m
	bui reduce(bul x) const {
		// 1. q_est = (x * \mu) / 2^1024
		bul q_est = mul_top_1024(x, mu);

		// 2. q_m = q_est * m \pmod{2^{1024}}
		bul m_bul = bui_to_bul(modulus);
		bul q_m = mul_low_fast(q_est, m_bul);

		// 3. r_est = x - q_m
		// Because q_est <= exact_q, x >= q_m, so this will never underflow safely
		sub_ip(x, q_m);

		bui r_low = x.low();
		bui r_high = x.high();

		// 4. Mathematical guarantee: r_est is either r or r + m.
		// Therefore, at most one subtraction is needed!
		if (!bui_is0(r_high) || cmp(r_low, modulus) >= 0)
			sub_ip(r_low, modulus);

		return r_low;
	}

	bui multiply(const bui& x, const bui& y) const {
		return reduce(mul(x, y));
	}

	bui pow(bui x, const bui& e) const {
		bui r = bui1();
		x = reduce(bui_to_bul(x));
		u32 hb1 = highest_bit(e);
		while (hb1-- > 0) {
			r = multiply(r, r);
			if (get_bit(e, hb1))
				r = multiply(r, x);
		}
		return r;
	}
};

// Convenience wrapper matching your mr_pow_mod / mr2_pow_mod style
inline bui barrett_pow_mod(const bui& x, const bui& e, const bui& m) {
	if (cmp(m, bui1()) <= 0) return bui0(); // Edge case guard
	BarrettReducer br(m);
	return br.pow(x, e);
}

// t += x * y_i
BI_ALWAYS_INLINE static u32 mul_add_acc(bui& t, const bui& x, const u32 y) {
	u64 c = 0;
	for (u32 j = 0; j < BI_N; ++j) {
		const u32 xj = x[BI_N - 1 - j];
		const u32 tj = BI_N - 1 - j;
		u64 s = (u64)t[tj] + (u64)xj * y + c;
		t[tj] = (u32)s;
		c = s >> BI_SBU32;
	}
	return (u32)c;
}

// CIOS-based Montgomery reducer (fixed R = 2^(32*BI_N) = 2^BI_BIT)
struct MontgomeryReducerCIOS {
	bui modulus;        // odd, > 1
	u32 n0prime{};      // -m^{-1} mod 2^32
	bui convertedOne{}; // R mod m
	bui r2{};           // R^2 mod m (normal domain), used for convertIn

	static BI_ALWAYS_INLINE u32 compute_n0prime(const bui& m) {
		u32 x{1}, m0{m[BI_N - 1]};
		// inverse mod 2^32
		x *= 2u - m0 * x;
		x *= 2u - m0 * x;
		x *= 2u - m0 * x;
		x *= 2u - m0 * x;
		x *= 2u - m0 * x;
		return 0u - x; // -m^{-1} mod 2^32
	}

	BI_ALWAYS_INLINE bui mul_cios(const bui& a, const bui& b) const {
		std::array<u32, BI_N> t{};
		u64 top = 0;

		for (u32 i = 0; i < BI_N; ++i) {
			u32 bi = b[BI_N - 1 - i];

			// t += a * bi
			u64 c = 0;

			for (u32 j = 0; j < BI_N; ++j) {
				u32 tj = BI_N - 1 - j;

				u64 s =
					(u64)t[tj]
					+ (u64)a[tj] * bi
					+ c;

				t[tj] = (u32)s;
				c = s >> 32;
			}

			u64 s = top + c;
			top = (u32)s;

			// q = least-significant limb * n0prime
			u32 q = (u32)((u64)t[BI_N - 1] * n0prime);

			// t += q * modulus
			c = 0;

			for (u32 j = 0; j < BI_N; ++j) {
				u32 tj = BI_N - 1 - j;

				u64 s =
					(u64)t[tj]
					+ (u64)modulus[tj] * q
					+ c;

				t[tj] = (u32)s;
				c = s >> 32;
			}

			s = top + c;
			top = (u32)s;

			// divide by beta
			for (u32 j = BI_N - 1; j > 0; --j)
				t[j] = t[j - 1];
			t[0] = (u32)top;
			top = 0;
		}

		bui r{};
		for (u32 i = 0; i < BI_N; ++i)
			r[i] = t[i];

		if (cmp(r, modulus) >= 0)
			sub_ip(r, modulus);

		return r;
	}

	BI_ALWAYS_INLINE bui mul_cios_old(const bui& a, const bui& b) const {
		bui t{};
		u64 carry = 0;
		for (u32 i = 0; i < BI_N; ++i) {
			const u32 bi = b[BI_N - 1 - i];

			// t += a * bi
			carry += mul_add_acc(t, a, bi);
			// u64 c = 0;
			// for (u32 j = 0; j < BI_N; ++j) {
			// 	const u32 aj = a[BI_N - 1 - j];
			// 	const u32 tj = BI_N - 1 - j;
			// 	u64 s = (u64)t[tj] + (u64)aj * bi + c;
			// 	t[tj] = (u32)s;
			// 	c = s >> BI_SBU32;
			// }
			// carry = (u32)((u64)carry + c);

			// q = t0 * n0' mod 2^32
			const u32 q = (u32)((u64)t[BI_N - 1] * n0prime);

			// t += m * q
			carry += mul_add_acc(t, modulus, q);
			// c = 0;
			// for (u32 j = 0; j < BI_N; ++j) {
			// 	const u32 mj = modulus[BI_N - 1 - j];
			// 	const u32 tj = BI_N - 1 - j;
			// 	u64 s = (u64)t[tj] + (u64)mj * q + c;
			// 	t[tj] = (u32)s;
			// 	c = s >> BI_SBU32;
			// }
			// carry = (u32)((u64)carry + c);

			// t >>= 32
			// TODO: shift_limb_right(t, 1);
			for (u32 j = BI_N - 1; j >= 1; --j) t[j] = t[j - 1];
			t[0] = carry;
			if (carry > (1ULL << 32) - 1) {
				printf("WUT!? %llu\n", carry);
			}
			carry >>= 32;
			// carry = 0;
		}
		if (cmp(t, modulus) >= 0) sub_ip(t, modulus);
		return t;
	}

	MontgomeryReducerCIOS(const bui& mod) : modulus(mod) {
		assert(get_bit(modulus, 0) && cmp(modulus, bui1()) > 0);
		n0prime = compute_n0prime(modulus);

		// R mod m
		bul R = bul_pow2(BI_BIT);
		convertedOne = mod_native(R, modulus);

		// R^2 mod m (normal domain)
		r2 = convertedOne;
		mul_mod_ip(r2, convertedOne, modulus);
	}

	// convert x -> xR mod m
	BI_ALWAYS_INLINE bui convertIn(bui x) const {
		x = mod_native(x, modulus);
		return mul_cios(x, r2);
	}

	// convert xR -> x
	BI_ALWAYS_INLINE bui convertOut(const bui& x) const {
		return mul_cios(x, bui1());
	}

	bui multiply(const bui& x, const bui& y) const {
		return mul_cios(x, y);
	}

	// Montgomery exponentiation: returns Montgomery-domain result
	bui pow(const bui& x, const bui& e) const {
		bui r = convertedOne;
		bui base = x;
		u32 hb = highest_bit(e);
		for (u32 i = 0; i < hb; ++i) {
			if (get_bit(e, i))
				r = multiply(r, base);
			base = multiply(base, base);
		}
		return r;
	}
};

inline bui mr_cios_pow_mod(bui x, const bui& e, const bui& m) {
	if (!is_valid_modulus(m)) return pow_mod(x, e, m);
	MontgomeryReducerCIOS mr(m);
	bui xm = mr.convertIn(x);
	bui rm = mr.pow(xm, e);
	printf("rm= %s\n", bui_to_dec(rm).c_str());
	auto out = mr.convertOut(rm);
	printf(" m= %s\n", bui_to_dec(out).c_str());
	return mr.convertOut(rm);
}

// Working CIOS, I think
// struct MontgomeryReducerCIOS2 {
// 	bui mod;      // modulus
// 	u32 n0_inv;   // -m[LSW]^{-1} mod 2^32
// 	bui r2{};
// 	MontgomeryReducerCIOS2() = default;
// 	explicit MontgomeryReducerCIOS2(const bui& m) : mod(m) {
// 		assert(mod[BI_N - 1] & 1);
//
// 		// Newton iteration for inverse mod 2^32
// 		u32 x = 1;
// 		for (u32 i = 0; i < 5; ++i)
// 			x *= 2u - mod[BI_N - 1] * x;
// 		n0_inv = 0u - x;
//
// 		bui r = shift_left_mod(bui1(), BI_BIT, mod);
// 		r2 = r;
// 		mul_mod_ip(r2, r, mod);
// 	}
//
// 	bui mul(const bui& a, const bui& b) const {
// 		u32 A[BI_N], B[BI_N], M[BI_N];
// 		for (u32 i = 0; i < BI_N; ++i) {
// 			A[i] = a[BI_N - 1 - i];
// 			B[i] = b[BI_N - 1 - i];
// 			M[i] = mod[BI_N - 1 - i];
// 		}
//
// 		u32 t[BI_N * 2 + 2]{};
// 		for (u32 i = 0; i < BI_N; ++i) {
// 			u64 carry = 0;
// 			for (u32 j = 0; j < BI_N; ++j) {
// 				u64 s = (u64)t[i + j] + (u64)A[i] * B[j] + carry;
// 				t[i + j] = (u32)s;
// 				carry = s >> 32;
// 			}
//
// 			u32 k = i + BI_N;
// 			while (carry) {
// 				if (carry > (1ULL << 32) - 1) {
// 					printf("WUT!? %llu\n", carry);
// 				}
// 				u64 s = (u64)t[k] + carry;
// 				t[k++] = (u32)s;
// 				carry = s >> 32;
// 			}
// 		}
//
// 		for (u32 i = 0; i < BI_N; ++i) {
// 			u32 mword = (u32)((u64)t[i] * n0_inv);
// 			u64 carry = 0;
//
// 			for (u32 j = 0; j < BI_N; ++j) {
// 				u64 s = (u64)t[i + j] + (u64)mword * M[j] + carry;
// 				t[i + j] = (u32)s;
// 				carry = s >> 32;
// 			}
//
// 			u32 k = i + BI_N;
// 			while (carry) {
// 				if (carry > (1ULL << 32) - 1) {
// 					printf("WUT!? %llu\n", carry);
// 				}
// 				u64 s = (u64)t[k] + carry;
// 				t[k++] = (u32)s;
// 				carry = s >> 32;
// 			}
// 		}
//
// 		bui r{};
// 		for (u32 i = 0; i < BI_N; ++i)
// 			r[BI_N - 1 - i] = t[BI_N + i];
//
// 		if (t[BI_N * 2] || cmp(r, mod) >= 0)
// 			sub_ip(r, mod);
//
// 		return r;
// 	}
//
// 	BI_ALWAYS_INLINE bui to_mont(const bui& x) const {
// 		return mul(mod_native(x, mod), r2);
// 	}
//
// 	BI_ALWAYS_INLINE bui from_mont(const bui& x) const {
// 		return mul(x, bui1());
// 	}
// };

// this is not CIOS? it's SOS (Separated Operand Scanning)
struct MontgomeryReducerSOS {
	bui m;      // modulus
	u32 n0_inv;   // -m[LSW]^{-1} mod 2^32
	bui r2{};
	MontgomeryReducerSOS() = default;
	explicit MontgomeryReducerSOS(const bui& m) : m(m) {
		assert(m[BI_N - 1] & 1);
		// Newton iteration for inverse mod 2^32
		{
			u32 x{1}, m0{m[BI_N - 1]};
			x *= 2u - m0 * x;
			x *= 2u - m0 * x;
			x *= 2u - m0 * x;
			x *= 2u - m0 * x;
			x *= 2u - m0 * x;
			n0_inv = 0u - x;
		}
		r2 = bui1();
		for (u32 i = 0; i < BI_BIT * 2; ++i)
			dbl_mod_ip(r2, m); // r2 = 2^(2*BI_BIT) mod mod
		// bui r = shift_left_mod(bui1(), BI_BIT, mod);
		// r2 = r;
		// mul_mod_ip(r2, r, mod);
	}

	bui mul(const bui& a, const bui& b) const {
		u32 t[BI_N * 2 + 2]{};
		for (u32 i = 0; i < BI_N; ++i) {
			u32 ai = a[BI_N - 1 - i];
			u64 c = 0;
			for (u32 j = 0; j < BI_N; ++j) {
				u64 s = (u64)t[i + j] + (u64)ai * b[BI_N - 1 - j] + c;
				t[i + j] = (u32)s;
				c = s >> 32;
			}
			u32 k = i + BI_N;
			while (c) {
				// if (c > (1ULL << 32) - 1) {
				// 	printf("WUT!? %llu\n", c);
				// }
				u64 s = (u64)t[k] + c;
				t[k++] = (u32)s;
				c = s >> 32;
			}
		}

		for (u32 i = 0; i < BI_N; ++i) {
			u32 mword = (u32)((u64)t[i] * n0_inv);
			u64 carry = 0;

			for (u32 j = 0; j < BI_N; ++j) {
				u64 s = (u64)t[i + j] + (u64)mword * m[BI_N - 1 - j] + carry;
				t[i + j] = (u32)s;
				carry = s >> 32;
			}

			u32 k = i + BI_N;
			while (carry) {
				// if (carry > (1ULL << 32) - 1) {
				// 	printf("WUT!? %llu\n", carry);
				// }
				u64 s = (u64)t[k] + carry;
				t[k++] = (u32)s;
				carry = s >> 32;
			}
		}

		bui r{};
		for (u32 i = 0; i < BI_N; ++i)
			r[BI_N - 1 - i] = t[BI_N + i];

		if (t[BI_N * 2] || cmp(r, m) >= 0)
			sub_ip(r, m);

		return r;
	}

	BI_ALWAYS_INLINE bui to_mont(const bui& x) const {
		return mul(mod_native(x, m), r2);
	}

	BI_ALWAYS_INLINE bui from_mont(const bui& x) const {
		return mul(x, bui1());
	}
};

bui pow_mod_mont_sos(const bui& x, const bui& e, const bui& m) {
	MontgomeryReducerSOS mr(m);
	bui base = mr.to_mont(x);
	bui result = mr.to_mont(bui1());
	u32 bits = highest_bit(e);
	while (bits-- > 0) {
		result = mr.mul(result, result);
		if (get_bit(e, bits))
			result = mr.mul(result, base);
	}
	return mr.from_mont(result);
}

struct MontgomeryReducerCIOS2 {
	bui mod;      // modulus
	u32 n0_inv;   // -m[LSW]^{-1} mod 2^32
	bui r2{};
	MontgomeryReducerCIOS2() = default;
	explicit MontgomeryReducerCIOS2(const bui& m) : mod(m) {
		assert(mod[BI_N - 1] & 1);
		// Newton iteration for inverse mod 2^32
		{
			u32 x{1}, m0{m[BI_N - 1]};
			x *= 2u - m0 * x;
			x *= 2u - m0 * x;
			x *= 2u - m0 * x;
			x *= 2u - m0 * x;
			x *= 2u - m0 * x;
			n0_inv = 0u - x;
		}

		r2 = bui1();
		for (u32 i = 0; i < BI_BIT * 2; ++i)
			dbl_mod_ip(r2, m); // r2 = 2^(2*BI_BIT) mod mod

		// bui r = shift_left_mod(bui1(), BI_BIT, mod);
		// r2 = r;
		// mul_mod_ip(r2, r, mod);
	}

	bui mul(const bui& a, const bui& b) const {
		u32 t[BI_N + 2]{};

		for (u32 i = 0; i < BI_N; ++i) {
			const u32 bi = b[BI_N - 1 - i];
			u64 carry = 0;

			for (u32 j = 0; j < BI_N; ++j) {
				const u32 aj = a[BI_N - 1 - j];
				u64 s = (u64)t[j] + (u64)aj * bi + carry;
				t[j] = (u32)s;
				carry = s >> 32;
			}

			{
				u64 s = (u64)t[BI_N] + carry;
				t[BI_N] = (u32)s;
				t[BI_N + 1] = (u32)(s >> 32);
			}

			const u32 mword = (u32)((u64)t[0] * n0_inv);

			{
				const u32 m0 = mod[BI_N - 1];
				u64 s = (u64)t[0] + (u64)mword * m0;
				carry = s >> 32;
			}

			for (u32 j = 1; j < BI_N; ++j) {
				const u32 mj = mod[BI_N - 1 - j];
				u64 s = (u64)t[j] + (u64)mword * mj + carry;
				t[j - 1] = (u32)s;
				carry = s >> 32;
			}

			{
				u64 s = (u64)t[BI_N] + carry;
				t[BI_N - 1] = (u32)s;
				carry = s >> 32;

				s = (u64)t[BI_N + 1] + carry;
				t[BI_N] = (u32)s;
				t[BI_N + 1] = (u32)(s >> 32);
			}
		}

		bui r{};
		for (u32 i = 0; i < BI_N; ++i)
			r[BI_N - 1 - i] = t[i];

		if (t[BI_N] || t[BI_N + 1] || cmp(r, mod) >= 0)
			sub_ip(r, mod);

		return r;
	}

	BI_ALWAYS_INLINE bui to_mont(const bui& x) const {
		return mul(mod_native(x, mod), r2);
	}

	BI_ALWAYS_INLINE bui from_mont(const bui& x) const {
		return mul(x, bui1());
	}
};

bui pow_mod_mont_cios2(const bui& x, const bui& e, const bui& m) {
	MontgomeryReducerCIOS2 mr(m);
	bui base = mr.to_mont(x);
	bui result = mr.to_mont(bui1());
	u32 bits = highest_bit(e);
	while (bits-- > 0) {
		result = mr.mul(result, result);
		if (get_bit(e, bits))
			result = mr.mul(result, base);
	}
	return mr.from_mont(result);
}

#endif
