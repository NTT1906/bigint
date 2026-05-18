#ifndef BIGINT_H_
#define BIGINT_H_
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <future>
#include <string>
#include <random>
#include <cctype>
#include <vector>

typedef uint32_t u32;
typedef uint64_t u64;

// #if !defined(DEBUG) && !defined(NDEBUG)
// #define NDEBUG
// #endif

// MACRO DETAIL:
// BI_BIT: fixed size of bigint in bit length
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

#define BI_USE_DIVMOD_KNUTH
#ifdef BI_NUSE_DIVMOD_KNUTH
#undef BI_USE_DIVMOD_KNUTH
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
	std::array<u32, BI_2N> limbs{};
	bul() = default;
	explicit bul(const bui& low) {
		std::copy_n(low.begin(), BI_N, limbs.begin() + BI_N);
	}
	bul(const bui& high, const bui& low) {
		std::copy_n(high.begin(), BI_N, limbs.begin());
		std::copy_n(low.begin(), BI_N, limbs.begin() + BI_N);
	}

	bui& high() { return *reinterpret_cast<bui*>(limbs.data()); }
	bui& low() { return *reinterpret_cast<bui*>(limbs.data() + BI_N); }
	bui high_copy() { return high(); }
	bui low_copy() { return low(); }
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
		bul r{}; r.limbs[BI_2N - 1] = 1;
		return r;
	}
	BI_OP_CONSTEXPR static bul from_u32(const u32 x) {
		bul r{}; r.limbs[BI_2N - 1] = x;
		return r;
	}
};

// compile-time safety checks
static_assert(std::is_trivially_copyable_v<bui>);
static_assert(std::is_trivially_copyable_v<bul>);
static_assert(sizeof(bui) == BI_SU32 * BI_N);
static_assert(sizeof(bul) == BI_SU32 * BI_2N);

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
bui shift_left_mod(const bui& x, u32 k, const bui& m);
bui shift_left_mod_bulk(bui x, u32 k, const bui& m);

bool bui_is0(const bui& x);
bool bul_is0(const bul& x);

int cmp(const bui& a, const bui& b);
int cmp(const bul& a, const bul& b);
int cmp(const bul& a, const bui& b);

void add_one_ip(bui &x);
void add_one_ip(bul &x);
void sub_one_ip(bui &x);
void sub_one_ip(bul &x);
void add_ip(bui& a, const bui& b);
void add_ip(bul& a, const bul& b);
void sub_ip(bui& a, const bui& b);
void sub_ip(bul& a, const bul& b);
bui add(bui a, const bui& b);
bul add(bul a, const bul& b);
bul add(const bui& a, const bul& b);
bul add(const bul& a, const bui& b);
bui sub(bui a, const bui& b);
bul sub(bul a, const bul& b);
void add_mod_soft_ip(bui &a, const bui &b, const bui &m);
void add_mod_soft_ip(bul &a, const bul &b, const bul &m);
void add_mod_ip(bui &a, const bui &b, const bui &m);
void add_mod_ip(bul &a, const bul &b, const bul &m);
void sub_mod_soft_ip(bui &a, const bui &b, const bui &m);
void sub_mod_soft_ip(bul &a, const bul &b, const bul &m);
void sub_mod_ip(bui &a, const bui &b, const bui &m);
void sub_mod_ip(bul &a, const bul &b, const bul &m);
void add_redc_ip(bui& a, const bui &b, const bui &m);
void add_redc_ip(bul &a, const bul &b, const bul &m);
void sub_redc_ip(bui &a, const bui &b, const bui &m);
void sub_redc_ip(bul &a, const bul &b, const bul &m);

bui nmod_native(bui x, const bui &m);
bui nmod_native(bul x, const bui &m);
void divmod_knuth(const bui &a, const bui& b, bui& quot, bui& rem);
void divmod_knuth2(const bui &a, const bui& b, bui& quot, bui& rem);
void divmod_knuth(const bul& a, const bui& b, bul& q, bui& r);
void divmod_knuth(const bul& a, const bul& b, bul& q, bul& r);

bui mod(const bui &x, const bui &m);
void mod_ip(bui &x, const bui &m);
bui mod(const bul &x, const bui &m);
void mod_ip(bul &x, const bui &m);
bul mod(const bul &x, const bul &m);
void mod_ip(bul &x, const bul &m);

void mul_ref(const bui &a, const bui &b, bul &r);
void mul_ip(bui &a, const bui &b);

void mul_mod_ip(bui &a, bui b, const bui &m);
void mul_mod_soft_ip(bui &a, const bui& b, const bui &m);
void sqr_mod_ip(bui &a, const bui &m);
void sqr_mod_soft_ip(bui &a, const bui &m);
bul sqr(const bui& a);

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
	assert(pos < BI_2N * BI_SBU32 && "Cannot set bit outside the scope of the big integer");
	u32 k = BI_2N - 1 - pos / BI_SBU32;
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
#ifndef BI_FORCE_UNROLL
	for (; i + 3 < BI_N; i += 4) {
		if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
			if (x[i  ]) return highest_bit(x[i  ]) + (BI_N - i - 1) * BI_SBU32;
			if (x[i+1]) return highest_bit(x[i+1]) + (BI_N - i - 2) * BI_SBU32;
			if (x[i+2]) return highest_bit(x[i+2]) + (BI_N - i - 3) * BI_SBU32;
			/* x[i+3] */return highest_bit(x[i+3]) + (BI_N - i - 4) * BI_SBU32;
		}
	}
#endif
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	for (; i < BI_N; ++i)
		if (x[i] != 0) return highest_bit(x[i]) + (BI_N - i - 1) * BI_SBU32;
	return 0; // all limbs zero
}

inline u32 highest_bit(const bul &x) {
	u32 i = 0;
#ifndef BI_FORCE_UNROLL
	for (; i + 3 < BI_2N; i += 4) {
		if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
			if (x[i  ]) return highest_bit(x[i  ]) + (BI_2N - i - 1) * BI_SBU32;
			if (x[i+1]) return highest_bit(x[i+1]) + (BI_2N - i - 2) * BI_SBU32;
			if (x[i+2]) return highest_bit(x[i+2]) + (BI_2N - i - 3) * BI_SBU32;
			/* x[i+3] */return highest_bit(x[i+3]) + (BI_2N - i - 4) * BI_SBU32;
		}
	}
#endif
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	for (; i < BI_2N; ++i)
		if (x[i] != 0) return highest_bit(x[i]) + (BI_2N - i - 1) * BI_SBU32;
	return 0; // all limbs zero
}

inline void bitwise_and_ip(bui &a, const bui &b) {
	for (u32 i = BI_N; i-- > 0;) a[i] &= b[i];
}

inline void bitwise_or_ip(bui &a, const bui &b) {
	for (u32 i = BI_N; i-- > 0;) a[i] |= b[i];
}

inline void bitwise_xor_ip(bui &a, const bui &b) {
	for (u32 i = BI_N; i-- > 0;) a[i] ^= b[i];
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
#ifndef BI_FORCE_UNROLL
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
#endif
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	for (; i < n; ++i) if (x[i] != 0) return n - i - 1;
	return 0;
}

