#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <string>
#include <cstring>

#ifndef BI_BIT
#define BI_BLEN (8192)
#endif
#ifndef DATASET_SIZE
#define DATASET_SIZE 1000
#endif
#ifndef BENCH_ITERATIONS
#define BENCH_ITERATIONS 1000
#endif
#include "../bigint.h"

volatile uw global_sink = 0;

struct DivCase {
	const char* name;
	std::vector<bui> a;
	std::vector<bui> b;
	std::vector<bui> q1;
	std::vector<bui> r1;
	std::vector<bui> q2;
	std::vector<bui> r2;
};

static void print_fail(const char* name, const bui& a, const bui& b, const bui& q, const bui& r) {
	std::cerr << "\n[!] TEST FAILED in " << name << "\n";
	std::cerr << "a = " << bui_to_hex(a) << "\n";
	std::cerr << "b = " << bui_to_hex(b) << "\n";
	std::cerr << "q = " << bui_to_hex(q) << "\n";
	std::cerr << "r = " << bui_to_hex(r) << "\n";
	std::exit(1);
}

static bui random_bui(std::mt19937& gen) {
	std::uniform_int_distribution<uw> dist(0, 0xffffffffu);
	bui x{};
	for (auto& limb : x)
		limb = dist(gen);
	return x;
}

static void make_nonzero(bui& x) {
	if (bui_is0(x))
		x = bui_from_u32(1);
}

static void force_limb_size(bui& x, uw limbs, bool odd = false) {
	if (limbs == 0) {
		x = {};
		return;
	}
	if (limbs > BI_LEN)
		limbs = BI_LEN;
	const uw zero_limbs = BI_LEN - limbs;
	for (uw i = 0; i < zero_limbs; ++i)
		x[i] = 0;
	x[zero_limbs] |= 1u << 31;
	if (odd)
		x[BI_LEN - 1] |= 1;
}

static DivCase make_case(const char* name, int size) {
	DivCase c{name};
	c.a.resize(size);
	c.b.resize(size);
	c.q1.resize(size);
	c.r1.resize(size);
	c.q2.resize(size);
	c.r2.resize(size);
	return c;
}

static void fill_case(DivCase& c, std::mt19937& gen) {
	for (size_t i = 0; i < c.a.size(); ++i) {
		c.a[i] = random_bui(gen);
		c.b[i] = random_bui(gen);

		if (std::strcmp(c.name, "random") == 0) {
			make_nonzero(c.b[i]);
		} else if (std::strcmp(c.name, "divisor_1") == 0) {
			c.b[i] = bui_from_u32(1);
		} else if (std::strcmp(c.name, "divisor_u32max") == 0) {
			c.b[i] = bui_from_u32(0xffffffffu);
		} else if (std::strcmp(c.name, "equal") == 0) {
			c.b[i] = c.a[i];
			make_nonzero(c.b[i]);
		} else if (std::strcmp(c.name, "zero_dividend") == 0) {
			c.a[i] = {};
			make_nonzero(c.b[i]);
		} else if (std::strcmp(c.name, "a_lt_b") == 0) {
			c.b[i] = c.a[i];
			c.b[i][BI_LEN - 1] |= 1;
			if (cmp(c.a[i], c.b[i]) >= 0)
				c.a[i] = {};
			make_nonzero(c.b[i]);
		} else if (std::strcmp(c.name, "top_zero_divisor") == 0) {
			c.b[i][0] = 0;
			make_nonzero(c.b[i]);
		} else if (std::strcmp(c.name, "half_size_divisor") == 0) {
			force_limb_size(c.a[i], BI_LEN, false);
			force_limb_size(c.b[i], BI_LEN / 2, true);
		} else if (std::strcmp(c.name, "quarter_size_divisor") == 0) {
			force_limb_size(c.a[i], BI_LEN, false);
			force_limb_size(c.b[i], BI_LEN / 4, true);
		} else if (std::strcmp(c.name, "small_2limb_divisor") == 0) {
			force_limb_size(c.a[i], BI_LEN, false);
			force_limb_size(c.b[i], BI_LEN < 2 ? BI_LEN : 2, true);
		}

		make_nonzero(c.b[i]);
	}
}

