#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <cassert>

#ifndef BI_BIT
#define BI_BLEN 1024
#endif
#include "../bigint.h"

volatile uw global_sink = 0;

BI_ALWAYS_INLINE uw dbl_ip_n_shift_be(uw* x, uw n) {
	assert(n != 0 && "Cannot double zero-limb.");
	uw c = x[0] >> 31;
	for (uw i = 0; i < n - 1; ++i)
		x[i] = (x[i] << 1) | (x[i + 1] >> 31);
	x[n - 1] <<= 1;
	return c;
}

BI_ALWAYS_INLINE uw dbl_ip_n_addcarry(uw* x, uw n) {
	assert(n != 0 && "Cannot double zero-limb.");
#if BI_USE_HW_INTRINSICS
	unsigned char c = 0;
	while (n-- > 0)
		c = _addcarry_u32(c, x[n], x[n], &x[n]);
	return c;
#else
	return dbl_ip_n_shift_be(x, n);
#endif
}

BI_ALWAYS_INLINE uw dbl_ip_n_scalar_u64(uw* x, uw n) {
	assert(n != 0 && "Cannot double zero-limb.");
	uw c = 0;
	while (n-- > 0) {
		udw s = (udw)x[n] * 2u + c;
		x[n] = (uw)s;
		c = (uw)(s >> 32);
	}
	return c;
}

template <typename Fn>
double bench_dbl(const char* name, Fn fn, std::vector<bui> work, int iterations) {
	std::cout << "--- Benchmarking " << name << "\n";
	const double total_calls = (double)work.size() * iterations;
	uw carry_sink = 0;

	auto start = std::chrono::high_resolution_clock::now();
	for (int iter = 0; iter < iterations; ++iter) {
		for (size_t i = 0; i < work.size(); ++i) {
			carry_sink ^= fn(work[i].data(), BI_LEN);
		}
	}
	auto end = std::chrono::high_resolution_clock::now();

	global_sink ^= carry_sink;
	global_sink ^= work[0][BI_LEN - 1];
	return std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
}

int main() {
	std::cout << "DBL_IP_N BENCHMARK REPORT (BI_BIT = " << BI_BLEN << ")\n";

	const int DATASET_SIZE = 1024;
	const int BENCH_ITERATIONS = 200000;

	std::vector<bui> x_vec(DATASET_SIZE);

	std::mt19937 gen(123456);
	std::uniform_int_distribution<uw> dist(0, 0xffffffffu);

	std::cout << "[+] Generating " << DATASET_SIZE << " test objects...\n";
	for (int i = 0; i < DATASET_SIZE; ++i)
		for (auto& limb : x_vec[i]) limb = dist(gen);

	{
		bui x1 = x_vec[0], x2 = x_vec[0], x3 = x_vec[0];
		uw c1 = dbl_ip_n_shift_be(x1.data(), BI_LEN);
		uw c2 = dbl_ip_n_addcarry(x2.data(), BI_LEN);
		uw c3 = dbl_ip_n_scalar_u64(x3.data(), BI_LEN);
		if (c1 != c2 || c1 != c3 || cmp(x1, x2) != 0 || cmp(x1, x3) != 0) {
			std::cerr << "Correctness check failed\n";
			return 1;
		}
	}

	std::cout << "[+] Benching " << BENCH_ITERATIONS << "x" << DATASET_SIZE << " times...\n";
	double shift_ns = bench_dbl("dbl_shift_be", dbl_ip_n_shift_be, x_vec, BENCH_ITERATIONS);
	double intrin_ns = bench_dbl("dbl_addcarry", dbl_ip_n_addcarry, x_vec, BENCH_ITERATIONS);
	double scalar_ns = bench_dbl("dbl_scalar_u64", dbl_ip_n_scalar_u64, x_vec, BENCH_ITERATIONS);

	std::cout << "--------------------------------------------------------------------------\n";
	std::cout << std::left << std::setw(20) << "Function"
	          << std::setw(18) << "Time (ns)"
	          << "\n";
	std::cout << "--------------------------------------------------------------------------\n";
	std::cout << std::fixed << std::setprecision(3);
	std::cout << std::left << std::setw(20) << "dbl_shift_be" << std::setw(18) << shift_ns << "\n";
	std::cout << std::left << std::setw(20) << "dbl_addcarry" << std::setw(18) << intrin_ns << "\n";
	std::cout << std::left << std::setw(20) << "dbl_scalar_u64" << std::setw(18) << scalar_ns << "\n";
	std::cout << "--------------------------------------------------------------------------\n";
	return 0;
}