// find the highest (MSB) limb
inline u32 highest_limb(const bui &x) { return highest_limb_template<BI_N>(x.data()); }

// find the highest (MSB) limb
inline u32 highest_limb(const bul &x) { return highest_limb_template<BI_2N>(x.data()); }

// Shift left by `l` whole 32-bit limbs (big-endian).
// x[0] = MSW, x[BI_N - 1] = LSW.
//
// Example (n = 5, l = 2):
//   [a0 a1 a2 a3 a4] -> [a2 a3 a4 0 0]
//
// Equivalent to division by 2^(32*l).
inline void shift_limb_left(bui &x, const u32 l) {
	if (l == 0) return;
	if (l >= BI_N) { x = {}; return; }
	std::copy(x.begin() + l, x.end(), x.begin());
	std::fill(x.end() - l, x.end(), 0);
}

// Shift right by `l` whole 32-bit limbs (big-endian).
// x[0] = MSW, x[2*BI_N - 1] = LSW.
//
// Example (n = 5, l = 1):
//   [a0 a1 a2 a3 a4] -> [0 a0 a1 a2 a3]
//
// Equivalent to division by 2^(32*l).
inline void shift_limb_right(bui &x, const u32 l) {
	if (l == 0) return;
	if (l >= BI_N) { x = {}; return; }
	std::copy_backward(x.begin(), x.end() - l, x.end());
	std::fill_n(x.begin(), l, 0);
}

// Shift left by `l` whole 32-bit limbs (big-endian).
// x[0] = MSW, x[2*BI_N - 1] = LSW.
//
// Example (n = 5, l = 2):
//   [a0 a1 a2 a3 a4] -> [a2 a3 a4 0 0]
//
// Equivalent to division by 2^(32*l).
inline void shift_limb_left(bul &x, const u32 l) {
	if (l == 0) return;
	if (l >= BI_2N) { x = {}; return; }
	std::copy(x.begin() + l, x.end(), x.begin());
	std::fill(x.end() - l, x.end(), 0);
}

// Shift right by `l` whole 32-bit limbs (big-endian).
// x[0] = MSW, x[2*BI_N - 1] = LSW.
//
// Example (n = 5, l = 1):
//   [a0 a1 a2 a3 a4] -> [0 a0 a1 a2 a3]
//
// Equivalent to division by 2^(32*l).
inline void shift_limb_right(bul &x, const u32 l) {
	if (l == 0) return;
	if (l >= BI_2N) { x = {}; return; }
	std::copy_backward(x.begin(), x.end() - l, x.end());
	std::fill_n(x.begin(), l, 0);
}

