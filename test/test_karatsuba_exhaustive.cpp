#include <iostream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <random>
#include <algorithm>
#include <iomanip>

#define BI_BIT 512
#include "../bigint.h"

static int passed = 0;
static int failed = 0;

static const u32 interesting[] = {
	0x00000000, 0x00000001, 0x00000002, 0x00000003,
	0x000000FF, 0x00000100, 0x7FFFFFFF, 0x80000000,
	0x80000001, 0xFFFFFFFE, 0xFFFFFFFF,
	0x00010000, 0x01000000, 0x10000000,
	0xAAAAAAAA, 0x55555555, 0x12345678, 0x87654321,
};
static constexpr int N_INTERESTING = sizeof(interesting) / sizeof(interesting[0]);

static std::mt19937 rng(42);
static u32 rand_u32() {
	return (u32)rng();
}

static void fill_rand(u32* x, u32 n) {
	for (u32 i = 0; i < n; ++i) x[i] = rand_u32();
}

static void fill_edge(u32* x, u32 n, int edge_type) {
	switch (edge_type % 6) {
	case 0: std::fill_n(x, n, 0u); break;
	case 1: std::fill_n(x, n, 0xFFFFFFFFu); break;
	case 2: x[0] = 1; std::fill_n(x + 1, n - 1, 0u); break;
	case 3: x[n - 1] = 0x80000000u; break;
	case 4: x[0] = 0x80000000u; break;
	case 5: x[n / 2] = 0xFFFFFFFFu; break;
	}
}

static bool test_karatsuba_imp_n(const u32* a, const u32* b, u32 n) {
	std::vector<u32> r_kara(2 * n, 0xDEADBEEF);
	std::vector<u32> r_mul(2 * n, 0xDEADBEEF);
	u32 scratch_limbs = 6 * n + 16;
	std::vector<u32> scratch(scratch_limbs, 0);

	karatsuba_imp(a, b, r_kara.data(), n, scratch.data());
	mul_imp(a, b, r_mul.data(), n);

	return memcmp(r_kara.data(), r_mul.data(), 2 * n * sizeof(u32)) == 0;
}

static bool test_karatsuba_pub(const bui& a, const bui& b) {
	bul r1 = karatsuba(a, b);
	bul r2 = mul(a, b);
	return cmp(r1, r2) == 0;
}

static void run_exhaustive_small() {
	std::cout << "--- Exhaustive tests for small n (interesting values) ---\n";

	for (u32 n = 1; n <= 2; ++n) {
		std::vector<u32> a(n), b(n);

		auto try_all = [&](int& iter) {
			for (int ai = 0; ai < N_INTERESTING; ++ai) {
				a[0] = interesting[ai];
				for (int bi = 0; bi < N_INTERESTING; ++bi) {
					b[0] = interesting[bi];
					if (n == 1) {
						if (!test_karatsuba_imp_n(a.data(), b.data(), n)) {
							std::cout << "FAIL: n=1, a=0x" << std::hex << a[0]
								<< " b=0x" << b[0] << std::dec << "\n";
							return false;
						}
						++iter;
					} else {
						for (int aj = 0; aj < N_INTERESTING; ++aj) {
							a[1] = interesting[aj];
							for (int bj = 0; bj < N_INTERESTING; ++bj) {
								b[1] = interesting[bj];
								if (!test_karatsuba_imp_n(a.data(), b.data(), n)) {
									std::cout << "FAIL: n=2, a=[0x" << std::hex << a[0]
										<< " 0x" << a[1] << "] b=[0x" << b[0]
										<< " 0x" << b[1] << "]" << std::dec << "\n";
									return false;
								}
								++iter;
							}
						}
					}
				}
			}
			return true;
		};

		int iter = 0;
		if (try_all(iter)) {
			std::cout << "  n=" << n << ": " << iter << " tests PASSED\n";
			passed += iter;
		} else {
			failed += iter;
		}
	}
}

