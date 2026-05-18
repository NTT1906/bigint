#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstring>

#define BI_BIT 512
#include "../bigint.h"

volatile u32 global_sink = 0;

int main() {
    std::cout << "POW_MOD BENCHMARK REPORT (BI_BIT = " << BI_BIT << ")\n";

    // const int DATASET_SIZE = 50;
    const int DATASET_SIZE = 10;
    // const int DATASET_SIZE = 20;
    const int BENCH_ITERATIONS = 20;
    // const int BENCH_ITERATIONS = 100;

    std::vector<bui> x_vec(DATASET_SIZE);
    std::vector<bui> e_vec(DATASET_SIZE);
    std::vector<bui> m_vec(DATASET_SIZE);
    std::vector<bui> r_mr(DATASET_SIZE);
    std::vector<bui> r_cios2(DATASET_SIZE);
    std::vector<bui> r_sos(DATASET_SIZE);
    std::vector<bui> r_cios3(DATASET_SIZE);

    std::mt19937 gen(123456);
    std::uniform_int_distribution<u32> dist(0, 0xFFFFFFFF);

    std::cout << "[+] Generating " << DATASET_SIZE << " test objects...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        for (auto& limb : m_vec[i]) limb = dist(gen);
        m_vec[i][BI_N - 1] |= 1;        // ensure odd
        m_vec[i][0] |= 1u << 31;        // ensure full bit width
        if (cmp(m_vec[i], bui1()) <= 0) m_vec[i] = bui_from_u32(3);

        for (auto& limb : x_vec[i]) limb = dist(gen);
        x_vec[i] = mod(x_vec[i], m_vec[i]);  // x < m

        for (auto& limb : e_vec[i]) limb = dist(gen);
        e_vec[i][0] |= 1u << 31;        // ensure full bit width
    }

    // ========================================================================
    // Benchmarking
    // ========================================================================
    double total_calls = (double)DATASET_SIZE * BENCH_ITERATIONS;
    auto start = std::chrono::high_resolution_clock::now();
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "[+] Benching " << BENCH_ITERATIONS << "x" << DATASET_SIZE << " times...\n";
    std::cout << "--- Benchmarking mr_pow_mod\n";
    start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < BENCH_ITERATIONS; ++iter)
        for (int i = 0; i < DATASET_SIZE; ++i)
            r_mr[i] = mr_pow_mod(x_vec[i], e_vec[i], m_vec[i]);
    end = std::chrono::high_resolution_clock::now();
    double mr_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
    global_sink ^= r_mr[0][BI_N - 1];

    std::cout << "--- Benchmarking pow_mod_mont_sos\n";
    start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < BENCH_ITERATIONS; ++iter)
        for (int i = 0; i < DATASET_SIZE; ++i)
            r_sos[i] = pow_mod_mont_sos(x_vec[i], e_vec[i], m_vec[i]);
    end = std::chrono::high_resolution_clock::now();
    double sos_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
    global_sink ^= r_sos[0][BI_N - 1];

    std::cout << "--- Benchmarking pow_mod_mont_cios2\n";
    start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < BENCH_ITERATIONS; ++iter)
        for (int i = 0; i < DATASET_SIZE; ++i)
            r_cios2[i] = pow_mod_mont_cios2(x_vec[i], e_vec[i], m_vec[i]);
    end = std::chrono::high_resolution_clock::now();
    double cios2_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
	global_sink ^= r_cios2[0][BI_N - 1];

	std::cout << "--- Benchmarking pow_mod_mont_cios3\n";
	start = std::chrono::high_resolution_clock::now();
	for (int iter = 0; iter < BENCH_ITERATIONS; ++iter)
		for (int i = 0; i < DATASET_SIZE; ++i)
			r_cios3[i] = pow_mod_mont_cios3(x_vec[i], e_vec[i], m_vec[i]);
	end = std::chrono::high_resolution_clock::now();
	double cios3_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
	global_sink ^= r_cios3[0][BI_N - 1];

    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Function"
              << std::setw(18) << "Time (ns)"
	          << "\n";
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(3);

    std::cout << std::left << std::setw(20) << "mr_pow_mod"
              << std::setw(18) << mr_ns << "\n";

    std::cout << std::left << std::setw(20) << "pow_mod_mont_sos"
              << std::setw(18) << sos_ns << "\n";

    std::cout << std::left << std::setw(20) << "pow_mod_mont_cios2"
			  << std::setw(18) << cios2_ns << "\n";

	std::cout << std::left << std::setw(20) << "pow_mod_mont_cios3"
			  << std::setw(18) << cios3_ns << "\n";

    std::cout << "--------------------------------------------------------------------------\n";
    return 0;
}