// shift left in-place (x *= 2^k)
BI_ALWAYS_INLINE void shift_left_ip_imp(u32 *x, const u32 n, const u32 k) {
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

inline void shift_left_ip(bui &x, const u32 k) { shift_left_ip_imp(x.data(), BI_N, k); }

inline void shift_left_ip(bul &x, const u32 k) { shift_left_ip_imp(x.data(), BI_2N, k); }

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
	if (limbs >= BI_2N) return {};
	u32 bits = k % BI_SBU32;
	bul r{};
	// limb-only move (toward MSW)
	std::copy_backward(x.begin() + (limbs > BI_N) * (limbs - BI_N), x.end(), r.begin() + BI_2N - limbs);
	// intra-word stitch (only if bits != 0)
	if (bits) {
		u32 c = 0, i = BI_2N;
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
	if (limbs >= BI_2N) return r;
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
	return mod(p2, m);
}

// shift left mod (r = x * 2^k mod m)
inline bui shift_left_mod_bulk(bui x, u32 k, const bui& m) {
	x = mod(x, m);
	while (k > 0) {
		u32 step = k > BI_BIT ? BI_BIT : k;
		bul p2 = shift_left_expand_fused(x, step);
		x = mod(p2, m);
		k -= step;
	}
	return x;
}

// shift left mod (r = x * 2^k mod m)
// @deprecated Use shift_left_mod2() instead
inline bui shift_left_mod(const bui& x, const u32 k, const bui& m) {
	return shift_left_mod2(x, k, m);
}

// shift right in-place (x /= 2^k)
BI_ALWAYS_INLINE void shift_right_ip_imp(u32 *x, const u32 n, const u32 k) {
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
inline void shift_right_ip(bui& x, const u32 k) { shift_right_ip_imp(x.data(), BI_N, k); }
// Big long: shift right in-place (x /= 2^k)
inline void shift_right_ip(bul& x, const u32 k) { shift_right_ip_imp(x.data(), BI_2N, k); }

// Checking if input bigint is zero
BI_ALWAYS_INLINE bool bu_is0_imp(const u32 *x, u32 n) {
#ifdef BI_NFORCE_UNROLL
	while (n >= 4) {
		n -= 4;
		if (x[n] | x[n+1] | x[n+2] | x[n+3]) return false;
	}
#endif
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	while (n-- > 0)
		if (x[n] != 0) return false;
	return true;
}

// Checking if input bui is zero
inline bool bui_is0(const bui &x) { return bu_is0_imp(x.data(), BI_N); }
// Checking if input bui is zero
inline bool bul_is0(const bul &x) { return bu_is0_imp(x.data(), BI_2N); }

// Return low-part of bul as bui
inline bui bul_low(const bul& x) { return x.low(); }
// Return high-part of bul as bui
inline bui bul_high(const bul& x) { return x.high(); }

// inline bui bul_low(const bul& x) {
// 	bui r{};
// 	std::copy(x.begin() + BI_N, x.end(), r.begin());
// 	return r;
// }
// inline bui bul_high(const bul& x) {
// 	bui r{};
// 	std::copy_n(x.begin(), BI_N, r.begin());
// 	return r;
// }

// Return new bul with low-part being input bui x
inline bul bui_to_bul(const bui& x) {
	bul r{};
	r.low() = x;
	// std::copy(x.begin(), x.end(), r.begin() + BI_N);
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
inline int cmp(const bul &a, const bul &b) { return cmp_imp(a.data(), b.data(), BI_2N); }
// Compare between bul and bui
inline int cmp(const bul& a, const bui& b) { return cmp_imp_nab(a.data(), BI_2N, b.data(), BI_N); }
// Compare between bui and bul
inline int cmp(const bui& a, const bul& b) { return cmp_imp_nab(a.data(), BI_N, b.data(), BI_2N); }

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
inline void randomize_ip(bul &x) { randomize_imp(x.data(), BI_2N); }

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
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	while (n-- > 0) {
		u64 s = (u64)a[n] + b[n] + c;
		a[n] = (u32)s;
		c = s >> BI_SBU32;
	}
	return c;
#endif
}

// Add 1 to big int
BI_ALWAYS_INLINE void add_one_ip_imp(u32* x, u32 n) { while (n-- > 0 && !++x[n]); }
BI_ALWAYS_INLINE void sub_one_ip_imp(u32* x, u32 n) { while (n-- > 0 && !x[n]--); }

inline void add_one_ip(bui &x) { add_one_ip_imp(x.data(), BI_N); }
inline void sub_one_ip(bui &x) { sub_one_ip_imp(x.data(), BI_N); }
inline void add_one_ip(bul &x) { add_one_ip_imp(x.data(), BI_2N); }
inline void sub_one_ip(bul &x) { sub_one_ip_imp(x.data(), BI_2N); }

inline void add_ip_n(u32* a, const u32* b, const u32 n) { add_ip_n_imp(a, b, n); }

[[nodiscard]] inline u32 add_ip_carry(bui &a, const bui &b) { return add_ip_n_imp(a.data(), b.data(), BI_N); }
[[nodiscard]] inline u32 add_ip_carry(bul &a, const bul &b) { return add_ip_n_imp(a.data(), b.data(), BI_2N); }

// a += b;
inline void add_ip(bui& a, const bui& b) { add_ip_n_imp(a.data(), b.data(), BI_N); }
// a += b
inline void add_ip(bul& a, const bul& b) { add_ip_n_imp(a.data(), b.data(), BI_2N); }

// r.size = 2n
inline void add_n(const u32* a, const u32* b, u32* r, const u32 n) {
	std::copy_n(a, n, r);
	add_ip_n_imp(r, b, n);
}

// r = a + b
inline bui add(bui a, const bui& b) { add_ip(a, b); return a; }
inline bul add(bul a, const bul& b) { add_ip(a, b); return a; }
inline bul add(const bui& a, const bul& b) { bul t{a}; add_ip(t, b); return t; }
inline bul add(const bul& a, const bui& b) { return add(b, a); }

// add_mod without pre-modulo-ed a
inline void add_mod_soft_ip(bui &a, const bui &b, const bui &m) { add_ip(a, b); mod_ip(a, m); }
// add_mod without pre-modulo-ed a
inline void add_mod_soft_ip(bul &a, const bul &b, const bul &m) { add_ip(a, b); mod_ip(a, m); }
inline void add_mod_ip(bui &a, const bui &b, const bui &m) { mod_ip(a, m); add_mod_soft_ip(a, b, m); }
inline void add_mod_ip(bul &a, const bul &b, const bul &m) { mod_ip(a, m); add_mod_soft_ip(a, b, m); }

// a = (a + b) redc m
inline void add_redc_ip(bui &a, const bui &b, const bui &m) {
	if (add_ip_n_imp(a.data(), b.data(), BI_N) || cmp(a, m) >= 0) {
		sub_ip(a, m);
	}
}

// a = (a + b) redc m
inline void add_redc_ip(bul &a, const bul &b, const bul &m) {
	if (add_ip_n_imp(a.data(), b.data(), BI_2N) || cmp(a, m) >= 0)
		sub_ip(a, m);
}

BI_ALWAYS_INLINE u32 sub_ip_n_imp(u32* a, const u32* b, u32 n) {
#if BI_USE_HW_INTRIN
	unsigned char br = 0;
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	while (n-- > 0)
		br = _subborrow_u32(br, a[n], b[n], &a[n]);
	return br;
#else
	u32 br = 0;
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	while (n-- > 0) {
		u64 d = (u64)a[n] - b[n] - br;
		a[n] = (u32)d;
		br = d >> BI_SBU32 & 1; // br occurs if 32nd bit is 1
	}
	return br;
#endif
}

// a -= b; // assume a > b
inline void sub_ip(bui& a, const bui& b) { sub_ip_n_imp(a.data(), b.data(), BI_N); }

inline void sub_ip(bul& a, const bul& b) { sub_ip_n_imp(a.data(), b.data(), BI_2N); }

// a -= b; // assume a > b
inline void sub_n(const u32* a, const u32* b, u32* r, u32 n) {
	std::copy_n(a, n, r);
	sub_ip_n_imp(r, b, n);
 }

inline bui sub(bui a, const bui& b) { sub_ip(a, b); return a; }
inline bul sub(bul a, const bul& b) { sub_ip(a, b); return a; }

// sub_mod without pre-modulo-ed a
inline void sub_mod_soft_ip(bui &a, const bui &b, const bui &m) { sub_ip(a, b); mod_ip(a, m); }
// sub_mod without pre-modulo-ed a
inline void sub_mod_soft_ip(bul &a, const bul &b, const bul &m) { sub_ip(a, b); mod_ip(a, m); }
inline void sub_mod_ip(bui &a, const bui &b, const bui &m) { mod_ip(a, m); sub_mod_soft_ip(a, b, m); }
inline void sub_mod_ip(bul &a, const bul &b, const bul &m) { mod_ip(a, m); sub_mod_soft_ip(a, b, m); }

inline void sub_redc_ip(bui& a, const bui& b, const bui& m) {
	if (cmp(a, b) >= 0) {
		sub_ip(a, b);
	} else {
		bui t = m, bb = b;
		sub_ip(bb, a); // bb = b - a
		sub_ip(t, bb); // t = m - (b - a)
		a = t;
	}
}

inline void sub_redc_ip(bul& a, const bul& b, const bul& m) {
	if (cmp(a, b) >= 0) {
		sub_ip(a, b);
	} else {
		bul t = m, bb = b;
		sub_ip(bb, a); // bb = b - a
		sub_ip(t, bb); // t = m - (b - a)
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
	}
}

// BI_ALWAYS_INLINE void mul_imp2(const u32* a, const u32* b, u32* r, const u32 n) {
// 	std::fill_n(r, 2 * n, 0);
// 	for (u32 i = 0; i < n; ++i) {
// 		if (a[n - 1 - i] == 0) continue;
// 		u64 c = 0;
// 		u32 k = 2 * n - 1 - i;
// 		BI_UNROLL(BI_UNROLL_THRESHOLD)
// 		for (u32 j = 0; j < n; ++j) {
// 			u64 p = (u64)a[n - 1 - i] * b[n - 1 - j] + r[k] + c;
// 			r[k--] = (u32)p;
// 			c = p >> BI_SBU32;
// 		}
// 		r[k] = c;
// 	}
// }

inline void mul_ref(const bui &a, const bui &b, bul &r) {
	mul_imp(a.data(), b.data(), r.data(), BI_N);
}

/// maybe mul_low_fast is better
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
			c = s >> BI_SBU32;
		}
	}
	return r;
}

// --- Karatsuba Multiplication ---
static constexpr u32 KARATSUBA_CUTOFF = 8;

