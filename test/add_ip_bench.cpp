#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

#ifndef BI_BIT
#define BI_BIT 4096
#endif
#include "../bigint.h"

volatile u32 global_sink = 0;

BI_ALWAYS_INLINE u32 add_ip_n_scalar_u64(u32* a, const u32* b, u32 n) {
	u32 c = 0;
	while (n-- > 0) {
		u64 s = (u64)a[n] + b[n] + c;
		a[n] = (u32)s;
		c = (u32)(s >> BI_SBU32);
	}
	return c;
}

BI_ALWAYS_INLINE u32 add_ip_n_hw_intrin(u32* a, const u32* b, u32 n) {
#if BI_USE_HW_INTRIN
	unsigned char c = 0;
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	while (n-- > 0)
		c = _addcarry_u32(c, a[n], b[n], &a[n]);
	return c;
#else
	return add_ip_n_scalar_u64(a, b, n);
#endif
}

BI_ALWAYS_INLINE u32 add_ip_n_u64_pair(u32* a, const u32* b, u32 n) {
	u32 c = 0;
	while (n >= 2) {
		n -= 2;
		u64 av = ((u64)a[n] << 32) | a[n + 1];
		u64 bv = ((u64)b[n] << 32) | b[n + 1];

		u64 s = av + bv;
		u64 r = s + c;
		c = (u32)((s < av) | (r < s));

		a[n]     = (u32)(r >> 32);
		a[n + 1] = (u32)r;
	}

	if (n) {
		u64 s = (u64)a[0] + b[0] + c;
		a[0] = (u32)s;
		c = (u32)(s >> BI_SBU32);
	}

	return c;
}

template <typename Fn>
double bench_add(const char* name, Fn fn, std::vector<bui> work, const std::vector<bui>& addends, int iterations) {
	std::cout << "--- Benchmarking " << name << "\n";
	const double total_calls = (double)work.size() * iterations;
	u32 carry_sink = 0;

	auto start = std::chrono::high_resolution_clock::now();
	for (int iter = 0; iter < iterations; ++iter) {
		for (size_t i = 0; i < work.size(); ++i) {
			carry_sink ^= fn(work[i].data(), addends[i].data(), BI_N);
		}
	}
	auto end = std::chrono::high_resolution_clock::now();

	global_sink ^= carry_sink;
	global_sink ^= work[0][BI_N - 1];
	return std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
}

int main() {
	std::cout << "ADD_IP_N BENCHMARK REPORT (BI_BIT = " << BI_BIT << ")\n";

	const int DATASET_SIZE = 1024;
	const int BENCH_ITERATIONS = 200000;

	std::vector<bui> a_vec(DATASET_SIZE);
	std::vector<bui> b_vec(DATASET_SIZE);

	std::mt19937 gen(123456);
	std::uniform_int_distribution<u32> dist(0, 0xffffffffu);

	std::cout << "[+] Generating " << DATASET_SIZE << " test objects...\n";
	for (int i = 0; i < DATASET_SIZE; ++i) {
		for (auto& limb : a_vec[i]) limb = dist(gen);
		for (auto& limb : b_vec[i]) limb = dist(gen);
	}

	{
		bui a1 = a_vec[0], a2 = a_vec[0], a3 = a_vec[0];
		u32 c1 = add_ip_n_scalar_u64(a1.data(), b_vec[0].data(), BI_N);
		u32 c2 = add_ip_n_hw_intrin(a2.data(), b_vec[0].data(), BI_N);
		u32 c3 = add_ip_n_u64_pair(a3.data(), b_vec[0].data(), BI_N);
		if (c1 != c2 || c1 != c3 || cmp(a1, a2) != 0 || cmp(a1, a3) != 0) {
			std::cerr << "Correctness check failed\n";
			return 1;
		}
	}

	std::cout << "[+] Benching " << BENCH_ITERATIONS << "x" << DATASET_SIZE << " times...\n";
	double scalar_ns = bench_add("add_scalar_u64", add_ip_n_scalar_u64, a_vec, b_vec, BENCH_ITERATIONS);
	double intrin_ns = bench_add("add_hw_intrin", add_ip_n_hw_intrin, a_vec, b_vec, BENCH_ITERATIONS);
	double pair_ns = bench_add("add_u64_pair", add_ip_n_u64_pair, a_vec, b_vec, BENCH_ITERATIONS);

	std::cout << "--------------------------------------------------------------------------\n";
	std::cout << std::left << std::setw(20) << "Function"
	          << std::setw(18) << "Time (ns)"
	          << "\n";
	std::cout << "--------------------------------------------------------------------------\n";
	std::cout << std::fixed << std::setprecision(3);
	std::cout << std::left << std::setw(20) << "add_scalar_u64" << std::setw(18) << scalar_ns << "\n";
	std::cout << std::left << std::setw(20) << "add_hw_intrin" << std::setw(18) << intrin_ns << "\n";
	std::cout << std::left << std::setw(20) << "add_u64_pair" << std::setw(18) << pair_ns << "\n";
	std::cout << "--------------------------------------------------------------------------\n";
	return 0;
}