/*

POW_MOD BENCHMARK REPORT (BI_BIT = 1024)
[+] Generating 20 test objects...
[+] Benching 100x20 times...
--------------------------------------------------------------------------
Function            Time (ns)
--------------------------------------------------------------------------

+ nmod_native:
--------------------------------------------------------------------------
mr_pow_mod          3851417.750
pow_mod_mont_sos    3642741.700
pow_mod_mont_cios2  2625606.850
--------------------------------------------------------------------------

+ mod:
mr_pow_mod          3691291.550
pow_mod_mont_sos    3573980.850
pow_mod_mont_cios2  2209555.300
--------------------------------------------------------------------------

+ all mod support:
--------------------------------------------------------------------------
mr_pow_mod          3153167.000
pow_mod_mont_sos    3292128.150
pow_mod_mont_cios2  2198425.750
--------------------------------------------------------------------------
*/

// =============================== NEW CIOS 3
/*
POW_MOD BENCHMARK REPORT (BI_BIT = 1024)
[+] Generating 20 test objects...
[+] Benching 100x20 times...
--- Benchmarking mr_pow_mod
--- Benchmarking pow_mod_mont_sos
--- Benchmarking pow_mod_mont_cios2
--- Benchmarking pow_mod_mont_cios3
--------------------------------------------------------------------------
Function            Time (ns)
--------------------------------------------------------------------------
mr_pow_mod          3385318.200
pow_mod_mont_sos    3324542.900
pow_mod_mont_cios2  2225286.250
pow_mod_mont_cios3  1404414.600
--------------------------------------------------------------------------
POW_MOD BENCHMARK REPORT (BI_BIT = 4096)
[+] Generating 20 test objects...
[+] Benching 20x20 times...
--- Benchmarking mr_pow_mod
--- Benchmarking pow_mod_mont_sos
--- Benchmarking pow_mod_mont_cios2
--- Benchmarking pow_mod_mont_cios3
--------------------------------------------------------------------------
Function            Time (ns)
--------------------------------------------------------------------------
mr_pow_mod          184768042.000
pow_mod_mont_sos    182309537.000
pow_mod_mont_cios2  136698312.000
pow_mod_mont_cios3  85938319.750
--------------------------------------------------------------------------
POW_MOD BENCHMARK REPORT (BI_BIT = 8192)
[+] Generating 10 test objects...
[+] Benching 20x10 times...
--- Benchmarking mr_pow_mod
--- Benchmarking pow_mod_mont_sos
--- Benchmarking pow_mod_mont_cios2
--- Benchmarking pow_mod_mont_cios3
--------------------------------------------------------------------------
Function            Time (ns)
--------------------------------------------------------------------------
mr_pow_mod          1391940103.000
pow_mod_mont_sos    1494614502.500
pow_mod_mont_cios2  1048688911.500
pow_mod_mont_cios3  680231801.500
--------------------------------------------------------------------------
POW_MOD BENCHMARK REPORT (BI_BIT = 512)
[+] Generating 10 test objects...
[+] Benching 20x10 times...
--- Benchmarking mr_pow_mod
--- Benchmarking pow_mod_mont_sos
--- Benchmarking pow_mod_mont_cios2
--- Benchmarking pow_mod_mont_cios3
--------------------------------------------------------------------------
Function            Time (ns)
--------------------------------------------------------------------------
mr_pow_mod          318583.000
pow_mod_mont_sos    239985.500
pow_mod_mont_cios2  217338.000
pow_mod_mont_cios3  187509.000
--------------------------------------------------------------------------

*/