static void karatsuba_imp(const u32* a, const u32* b, u32* r, u32 n, u32* scratch) {
	if (n <= KARATSUBA_CUTOFF) {
		mul_imp(a, b, r, n);
		return;
	}

	u32 half = n / 2;
	u32 other = n - half;

	const u32* a_hi = a;
	const u32* a_lo = a + half;
	const u32* b_hi = b;
	const u32* b_lo = b + half;

	// r[0..2*half-1] = z2 = a_hi*b_hi (MSB side)
	// r[2n-2*other..2n-1] = z0 = a_lo*b_lo (LSB side)
	u32* z0 = r + 2 * n - 2 * other;
	u32* z2 = r;
	karatsuba_imp(a_lo, b_lo, z0, other, scratch);
	karatsuba_imp(a_hi, b_hi, z2, half, scratch);

	u32 sum_len = other + 1;
	u32* sum_a = scratch;
	u32* sum_b = scratch + sum_len;
	u32* z1_raw = scratch + 2 * sum_len;
	u32* sub_scratch = z1_raw + 2 * sum_len;

	// sum_a = a_hi + a_lo (other+1 limbs, sum_a[0] = carry)
	std::copy_n(a_lo, other, sum_a + 1);
	u32 carry = add_ip_n_imp(sum_a + 1 + (other - half), a_hi, half);
	for (int i = (int)(other - half) - 1; carry && i >= 0; --i) {
		u64 s = (u64)sum_a[1 + i] + carry;
		sum_a[1 + i] = (u32)s;
		carry = (u32)(s >> BI_SBU32);
	}
	sum_a[0] = carry;

	// sum_b = b_hi + b_lo
	std::copy_n(b_lo, other, sum_b + 1);
	carry = add_ip_n_imp(sum_b + 1 + (other - half), b_hi, half);
	for (int i = (int)(other - half) - 1; carry && i >= 0; --i) {
		u64 s = (u64)sum_b[1 + i] + carry;
		sum_b[1 + i] = (u32)s;
		carry = (u32)(s >> BI_SBU32);
	}
	sum_b[0] = carry;

	// z1_raw = sum_a * sum_b
	karatsuba_imp(sum_a, sum_b, z1_raw, sum_len, sub_scratch);

	// Extract z1 = z1_raw - z0 - z2
	// z0 occupies z1_raw[2..2+2*other-1]
	u32 br = sub_ip_n_imp(z1_raw + 2, z0, 2 * other);
	for (int i = 1; br && i >= 0; --i) {
		if (z1_raw[i] < br) { z1_raw[i] = 0xFFFFFFFF; } else { z1_raw[i] -= br; br = 0; }
	}

	// z2 occupies z1_raw[2+2*(other-half)..] = z1_raw[z2_off..z2_off+2*half-1]
	u32 z2_off = 2 + 2 * (other - half);
	br = sub_ip_n_imp(z1_raw + z2_off, z2, 2 * half);
	for (int i = (int)z2_off - 1; br && i >= 0; --i) {
		if (z1_raw[i] < br) { z1_raw[i] = 0xFFFFFFFF; } else { z1_raw[i] -= br; br = 0; }
	}

	// z1 = z1_raw[2..2+2*other-1], add to r[half..half+2*other-1]
	carry = add_ip_n_imp(r + half, z1_raw + 2, 2 * other);
	for (int i = (int)half - 1; carry && i >= 0; --i) {
		u64 s = (u64)r[i] + carry;
		r[i] = (u32)s;
		carry = (u32)(s >> BI_SBU32);
	}
}

inline bul karatsuba(const bui& a, const bui& b) {
	bul r{};
	// Compute scratch size: top level needs at most 6*BI_N + a small constant
	u32 scratch_limbs = 6 * BI_N + 16;
	std::vector<u32> scratch(scratch_limbs);
	karatsuba_imp(a.data(), b.data(), r.data(), BI_N, scratch.data());
	return r;
}

inline void mul_mod_ip(bui &a, bui b, const bui &m) {
	mod_ip(a, m);
	mod_ip(b, m);
	bul r{};
	mul_ref(a, b, r);
	a = mod(r, m);
}

inline void mul_mod_soft_ip(bui &a, const bui& b, const bui &m) {
	bul r{};
	mul_ref(a, b, r);
	a = mod(r, m);
}

inline void sqr_mod_ip(bui &a, const bui &m) {
	mod_ip(a, m);
	bul r = sqr(a);
	a = mod(r, m);
}

inline void sqr_mod_soft_ip(bui &a, const bui &m) {
	bul r = sqr(a);
	a = mod(r, m);
}

// Do the exact same sliding window trick for the _ip versions:
inline void nmod_native_ip(bui& x, const bui& m) {
	long long shift = (long long)highest_bit(x) - highest_bit(m);
	if (shift < 0) return;
	bui shifted_m = m;
	shift_left_ip(shifted_m, shift);
	for (; shift >= 0; --shift) {
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
		if (cmp(x, shifted_m) >= 0)
			sub_ip(x, shifted_m);
		shift_right_ip(shifted_m, 1);
	}
}

inline bui nmod_native(bui x, const bui& m) {
	nmod_native_ip(x, m);
	return x;
}

inline bui nmod_native(bul x, const bui& m) {
	nmod_native_ip(x, m);
	return x.low();
}

inline bui mod(const bui &x, const bui &m) {
#ifdef BI_USE_DIVMOD_KNUTH
	bui q, r;
	divmod_knuth(x, m, q, r);
	return r;
#else
	return nmod_native(x, m);
#endif
}

inline void mod_ip(bui &x, const bui &m) { x = mod(x, m); }

inline bui mod(const bul &x, const bui &m) {
#ifdef BI_USE_DIVMOD_KNUTH
	bul q;
	bui r;
	divmod_knuth(x, m, q, r);
	return r;
#else
	return nmod_native(x, m);
#endif
}

inline void mod_ip(bul &x, const bui &m) { x.high() = {}; x.low() = mod(x, m); }

inline bul mod(const bul &x, const bul &m) {
#ifdef BI_USE_DIVMOD_KNUTH
	bul q;
	bul r;
	divmod_knuth(x, m, q, r);
	return r;
#else
	return nmod_native(x, m);
#endif
}

inline void mod_ip(bul &x, const bul &m) { x = mod(x, m); }

