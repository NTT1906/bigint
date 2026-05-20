#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#ifndef BI_BIT
#define BI_BLEN 4096
#endif
// #define BI_UW_FORCE_32
#define BI_FORCE_NO_USE_HW_INTRIN
#include "../bigint.h"

using uint = unsigned int;
using ull = unsigned long long;

volatile uw global_sink_i_addcarry = 0;

BI_ALWAYS_INLINE unsigned char bench_i_addcarry(unsigned char c, uw a, uw b, uw* p) {
#if BI_USE_HW_INTRINSICS
#if BI_UW_BITS == 64
	// unsigned long long r;
	// *p = __builtin_addcll(a, b,c, &r);
	// return r;
	return _addcarry_u64(c, (ull)a, (ull)b, reinterpret_cast<ull*>(p));
#else
	// unsigned int r;
	// *p = __builtin_addc(a, b,c, &r);
	// return r;
	return _addcarry_u32(c, (uint)a, (uint)b, reinterpret_cast<uint*>(p));
#endif
#else
	udw s = (udw)a + b + c;
	*p = (uw)s;
	return (unsigned char)(s >> BI_SBU32);
#endif
}

BI_ALWAYS_INLINE uw bench_add_ip_n_imp(uw* a, const uw* b, uw n) {
	unsigned char c = 0;
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	while (n-- > 0)
		c = bench_i_addcarry(c, a[n], b[n], &a[n]);
	return c;
}

BI_ALWAYS_INLINE uw ref_add_ip_n_imp(uw* a, const uw* b, uw n) {
	uw c = 0;
	while (n-- > 0) {
		udw s = (udw)a[n] + b[n] + c;
		a[n] = (uw)s;
		c = (uw)(s >> BI_SBU32);
	}
	return c;
}

template <typename Fn>
double bench_add(const char* name, Fn fn, std::vector<bui> work, const std::vector<bui>& addends, int iterations) {
	std::cout << "--- Benchmarking " << name << "\n";
	const double total_calls = (double)work.size() * iterations;
	uw carry_sink = 0;

	auto start = std::chrono::high_resolution_clock::now();
	for (int iter = 0; iter < iterations; ++iter) {
		for (size_t i = 0; i < work.size(); ++i) {
			carry_sink ^= fn(work[i].data(), addends[i].data(), BI_LEN);
		}
	}
	auto end = std::chrono::high_resolution_clock::now();

	global_sink_i_addcarry ^= carry_sink;
	global_sink_i_addcarry ^= work[0][BI_LEN - 1];
	return std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
}

int main() {
	std::cout << "I_ADDCARRY BENCHMARK REPORT\n";
	std::cout << "BI_BIT=" << BI_BLEN
		<< " BI_UW_BITS=" << BI_UW_BITS
		<< " BI_USE_HW_INTRINSICS=" << BI_USE_HW_INTRINSICS
		<< "\n";

	const int dataset_size = 1024;
	const int bench_iterations = 200000;

	std::vector<bui> a_vec(dataset_size);
	std::vector<bui> b_vec(dataset_size);

	std::mt19937_64 gen(123456789);
	std::uniform_int_distribution<uw> dist(0, BI_UW_MAX);

	for (int i = 0; i < dataset_size; ++i) {
		for (auto& limb : a_vec[i]) limb = dist(gen);
		for (auto& limb : b_vec[i]) limb = dist(gen);
	}

	{
		bui x = a_vec[0];
		bui y = a_vec[0];
		uw c1 = bench_add_ip_n_imp(x.data(), b_vec[0].data(), BI_LEN);
		uw c2 = ref_add_ip_n_imp(y.data(), b_vec[0].data(), BI_LEN);
		if (c1 != c2 || cmp(x, y) != 0) {
			std::cerr << "Correctness check failed\n";
			return 1;
		}
	}

	const double ns_per_call = bench_add("bench_add_ip_n_imp", bench_add_ip_n_imp, a_vec, b_vec, bench_iterations);
	const double ns_per_limb = ns_per_call / BI_LEN;

	std::cout << std::fixed << std::setprecision(3);
	std::cout << "ns/call=" << ns_per_call << "\n";
	std::cout << "ns/limb=" << ns_per_limb << "\n";
	return 0;
}
