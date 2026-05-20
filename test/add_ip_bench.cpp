#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

#ifndef BI_BIT
#define BI_BIT_WIDTH 4096
#endif
#include "../bigint.h"

volatile uw global_sink = 0;

BI_ALWAYS_INLINE uw add_ip_n_scalar_u64(uw* a, const uw* b, uw n) {
	uw c = 0;
	while (n-- > 0) {
		udw s = (udw)a[n] + b[n] + c;
		a[n] = (uw)s;
		c = (uw)(s >> BI_SBU32);
	}
	return c;
}

BI_ALWAYS_INLINE uw add_ip_n_hw_intrin(uw* a, const uw* b, uw n) {
#if BI_USE_HW_INTRINSICS
	unsigned char c = 0;
	BI_UNROLL(BI_UNROLL_THRESHOLD)
	while (n-- > 0)
		c = _addcarry_u32(c, a[n], b[n], &a[n]);
	return c;
#else
	return add_ip_n_scalar_u64(a, b, n);
#endif
}

BI_ALWAYS_INLINE uw add_ip_n_u64_pair(uw* a, const uw* b, uw n) {
	uw c = 0;
	while (n >= 2) {
		n -= 2;
		udw av = ((udw)a[n] << 32) | a[n + 1];
		udw bv = ((udw)b[n] << 32) | b[n + 1];

		udw s = av + bv;
		udw r = s + c;
		c = (uw)((s < av) | (r < s));

		a[n]     = (uw)(r >> 32);
		a[n + 1] = (uw)r;
	}

	if (n) {
		udw s = (udw)a[0] + b[0] + c;
		a[0] = (uw)s;
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

	global_sink ^= carry_sink;
	global_sink ^= work[0][BI_LEN - 1];
	return std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
}

int main() {
	std::cout << "ADD_IP_N BENCHMARK REPORT (BI_BIT = " << BI_BIT_WIDTH << ")\n";

	const int DATASET_SIZE = 1024;
	const int BENCH_ITERATIONS = 200000;

	std::vector<bui> a_vec(DATASET_SIZE);
	std::vector<bui> b_vec(DATASET_SIZE);

	std::mt19937 gen(123456);
	std::uniform_int_distribution<uw> dist(0, 0xffffffffu);

	std::cout << "[+] Generating " << DATASET_SIZE << " test objects...\n";
	for (int i = 0; i < DATASET_SIZE; ++i) {
		for (auto& limb : a_vec[i]) limb = dist(gen);
		for (auto& limb : b_vec[i]) limb = dist(gen);
	}

	{
		bui a1 = a_vec[0], a2 = a_vec[0], a3 = a_vec[0];
		uw c1 = add_ip_n_scalar_u64(a1.data(), b_vec[0].data(), BI_LEN);
		uw c2 = add_ip_n_hw_intrin(a2.data(), b_vec[0].data(), BI_LEN);
		uw c3 = add_ip_n_u64_pair(a3.data(), b_vec[0].data(), BI_LEN);
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