/// <r = x*x> Return a squared result of input x
BI_ALWAYS_INLINE void sqr_imp(const u32* a, u32* r, const u32 n) {
	std::fill_n(r, 2 * n, 0);

	// 1. Calculate symmetrical cross-products only ONCE (i < j)
	for (u32 i = n; i-- > 1;) {
		if (!a[i]) continue;
		u64 c = 0;
		for (u32 j = i; j-- > 0;) {
			u64 p = (u64)a[i] * a[j] + r[i + j + 1] + c;
			r[i + j + 1] = (u32)p;
			c = p >> BI_SBU32;
		}
		r[i] = c;
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

/// <r = x*x> Return a squared result of input x
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
	if (hl == 0 && a[BI_2N - 1] == 0) return;
	for (u32 i = BI_2N - 1 - hl; i < BI_2N; ++i)
		q[i] = u32_divmod_single(r, a[i], b, &r);
}

inline u32 u32_mod(bul x, const u32 m) {
	u32 r = 0;
	u32 hl = highest_limb(x);
	if (hl == 0 && x[BI_2N - 1] == 0) return 0;
	for (u32 i = BI_2N - 1 - hl; i < BI_2N; ++i)
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

// Return pow_mod (x^e % m)
inline bui pow_mod2(bui x, const bui& e, const bui &m) {
	mod_ip(x, m);
	bui r = bui1();
	u32 hb = highest_bit(e);
	for (u32 i = 0; i < hb; ++i) {
		if (get_bit(e, i))
			mul_mod_soft_ip(r, x, m);
		sqr_mod_soft_ip(x, m);
	}
	return r;
}

// Return pow_mod (x^e % m) using sliding window exponentiation
inline bui pow_mod_window(bui x, const bui& e, const bui &m) {
	mod_ip(x, m);

	u32 hb = highest_bit(e);
	if (hb <= 1) {
		if (hb == 1 && get_bit(e, 0)) return x;
		return bui1();
	}

	u32 w = 4;
	if (hb > 200) w = 5;
	if (hb > 600) w = 6;
	if (hb > 1200) w = 7;

	const u32 tsz = 1u << (w - 1);
	std::vector<bui> table(tsz);
	table[0] = x;
	{
		bui x2 = x;
		sqr_mod_soft_ip(x2, m);
		for (u32 i = 1; i < tsz; ++i) {
			table[i] = table[i - 1];
			mul_mod_soft_ip(table[i], x2, m);
		}
	}

	bui r = bui1();
	u32 i = hb;
	while (i-- > 0) {
		if (!get_bit(e, i)) {
			sqr_mod_soft_ip(r, m);
			continue;
		}
		u32 s = i >= w ? i - w + 1 : 0;
		while (s < i && !get_bit(e, s)) ++s;
		u32 len = i - s + 1;
		u32 v = 0;
		for (u32 j = 0; j < len; ++j)
			v = (v << 1) | get_bit(e, i - j);

		for (u32 j = 0; j < len; ++j)
			sqr_mod_soft_ip(r, m);
		mul_mod_soft_ip(r, table[v >> 1], m);

		i = s;
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
		u32 r_idx = BI_2N - 1 - (j + n);

		u32 u_jn = r[r_idx];
		u32 u_jn1 = (r_idx + 1 < BI_2N) ? r[r_idx + 1] : 0;
		u32 u_jn2 = (r_idx + 2 < BI_2N) ? r[r_idx + 2] : 0;

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

	// 4. Denormalize the remainder
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
		shift_left_ip_imp(d.data(), BI_N, norm_shift);
		shift_left_ip_imp(u.data(), BI_N + 2, norm_shift);
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

		for (u32 i = 0; i < n; ++i) {
			u32 u_i = u_idx + n - i;
			u32 d_i = BI_N - 1 - i;

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
		shift_right_ip_imp(u.data(), BI_N + 2, norm_shift);

	rem = {};
	std::copy_n(u.begin() + 2, BI_N, rem.begin());
}

BI_ALWAYS_INLINE void u32_divmod_imp(const u32* a, u32 na, u32 d, u32* q, u32* r_out) {
	u32 r = 0;
	u32 a_lead_pow = highest_limb_imp(a, na);
	if (a_lead_pow == 0 && a[na - 1] == 0) {
		*r_out = 0;
		return;
	}
	for (u32 i = na - 1 - a_lead_pow; i < na; ++i)
		q[i] = u32_divmod_single(r, a[i], d, &r);
	*r_out = r;
}

inline void divmod_knuth_imp(const u32* a, const u32 na, const u32* b, const u32 nb, u32* q, const u32 nq, u32* r, const u32 nr) {
	assert(!bu_is0_imp(b, nb));
	int cm = cmp_imp_nab(a, na, b, nb);
	if (cm < 0) {
		memset(q, 0, nq * BI_SU32);
		memset(r, 0, nr * BI_SU32);
		if (nr >= na)
			memcpy(r + nr - na, a, na * BI_SU32);
		else
			memcpy(r, a + na - nr, nr * BI_SU32);
		return;
	}
	if (cm == 0) {
		memset(q, 0, nq * BI_SU32);
		q[nq - 1] = 1;
		memset(r, 0, nr * BI_SU32);
		return;
	}

	u32 d_lead_pow = highest_limb_imp(b, nb);
	if (d_lead_pow == 0 && b[nb - 1] != 0) {
		memset(q, 0, nq * BI_SU32);
		u32 r32 = 0;
		u32_divmod_imp(a, na, b[nb - 1], q, &r32);
		memset(r, 0, nr * BI_SU32);
		r[nr - 1] = r32;
		return;
	}

	const u32 n = d_lead_pow + 1;
	const u32 d_start = nb - n;

	std::vector<u32> u(na + 2, 0);
	std::vector<u32> d(nb, 0);
	memcpy(d.data(), b, nb * BI_SU32);
	memcpy(u.data() + 2, a, na * BI_SU32);

	u32 d0 = d[d_start];
	const u32 norm_shift = d0 == 0 ? 0 : BI_SBU32 - highest_bit(d0);

	if (norm_shift > 0) {
		shift_left_ip_imp(d.data(), nb, norm_shift);
		shift_left_ip_imp(u.data(), na + 2, norm_shift);
	}

	d0 = d[d_start];
	const u32 d1 = n > 1 ? d[d_start + 1] : 0;

	memset(q, 0, nq * BI_SU32);
	const u32 u_lead_pow = highest_limb_imp(u.data(), na + 2);
	int j = (int)u_lead_pow - (int)d_lead_pow + 1;

	while (j-- > 0) {
		const u32 u_idx = na + 1 - (j + n);

		u32 u_jn = u[u_idx];
		u32 u_jn1 = u[u_idx + 1];
		u32 u_jn2 = (u_idx + 2 < na + 2) ? u[u_idx + 2] : 0;

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
		for (u32 i = 0; i < n; ++i) {
			u32 u_i = u_idx + n - i;
			u32 d_i = nb - 1 - i;

			u64 sub = qhat * d[d_i] + borrow;
			borrow = (sub >> BI_SBU32) + (u[u_i] < (u32)sub);
			u[u_i] -= (u32)sub;
		}

		bool is_negative = borrow > u[u_idx];
		u[u_idx] -= (u32)borrow;

		u32 q_idx = nq - 1 - j;
		if (q_idx < nq)
			q[q_idx] = (u32)qhat;

		if (is_negative) {
			if (q_idx < nq)
				--q[q_idx];
			u32 carry = add_ip_n_imp(u.data() + u_idx + 1, d.data() + d_start, n);
			u[u_idx] += carry;
		}
	}

	if (norm_shift > 0)
		shift_right_ip_imp(u.data(), na + 2, norm_shift);

	memset(r, 0, nr * BI_SU32);
	if (nr >= na)
		memcpy(r + nr - na, u.data() + 2, na * BI_SU32);
	else
		memcpy(r, u.data() + 2 + na - nr, nr * BI_SU32);
}

template <u32 na, u32 nb, u32 nq, u32 nr>
void divmod_knuth_template(const u32* a, const u32* b, u32* q, u32* r) {
	assert(!bu_is0_imp(b, nb));
	int cm = cmp_imp_nab(a, na, b, nb);
	if (cm < 0) {
		std::fill_n(q, nq, 0);
		std::fill_n(r, nr, 0);
		if (nr >= na)
			std::copy_n(a, na, r + nr - na);
		else
			std::copy_n(a + na - nr, nr, r);
		return;
	}
	if (cm == 0) {
		std::fill_n(q, nq, 0);
		q[nq - 1] = 1;
		std::fill_n(r, nr, 0);
		return;
	}

	u32 d_lead_pow = highest_limb_imp(b, nb);
	if (d_lead_pow == 0 && b[nb - 1] != 0) {
		std::fill_n(q, nq, 0);
		u32 r32 = 0;
		u32_divmod_imp(a, na, b[nb - 1], q, &r32);
		std::fill_n(r, nr, 0);
		r[nr - 1] = r32;
		return;
	}

	const u32 n = d_lead_pow + 1;
	const u32 d_start = nb - n;

	std::array<u32, na + 2> u{};
	std::array<u32, nb> d{};
	std::copy_n(b, nb, d.begin());
	std::copy_n(a, na, u.begin() + 2);

	u32 d0 = d[d_start];
	const u32 norm_shift = d0 == 0 ? 0 : BI_SBU32 - highest_bit(d0);

	if (norm_shift > 0) {
		shift_left_ip_imp(d.data(), nb, norm_shift);
		shift_left_ip_imp(u.data(), na + 2, norm_shift);
	}

	d0 = d[d_start];
	const u32 d1 = n > 1 ? d[d_start + 1] : 0;

	std::fill_n(q, nq, 0);
	const u32 u_lead_pow = highest_limb_imp(u.data(), na + 2);
	int j = (int)u_lead_pow - (int)d_lead_pow + 1;

	while (j-- > 0) {
		const u32 u_idx = na + 1 - (j + n);

		u32 u_jn = u[u_idx];
		u32 u_jn1 = u[u_idx + 1];
		u32 u_jn2 = (u_idx + 2 < na + 2) ? u[u_idx + 2] : 0;

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
		for (u32 i = 0; i < n; ++i) {
			u32 u_i = u_idx + n - i;
			u32 d_i = nb - 1 - i;

			u64 sub = qhat * d[d_i] + borrow;
			borrow = (sub >> BI_SBU32) + (u[u_i] < (u32)sub);
			u[u_i] -= (u32)sub;
		}

		bool is_negative = borrow > u[u_idx];
		u[u_idx] -= (u32)borrow;

		u32 q_idx = nq - 1 - j;
		if (q_idx < nq)
			q[q_idx] = (u32)qhat;

		if (is_negative) {
			if (q_idx < nq)
				--q[q_idx];
			u32 carry = add_ip_n_imp(u.data() + u_idx + 1, d.data() + d_start, n);
			u[u_idx] += carry;
		}
	}

	if (norm_shift > 0)
		shift_right_ip_imp(u.data(), na + 2, norm_shift);

	std::fill_n(r, nr, 0);
	if (nr >= na)
		std::copy_n(u.begin() + 2, na, r + nr - na);
	else
		std::copy_n(u.begin() + 2 + na - nr, nr, r);
}

inline void divmod_knuth(const bul& a, const bui& b, bul& q, bui& r) {
	divmod_knuth_template<BI_2N, BI_N, BI_2N, BI_N>(a.data(), b.data(), q.data(), r.data());
}

inline void divmod_knuth(const bul& a, const bul& b, bul& q, bul& r) {
	divmod_knuth_template<BI_2N, BI_2N, BI_2N, BI_2N>(a.data(), b.data(), q.data(), r.data());
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
inline void dbl_ip(bul &x) { dbl_ip_n_imp(x.data(), BI_2N); }

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
	parts.reserve(BI_2N);
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
// q = a / d (quotient), returns remainder r = a % d.
// Requires: d != 0
inline u32 u32_divmod_bul(const bul &a, u32 d, bul &q) {
	u64 rem = 0;
	for (u32 i = 0; i < BI_2N; ++i) q[i] = 0;
	for (u32 i = 0; i < BI_2N; ++i) {
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
		c = p >> BI_SBU32;
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

// Extended Euclidean algorithm
inline bool mod_inverse_old(bui a, const bui &m, bui &inv_out) {
	// invalid modulus or zero
	if (bui_is0(m)) return false;
	if (cmp(a, m) >= 0) a = mod(a, m);
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

		auto qtm_rem = mod(prod, m); // qtm_rem = (prod) % m

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

// Fast modular inverse: binary GCD for odd m, extended Euclidean for even m.
inline bool mod_inverse(const bui& a_in, const bui& m, bui& inv_out) {
	if (bui_is0(m)) return false;
	bui a = a_in;
	if (cmp(a, m) >= 0) a = mod(a, m);
	if (bui_is0(a)) return false;

	if (get_bit(m, 0)) {
		// --- m is odd: binary extended GCD (fast path) ---
		// Invariant: inv_out obeys a * inv_out ≡ u (mod m)
		bui u = a, v = m;
		bui x1 = bui1(), x2 = bui0();

		while (!bui_is0(u)) {
			// Remove factors of 2 from u
			while (!get_bit(u, 0)) {
				shift_right_ip(u, 1);
				if (get_bit(x1, 0)) {
					u32 carry = add_ip_n_imp(x1.data(), m.data(), BI_N);
					shift_right_ip(x1, 1);
					if (carry) set_bit_ip(x1, BI_BIT - 1, 1);
				} else {
					shift_right_ip(x1, 1);
				}
			}
			// Remove factors of 2 from v
			while (!get_bit(v, 0)) {
				shift_right_ip(v, 1);
				if (get_bit(x2, 0)) {
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
				if (cmp(x1, x2) >= 0) {
					sub_ip(x1, x2);
				} else {
					bui tmp = x2;
					sub_ip(tmp, x1);
					x1 = m;
					sub_ip(x1, tmp);
				}
			} else {
				sub_ip(v, u);
				if (cmp(x2, x1) >= 0) {
					sub_ip(x2, x1);
				} else {
					bui tmp = x1;
					sub_ip(tmp, x2);
					x2 = m;
					sub_ip(x2, tmp);
				}
			}
		}
		if (cmp(v, bui1()) != 0) return false;
		inv_out = x2;
		return true;
	}

	// --- m is even: extended Euclidean algorithm ---
	bui r0 = m, r1 = a;
	bui t0{}, t1 = bui1();

	while (!bui_is0(r1)) {
		bui q{}, rem{};
		divmod(r0, r1, q, rem);
		r0 = r1;
		r1 = rem;

		// qt = (q * t1) % m
		bul prod{};
		mul_ref(q, t1, prod);
		bui qt = mod(prod, m);

		// tnew = (t0 - qt) mod m
		bui tnew{};
		if (cmp(t0, qt) >= 0) {
			tnew = t0;
			sub_ip(tnew, qt);
		} else {
			sub_ip(qt, t0);   // qt = qt - t0
			tnew = m;
			sub_ip(tnew, qt); // tnew = m - (qt - t0)
		}
		t0 = t1;
		t1 = tnew;
	}

	if (cmp(r0, bui1()) != 0) return false;
	inv_out = t0;
	return true;
}

inline bool mod_inverse_binary(bui a, const bui& m, bui& inv_out) {
	if (bui_is0(a) || bui_is0(m)) return false;
	if (cmp(a, m) >= 0) a = mod(a, m);
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
		convertedOne = mod(reducer, modulus);
		mod_inverse(convertedOne, modulus, reciprocal); // reducer^-1 mod modulus (fast path: odd m → binary GCD)

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
		u32 c = add_ip_n_imp(product.data(), tmp2.data(), BI_2N);
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

// /**
//  * @brief Barrett modular arithmetic helper for fixed-size big integers.
//  * Precomputes mu = floor(2^1024 / m) to replace expensive division
//  * with multiplication and bit shifts. Ideal for when the modulus
//  * is fixed across many operations.
//  */
// struct BarrettReducer {
// 	bui modulus;
// 	bul mu{}; // Precomputed 2^1024 / m
// 	BarrettReducer(const bui& m) : modulus(m) {
// 		assert(cmp(m, bui1()) > 0 && "Modulus must be >= 2 for Barrett reduction");
// 		mu = compute_mu(m);
// 	}
//
// 	// Precomputes \mu = 2^{1024} / m
// 	static bul compute_mu(const bui& m) {
// 		bul q{};
// 		bul r{}; // Using bul for remainder to prevent overflow during shift_left_ip
// 		bul m_bul = bui_to_bul(m);
//
// 		// 1. Calculate exactly how many bits we can safely skip
// 		u32 m_bits = highest_bit(m) + 1;
//
// 		// 2. Fast-forward the remainder to 2^(m_bits)
// 		set_bit_ip(r, m_bits, 1);
//
// 		// 3. Run the loop only for the remaining bits (usually ~512 iterations instead of 1024)
// 		for (int i = BI_BIT * 2 - m_bits; i >= 0; --i) {
// 			if (cmp(r, m_bul) >= 0) {
// 				sub_ip(r, m_bul);
// 				set_bit_ip(q, i, 1);
// 			}
// 			if (i > 0)
// 				shift_left_ip(r, 1);
// 		}
// 		return q;
// 	}
//
// 	// 1024-bit x 1024-bit -> Returns the Top 1024-bits
// 	static bul mul_top_1024(const bul& a, const bul& b) {
// 		static constexpr u32 N2 = BI_2N;
// 		std::array<u32, N2 * 2> r_full{};
//
// 		// Standard O(n^2) multiply, but calculating out to 2048 bits
// 		for (u32 i = 0; i < N2; ++i) {
// 			if (!a[N2 - 1 - i]) continue;
// 			u32 c = 0;
// 			for (u32 j = 0; j < N2; ++j) {
// 				u64 p = (u64)a[N2 - 1 - i] * b[N2 - 1 - j] + r_full[N2 * 2 - 1 - (i + j)] + c;
// 				r_full[N2 * 2 - 1 - (i + j)] = (u32)p;
// 				c = (u32)(p >> BI_SBU32);
// 			}
// 			r_full[N2 * 2 - 1 - (i + N2)] = c;
// 		}
//
// 		bul r{};
// 		// Extract the upper 1024 bits (the high N2 limbs)
// 		for (u32 i = 0; i < N2; ++i) {
// 			r[i] = r_full[i];
// 		}
// 		return r;
// 	}
//
// 	// Reduces a double-width integer (bul) down to single-width (bui) mod m
// 	bui reduce(bul x) const {
// 		// 1. q_est = (x * \mu) / 2^1024
// 		bul q_est = mul_top_1024(x, mu);
//
// 		// 2. q_m = q_est * m \pmod{2^{1024}}
// 		bul m_bul = bui_to_bul(modulus);
// 		bul q_m = mul_low_fast(q_est, m_bul);
//
// 		// 3. r_est = x - q_m
// 		// Because q_est <= exact_q, x >= q_m, so this will never underflow safely
// 		sub_ip(x, q_m);
//
// 		bui r_low = x.low();
// 		bui r_high = x.high();
//
// 		// 4. Mathematical guarantee: r_est is either r or r + m.
// 		// Therefore, at most one subtraction is needed!
// 		if (!bui_is0(r_high) || cmp(r_low, modulus) >= 0)
// 			sub_ip(r_low, modulus);
//
// 		return r_low;
// 	}
//
// 	bui multiply(const bui& x, const bui& y) const {
// 		return reduce(mul(x, y));
// 	}
//
// 	bui pow(bui x, const bui& e) const {
// 		bui r = bui1();
// 		x = reduce(bui_to_bul(x));
// 		u32 hb1 = highest_bit(e);
// 		while (hb1-- > 0) {
// 			r = multiply(r, r);
// 			if (get_bit(e, hb1))
// 				r = multiply(r, x);
// 		}
// 		return r;
// 	}
// };
//
// // Convenience wrapper matching your mr_pow_mod / mr2_pow_mod style
// inline bui barrett_pow_mod(const bui& x, const bui& e, const bui& m) {
// 	if (cmp(m, bui1()) <= 0) return bui0(); // Edge case guard
// 	BarrettReducer br(m);
// 	return br.pow(x, e);
// }

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

struct MontgomeryReducerSOS {
	bui m;
	u32 n0_inv{}; // -m[LSW]^{-1} mod 2^32
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
		// R = 2^BI_BIT mod m
		r2 = shift_left_mod(bui1(), BI_BIT, m);
		// R^2 = R * 2^BI_BIT mod m = 2^{2*BI_BIT} mod m
		for (u32 i = 0; i < BI_BIT; ++i)
			dbl_mod_ip(r2, m);
	}

	bui mul(const bui& a, const bui& b) const {
		u32 t[BI_2N + 2]{};
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
				c = s >> BI_SBU32;
			}

			u32 k = i + BI_N;
			while (carry) {
				// if (carry > (1ULL << 32) - 1) {
				// 	printf("WUT!? %llu\n", carry);
				// }
				u64 s = (u64)t[k] + carry;
				t[k++] = (u32)s;
				c = s >> BI_SBU32;
			}
		}

		bui r{};
		for (u32 i = 0; i < BI_N; ++i)
			r[BI_N - 1 - i] = t[BI_N + i];

		if (t[BI_2N] || cmp(r, m) >= 0)
			sub_ip(r, m);

		return r;
	}

	BI_ALWAYS_INLINE bui to_mont(const bui& x) const {
		return mul(mod(x, m), r2);
	}

	BI_ALWAYS_INLINE bui from_mont(const bui& x) const {
		return mul(x, bui1());
	}
};

inline bui pow_mod_mont_sos(const bui& x, const bui& e, const bui& m) {
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
	bui m;      // modulus
	u32 n0_inv{};   // -m[LSW]^{-1} mod 2^32
	bui r2{};
	MontgomeryReducerCIOS2() = default;
	explicit MontgomeryReducerCIOS2(const bui& m) : m(m) {
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

		// // R = 2^BI_BIT mod m
		// r2 = shift_left_mod(bui1(), BI_BIT, m);
		// // R^2 = R * 2^BI_BIT mod m = 2^{2*BI_BIT} mod m
		// for (u32 i = 0; i < BI_BIT; ++i)
		// 	dbl_mod_ip(r2, m);
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
				carry = s >> BI_SBU32;
			}

			{
				u64 s = (u64)t[BI_N] + carry;
				t[BI_N] = (u32)s;
				t[BI_N + 1] = (u32)(s >> BI_SBU32);
			}

			const u32 mword = (u32)((u64)t[0] * n0_inv);

			{
				const u32 m0 = m[BI_N - 1];
				u64 s = (u64)t[0] + (u64)mword * m0;
				carry = s >> BI_SBU32;
			}

			for (u32 j = 1; j < BI_N; ++j) {
				const u32 mj = m[BI_N - 1 - j];
				u64 s = (u64)t[j] + (u64)mword * mj + carry;
				t[j - 1] = (u32)s;
				carry = s >> BI_SBU32;
			}

			{
				u64 s = (u64)t[BI_N] + carry;
				t[BI_N - 1] = (u32)s;
				carry = s >> BI_SBU32;

				s = (u64)t[BI_N + 1] + carry;
				t[BI_N] = (u32)s;
				t[BI_N + 1] = (u32)(s >> BI_SBU32);
			}
		}

		bui r{};
		for (u32 i = 0; i < BI_N; ++i)
			r[BI_N - 1 - i] = t[i];

		if (t[BI_N] || t[BI_N + 1] || cmp(r, m) >= 0)
			sub_ip(r, m);

		return r;
	}

	BI_ALWAYS_INLINE bui to_mont(const bui& x) const {
		return mul(mod(x, m), r2);
	}

	BI_ALWAYS_INLINE bui from_mont(const bui& x) const {
		return mul(x, bui1());
	}
};

inline bui pow_mod_mont_cios2(const bui& x, const bui& e, const bui& m) {
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

struct MontgomeryReducerCIOS3 {
	bui m;
	u32 n0_inv{};
	bui r2{};
	MontgomeryReducerCIOS3() = default;
	explicit MontgomeryReducerCIOS3(const bui& m) : m(m) {
		assert(m[BI_N - 1] & 1);
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

		// // R = 2^BI_BIT mod m
		// r2 = shift_left_mod(bui1(), BI_BIT, m);
		// // R^2 = R * 2^BI_BIT mod m = 2^{2*BI_BIT} mod m
		// for (u32 i = 0; i < BI_BIT; ++i)
		// 	dbl_mod_ip(r2, m);
	}

	bui mul(const bui& a, const bui& b) const {
		std::array<u32, BI_N + 2> t{};

		for (u32 i = 0; i < BI_N; ++i) {
			const u32 bi = b[BI_N - 1 - i];

			u64 A, C;
			u32 t0_new;

			{
				const u32 a0 = a[BI_N - 1];
				u64 s = (u64)t[0] + (u64)a0 * bi;
				A = s >> BI_SBU32;
				t0_new = (u32)s;
			}

			u32 q = (u32)((u64)t0_new * n0_inv);

			{
				const u32 m0 = m[BI_N - 1];
				u64 s = (u64)t0_new + (u64)q * m0;
				C = s >> BI_SBU32;
			}

			BI_UNROLL(BI_UNROLL_THRESHOLD)
			for (u32 j = 1; j < BI_N; ++j) {
				u32 aj = a[BI_N - 1 - j];
				u64 s1 = (u64)t[j] + (u64)aj * bi + A;
				A = s1 >> BI_SBU32;
				u32 tj_new = (u32)s1;

				u32 mj = m[BI_N - 1 - j];
				u64 s2 = (u64)tj_new + (u64)q * mj + C;
				C = s2 >> BI_SBU32;
				t[j - 1] = (u32)s2;
			}

			u64 s = (u64)t[BI_N] + C + A;
			t[BI_N - 1] = (u32)s;
			A = s >> BI_SBU32;

			s = (u64)t[BI_N + 1] + A;
			t[BI_N] = (u32)s;
			t[BI_N + 1] = (u32)(s >> BI_SBU32);
		}

		bui r{};
		for (u32 i = 0; i < BI_N; ++i)
			r[BI_N - 1 - i] = t[i];

		if (t[BI_N] || t[BI_N + 1] || cmp(r, m) >= 0)
			sub_ip(r, m);

		return r;
	}

	bui sqr(const bui& a) const {
		std::array<u32, BI_N + 2> t{};

		for (u32 i = 0; i < BI_N; ++i) {
			const u32 bi = a[BI_N - 1 - i];

			u64 A, C;
			u32 t0_new;

			{
				const u32 a0 = a[BI_N - 1];
				u64 s = (u64)t[0] + (u64)a0 * bi;
				A = s >> 32;
				t0_new = (u32)s;
			}

			u32 q = (u32)((u64)t0_new * n0_inv);

			{
				const u32 m0 = m[BI_N - 1];
				u64 s = (u64)t0_new + (u64)q * m0;
				C = s >> 32;
			}

			BI_UNROLL(BI_UNROLL_THRESHOLD)
			for (u32 j = 1; j < BI_N; ++j) {
				u32 aj = a[BI_N - 1 - j];
				u64 s1 = (u64)t[j] + (u64)aj * bi + A;
				A = s1 >> 32;
				u32 tj_new = (u32)s1;

				u32 mj = m[BI_N - 1 - j];
				u64 s2 = (u64)tj_new + (u64)q * mj + C;
				C = s2 >> 32;
				t[j - 1] = (u32)s2;
			}

			u64 s = (u64)t[BI_N] + C + A;
			t[BI_N - 1] = (u32)s;
			A = s >> BI_SBU32;
			s = (u64)t[BI_N + 1] + A;
			t[BI_N] = (u32)s;
			t[BI_N + 1] = (u32)(s >> 32);
		}

		bui r{};
		for (u32 i = 0; i < BI_N; ++i)
			r[BI_N - 1 - i] = t[i];

		if (t[BI_N] || t[BI_N + 1] || cmp(r, m) >= 0)
			sub_ip(r, m);

		return r;
	}

	BI_ALWAYS_INLINE bui to_mont(const bui& x) const {
		return mul(mod(x, m), r2);
	}

	BI_ALWAYS_INLINE bui from_mont(const bui& x) const {
		return mul(x, bui1());
	}
};

inline bui pow_mod_mont_cios3(const bui& x, const bui& e, const bui& m) {
	MontgomeryReducerCIOS3 mr(m);
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

// Return pow_mod (x^e % m) using sliding window + Montgomery CIOS3
inline bui pow_mod_mont_window(const bui& x, const bui& e, const bui& m) {
	MontgomeryReducerCIOS3 mr(m);

	u32 hb = highest_bit(e);
	if (hb <= 1) {
		if (hb == 1 && get_bit(e, 0)) return mod(x, m);
		return bui1();
	}

	u32 w = 4;
	if (hb > 200) w = 5;
	if (hb > 600) w = 6;
	if (hb > 1200) w = 7;

	const u32 tsz = 1u << (w - 1);
	std::vector<bui> table(tsz);
	bui base = mr.to_mont(x);
	table[0] = base;
	bui base2 = mr.mul(base, base);
	for (u32 i = 1; i < tsz; ++i)
		table[i] = mr.mul(table[i - 1], base2);

	bui r = mr.to_mont(bui1());
	u32 i = hb;
	while (i-- > 0) {
		if (!get_bit(e, i)) {
			r = mr.mul(r, r);
			continue;
		}

		u32 s = i >= w ? i - w + 1 : 0;
		while (s < i && !get_bit(e, s)) ++s;

		u32 len = i - s + 1;
		u32 v = 0;
		for (u32 j = 0; j < len; ++j)
			v = (v << 1) | get_bit(e, i - j);

		for (u32 j = 0; j < len; ++j)
			r = mr.mul(r, r);
		r = mr.mul(r, table[v >> 1]);

		i = s;
	}

	return mr.from_mont(r);
}

#endif