static void validate_case(DivCase& c) {
	for (size_t i = 0; i < c.a.size(); ++i) {
		divmod_knuth(c.a[i], c.b[i], c.q1[i], c.r1[i]);
		divmod_knuth2(c.a[i], c.b[i], c.q2[i], c.r2[i]);

		if (cmp(c.q1[i], c.q2[i]) != 0 || cmp(c.r1[i], c.r2[i]) != 0)
			print_fail(c.name, c.a[i], c.b[i], c.q2[i], c.r2[i]);

		bui qb = mul_low(c.q2[i], c.b[i]);
		bui check = add(qb, c.r2[i]);
		if (cmp(check, c.a[i]) != 0 || cmp(c.r2[i], c.b[i]) >= 0)
			print_fail(c.name, c.a[i], c.b[i], c.q2[i], c.r2[i]);
	}
}

template <typename Fn>
double bench_div(Fn fn, const std::vector<bui>& a, const std::vector<bui>& b, std::vector<bui>& q, std::vector<bui>& r, int iterations) {
	const double total_calls = (double)a.size() * iterations;
	auto start = std::chrono::high_resolution_clock::now();
	for (int iter = 0; iter < iterations; ++iter)
		for (size_t i = 0; i < a.size(); ++i)
			fn(a[i], b[i], q[i], r[i]);
	auto end = std::chrono::high_resolution_clock::now();

	global_sink ^= q[0][BI_LEN - 1];
	global_sink ^= r[0][BI_LEN - 1];
	return std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
}

int main() {
	std::cout << "DIVMOD_KNUTH2 CATEGORY BENCHMARK REPORT (BI_BIT = " << BI_BLEN << ")\n";

	std::vector<DivCase> cases;
	cases.push_back(make_case("random", DATASET_SIZE));
	cases.push_back(make_case("divisor_1", DATASET_SIZE));
	cases.push_back(make_case("divisor_u32max", DATASET_SIZE));
	cases.push_back(make_case("equal", DATASET_SIZE));
	cases.push_back(make_case("zero_dividend", DATASET_SIZE));
	cases.push_back(make_case("a_lt_b", DATASET_SIZE));
	cases.push_back(make_case("top_zero_divisor", DATASET_SIZE));
	cases.push_back(make_case("half_size_divisor", DATASET_SIZE));
	cases.push_back(make_case("quarter_size_divisor", DATASET_SIZE));
	cases.push_back(make_case("small_2limb_divisor", DATASET_SIZE));

	std::mt19937 gen(123456);
	std::cout << "[+] Generating " << DATASET_SIZE << " test objects per category...\n";
	for (auto& c : cases)
		fill_case(c, gen);

	std::cout << "[+] Validating categories...\n";
	for (auto& c : cases)
		validate_case(c);

	std::cout << "[+] Benching " << BENCH_ITERATIONS << "x" << DATASET_SIZE << " per category...\n";

	double sum_knuth = 0.0;
	double sum_knuth2 = 0.0;

	std::cout << "--------------------------------------------------------------------------------\n";
	std::cout << std::left << std::setw(24) << "Case"
	          << std::setw(18) << "divmod_knuth"
	          << std::setw(18) << "divmod_knuth2"
	          << std::setw(12) << "Speedup"
	          << "\n";
	std::cout << "--------------------------------------------------------------------------------\n";
	std::cout << std::fixed << std::setprecision(3);

	for (auto& c : cases) {
		double knuth_ns = bench_div(divmod_knuth, c.a, c.b, c.q1, c.r1, BENCH_ITERATIONS);
		double knuth2_ns = bench_div(divmod_knuth2, c.a, c.b, c.q2, c.r2, BENCH_ITERATIONS);
		sum_knuth += knuth_ns;
		sum_knuth2 += knuth2_ns;

		std::cout << std::left << std::setw(24) << c.name
		          << std::setw(18) << knuth_ns
		          << std::setw(18) << knuth2_ns
		          << std::setw(12) << (knuth_ns / knuth2_ns)
		          << "\n";
	}

	const double avg_knuth = sum_knuth / cases.size();
	const double avg_knuth2 = sum_knuth2 / cases.size();
	std::cout << "--------------------------------------------------------------------------------\n";
	std::cout << std::left << std::setw(24) << "AVG"
	          << std::setw(18) << avg_knuth
	          << std::setw(18) << avg_knuth2
	          << std::setw(12) << (avg_knuth / avg_knuth2)
	          << "\n";
	std::cout << "--------------------------------------------------------------------------------\n";
	return 0;
}
