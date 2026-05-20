#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <type_traits>
#include <vector>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__GNUC__) || defined(__clang__)
#if defined(__x86_64__) || defined(__i386__)
#include <x86intrin.h>
#endif
#endif

template <typename Uw>
struct double_width;

template <>
struct double_width<uint32_t> {
	using type = uint64_t;
};

template <>
struct double_width<uint64_t> {
	using type = unsigned __int128;
};

template <typename Uw, bool UseHw>
struct case_config {
	using uw = Uw;
	using udw = typename double_width<Uw>::type;
	static constexpr unsigned bits = sizeof(uw) * 8;
	static constexpr bool use_hw = UseHw;
};

template <typename Cfg>
inline unsigned char i_addcarry_case(unsigned char c, typename Cfg::uw a, typename Cfg::uw b, typename Cfg::uw* p) {
	using uw = typename Cfg::uw;
	using udw = typename Cfg::udw;
	if constexpr (Cfg::use_hw) {
		if constexpr (sizeof(uw) == 8) {
			return _addcarry_u64(
				c,
				static_cast<unsigned long long>(a),
				static_cast<unsigned long long>(b),
				reinterpret_cast<unsigned long long*>(p)
			);
		} else {
			return _addcarry_u32(
				c,
				static_cast<unsigned int>(a),
				static_cast<unsigned int>(b),
				reinterpret_cast<unsigned int*>(p)
			);
		}
	} else {
		udw s = (udw)a + b + c;
		*p = (uw)s;
		return (unsigned char)(s >> Cfg::bits);
	}
}

template <typename Cfg>
inline typename Cfg::uw add_ip_n_imp_case(typename Cfg::uw* a, const typename Cfg::uw* b, size_t n) {
	unsigned char c = 0;
	while (n-- > 0)
		c = i_addcarry_case<Cfg>(c, a[n], b[n], &a[n]);
	return c;
}

template <typename Uw>
inline Uw ref_add_ip_n_imp(Uw* a, const Uw* b, size_t n) {
	using udw = typename double_width<Uw>::type;
	constexpr unsigned bits = sizeof(Uw) * 8;
	Uw c = 0;
	while (n-- > 0) {
		udw s = (udw)a[n] + b[n] + c;
		a[n] = (Uw)s;
		c = (Uw)(s >> bits);
	}
	return c;
}

template <typename Cfg>
double run_bench(const char* label, size_t limbs, int dataset_size, int iterations, uint64_t seed) {
	using uw = typename Cfg::uw;
	std::vector<std::vector<uw>> a(dataset_size, std::vector<uw>(limbs));
	std::vector<std::vector<uw>> b(dataset_size, std::vector<uw>(limbs));

	std::mt19937_64 gen(seed);
	std::uniform_int_distribution<uw> dist(0, std::numeric_limits<uw>::max());
	for (int i = 0; i < dataset_size; ++i) {
		for (size_t j = 0; j < limbs; ++j) {
			a[i][j] = dist(gen);
			b[i][j] = dist(gen);
		}
	}

	{
		auto x = a[0];
		auto y = a[0];
		uw c1 = add_ip_n_imp_case<Cfg>(x.data(), b[0].data(), limbs);
		uw c2 = ref_add_ip_n_imp(y.data(), b[0].data(), limbs);
		if (c1 != c2 || x != y) {
			std::cerr << "Correctness check failed for " << label << "\n";
			std::exit(1);
		}
	}

	volatile uw sink = 0;
	auto start = std::chrono::high_resolution_clock::now();
	for (int iter = 0; iter < iterations; ++iter) {
		for (int i = 0; i < dataset_size; ++i) {
			sink ^= add_ip_n_imp_case<Cfg>(a[i].data(), b[i].data(), limbs);
		}
	}
	auto end = std::chrono::high_resolution_clock::now();
	sink ^= a[0][limbs - 1];

	const double total_calls = (double)dataset_size * iterations;
	const double ns_per_call = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
	const double ns_per_limb = ns_per_call / (double)limbs;

	std::cout << std::left << std::setw(14) << label
		<< std::setw(10) << (sizeof(uw) * 8)
		<< std::setw(10) << (Cfg::use_hw ? "hw" : "soft")
		<< std::setw(16) << std::fixed << std::setprecision(3) << ns_per_call
		<< std::setw(16) << ns_per_limb
		<< "\n";
	return sink;
}

int main() {
#if !defined(__x86_64__) && !defined(__i386__) && !defined(_M_X64) && !defined(_M_IX86)
	std::cerr << "This benchmark needs x86/x64 for _addcarry intrinsics.\n";
	return 1;
#endif

	const size_t bit_size = 4096;
	const int dataset_size = 1024;
	const int iterations = 200000;
	const size_t limbs32 = bit_size / 32;
	const size_t limbs64 = bit_size / 64;

	std::cout << "I_ADDCARRY CASES BENCHMARK\n";
	std::cout << "bit_size=" << bit_size
		<< " dataset=" << dataset_size
		<< " iterations=" << iterations
		<< "\n";
	std::cout << std::left << std::setw(14) << "case"
		<< std::setw(10) << "uw_bits"
		<< std::setw(10) << "path"
		<< std::setw(16) << "ns/call"
		<< std::setw(16) << "ns/limb"
		<< "\n";

	run_bench<case_config<uint32_t, true>>("u32_hw", limbs32, dataset_size, iterations, 123456789ULL);
	run_bench<case_config<uint32_t, false>>("u32_soft", limbs32, dataset_size, iterations, 123456789ULL);
	run_bench<case_config<uint64_t, true>>("u64_hw", limbs64, dataset_size, iterations, 123456789ULL);
	run_bench<case_config<uint64_t, false>>("u64_soft", limbs64, dataset_size, iterations, 123456789ULL);
	return 0;
}
