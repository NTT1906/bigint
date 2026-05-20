#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

#ifndef BI_BIT
#define BI_BIT (4096 * 2)
#endif
#include "../bigint.h"

volatile u32 global_sink = 0;

static void print_fail(const char* name, const bui& a, const bui& b, const bui& q, const bui& r) {
	std::cerr << "\n[!] TEST FAILED in " << name << "\n";
	std::cerr << "a = " << bui_to_hex(a) << "\n";
	std::cerr << "b = " << bui_to_hex(b) << "\n";
	std::cerr << "q = " << bui_to_hex(q) << "\n";
	std::cerr << "r = " << bui_to_hex(r) << "\n";
	std::exit(1);
}

template <typename Fn>
double bench_div(const char* name, Fn fn, const std::vector<bui>& a_vec, const std::vector<bui>& b_vec, std::vector<bui>& q_vec, std::vector<bui>& r_vec, int iterations) {
	std::cout << "--- Benchmarking " << name << "\n";
	const double total_calls = (double)a_vec.size() * iterations;

	auto start = std::chrono::high_resolution_clock::now();
	for (int iter = 0; iter < iterations; ++iter) {
		for (size_t i = 0; i < a_vec.size(); ++i)
			fn(a_vec[i], b_vec[i], q_vec[i], r_vec[i]);
	}
	auto end = std::chrono::high_resolution_clock::now();

	global_sink ^= q_vec[0][BI_LEN - 1];
	global_sink ^= r_vec[0][BI_LEN - 1];
	return std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
}

int main() {
	std::cout << "DIVMOD_KNUTH2 BENCHMARK REPORT (BI_BIT = " << BI_BIT << ")\n";

	const int DATASET_SIZE = 10000;
	const int BENCH_ITERATIONS = 1000;

	std::vector<bui> a_vec(DATASET_SIZE);
	std::vector<bui> b_vec(DATASET_SIZE);
	std::vector<bui> q1(DATASET_SIZE), r1(DATASET_SIZE);
	std::vector<bui> q2(DATASET_SIZE), r2(DATASET_SIZE);

	std::mt19937 gen(123456);
	std::uniform_int_distribution<u32> dist(0, 0xffffffffu);

	std::cout << "[+] Generating " << DATASET_SIZE << " test objects...\n";
	for (int i = 0; i < DATASET_SIZE; ++i) {
		for (auto& limb : a_vec[i]) limb = dist(gen);
		for (auto& limb : b_vec[i]) limb = dist(gen);

		if (i % 10 == 0) b_vec[i] = bui_from_u32(1);
		else if (i % 11 == 0) b_vec[i] = bui_from_u32(0xffffffffu);
		else if (i % 12 == 0) b_vec[i] = a_vec[i];
		else if (i % 13 == 0) {
			b_vec[i] = a_vec[i];
			for (auto& limb : a_vec[i]) limb = dist(gen);
		}
		else if (i % 14 == 0) a_vec[i] = {};
		else if (i % 17 == 0) b_vec[i][0] = 0;
		else if (i % 18 == 0) {
			// force b to half size
			for (u32 j = 0; j < BI_LEN / 2; ++j)
				b_vec[i][j] = 0;
			b_vec[i][BI_LEN / 2] |= 1u << 31;
			b_vec[i][BI_LEN - 1] |= 1;
		}
		if (bui_is0(b_vec[i]))
			b_vec[i] = bui_from_u32(1);
	}

	std::cout << "[+] Validating " << DATASET_SIZE << " cases...\n";
	for (int i = 0; i < DATASET_SIZE; ++i) {
		divmod_knuth(a_vec[i], b_vec[i], q1[i], r1[i]);
		divmod_knuth2(a_vec[i], b_vec[i], q2[i], r2[i]);

		if (cmp(q1[i], q2[i]) != 0 || cmp(r1[i], r2[i]) != 0)
			print_fail("divmod_knuth2 mismatch", a_vec[i], b_vec[i], q2[i], r2[i]);

		bui qb = mul_low(q2[i], b_vec[i]);
		bui check = add(qb, r2[i]);
		if (cmp(check, a_vec[i]) != 0 || cmp(r2[i], b_vec[i]) >= 0)
			print_fail("divmod_knuth2 identity", a_vec[i], b_vec[i], q2[i], r2[i]);
	}

	std::cout << "[+] Benching " << BENCH_ITERATIONS << "x" << DATASET_SIZE << " times...\n";
	double knuth_ns = bench_div("divmod_knuth", divmod_knuth, a_vec, b_vec, q1, r1, BENCH_ITERATIONS);
	double knuth2_ns = bench_div("divmod_knuth2", divmod_knuth2, a_vec, b_vec, q2, r2, BENCH_ITERATIONS);

	std::cout << "--------------------------------------------------------------------------\n";
	std::cout << std::left << std::setw(20) << "Function"
	          << std::setw(18) << "Time (ns)"
	          << "\n";
	std::cout << "--------------------------------------------------------------------------\n";
	std::cout << std::fixed << std::setprecision(3);
	std::cout << std::left << std::setw(20) << "divmod_knuth" << std::setw(18) << knuth_ns << "\n";
	std::cout << std::left << std::setw(20) << "divmod_knuth2" << std::setw(18) << knuth2_ns << "\n";
	std::cout << "--------------------------------------------------------------------------\n";
	return 0;
}