static void run_varied_n() {
	std::cout << "--- Testing varied n (random + edge cases) ---\n";

	u32 ns[] = {1, 2, 3, 4, 5, 6, 7, 9, 10, 11, 13, 15, 16, 17, 31, 32, 33, 63, 64, 65, 127, 128, 129, BI_N};

	for (u32 n : ns) {
		if (n > BI_N) continue;

		const int TRIALS = (n <= 8) ? 50000 : (n <= 32) ? 20000 : (n <= 128) ? 5000 : 1000;

		std::vector<u32> a(n), b(n);
		int ok = 0;

		for (int t = 0; t < TRIALS; ++t) {
			if (t % 10 == 0) {
				fill_edge(a.data(), n, t);
				fill_edge(b.data(), n, t + 1);
			} else {
				fill_rand(a.data(), n);
				fill_rand(b.data(), n);
			}

			if (test_karatsuba_imp_n(a.data(), b.data(), n)) {
				++ok;
			} else {
				std::cout << "FAIL: n=" << n << " trial=" << t << "\n  a=[";
				for (u32 i = 0; i < n; ++i) std::cout << std::hex << a[i] << " ";
				std::cout << "]\n  b=[";
				for (u32 i = 0; i < n; ++i) std::cout << b[i] << " ";
				std::cout << "]" << std::dec << "\n";
				failed += 1;
				return;
			}
		}
		std::cout << "  n=" << n << " (" << std::setw(3) << (n * 32) << " bits): "
			<< ok << "/" << TRIALS << " PASSED\n";
		passed += ok;
	}
}

static void run_public_api() {
	std::cout << "--- Testing public karatsuba() API ---\n";

	const int TRIALS = 50000;
	int ok = 0;

	for (int t = 0; t < TRIALS; ++t) {
		bui a{}, b{};
		if (t % 10 == 0) {
			u32 edge_a[BI_N], edge_b[BI_N];
			fill_edge(edge_a, BI_N, t);
			fill_edge(edge_b, BI_N, t + 1);
			std::copy_n(edge_a, BI_N, a.data());
			std::copy_n(edge_b, BI_N, b.data());
		} else {
			fill_rand(a.data(), BI_N);
			fill_rand(b.data(), BI_N);
		}

		if (test_karatsuba_pub(a, b)) {
			++ok;
		} else {
			std::cout << "FAIL: public API trial=" << t << "\n";
			failed += 1;
			return;
		}
	}
	std::cout << "  karatsuba() vs mul(): " << ok << "/" << TRIALS << " PASSED\n";
	passed += ok;
}

// Test that computing with external scratch vs internal allocation matches
static void run_scratch_consistency() {
	std::cout << "--- Testing scratch buffer consistency ---\n";

	const int TRIALS = 5000;
	int ok = 0;

	for (int t = 0; t < TRIALS; ++t) {
		bui a{}, b{};
		fill_rand(a.data(), BI_N);
		fill_rand(b.data(), BI_N);

		// Use karatsuba() which does its own scratch allocation
		bul r1 = karatsuba(a, b);

		// Manually call karatsuba_imp with a fixed scratch
		bul r2{};
		u32 scratch_limbs = 6 * BI_N + 16;
		std::vector<u32> scratch(scratch_limbs, 0);
		karatsuba_imp(a.data(), b.data(), r2.data(), BI_N, scratch.data());

		if (cmp(r1, r2) == 0) {
			++ok;
		} else {
			std::cout << "FAIL: scratch consistency trial=" << t << "\n";
			failed += 1;
			return;
		}
	}
	std::cout << "  " << ok << "/" << TRIALS << " PASSED\n";
	passed += ok;
}

int main() {
	std::cout << "============================================================\n";
	std::cout << "  KARATSUBA EXHAUSTIVE CORRECTNESS TEST (BI_BIT=" << BI_BIT << ")\n";
	std::cout << "============================================================\n\n";

	run_exhaustive_small();
	run_varied_n();
	run_public_api();
	run_scratch_consistency();

	std::cout << "\n============================================================\n";
	std::cout << "  RESULTS: " << passed << " passed, " << failed << " failed\n";
	std::cout << "============================================================\n";
	return failed > 0 ? 1 : 0;
}
