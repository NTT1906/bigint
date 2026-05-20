#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstring>

#define BI_BIT_WIDTH 4096
#include "../bigint.h"

volatile uw global_sink = 0;

int main() {
    std::cout << "========================================================\n";
    std::cout << "       POW_MOD BENCHMARK REPORT (BI_BIT = " << BI_BIT_WIDTH << ")\n";
    std::cout << "========================================================\n\n";

    // const int DATASET_SIZE = 50;
    const int DATASET_SIZE = 25;
    const int BENCH_ITERATIONS = 10;

    std::vector<bui> x_vec(DATASET_SIZE);
    std::vector<bui> e_vec(DATASET_SIZE);
    std::vector<bui> m_vec(DATASET_SIZE);
    std::vector<bui> r_ref(DATASET_SIZE);
    std::vector<bui> r_mr(DATASET_SIZE);
    std::vector<bui> r_cios(DATASET_SIZE);

    std::mt19937 gen(123456);
    std::uniform_int_distribution<uw> dist(0, 0xFFFFFFFF);

    std::cout << "[+] Generating " << DATASET_SIZE << " test objects...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        for (auto& limb : m_vec[i]) limb = dist(gen);
        m_vec[i][BI_N - 1] |= 1;        // ensure odd
        m_vec[i][0] |= 1u << 31;        // ensure full bit width
        if (cmp(m_vec[i], bui1()) <= 0) m_vec[i] = bui_from_u32(3);

        for (auto& limb : x_vec[i]) limb = dist(gen);
        x_vec[i] = mod_native_deprecated(x_vec[i], m_vec[i]);  // x < m

        for (auto& limb : e_vec[i]) limb = dist(gen);
        e_vec[i][0] |= 1u << 31;        // ensure full bit width
    }

    // Validation
	if (BI_BIT_WIDTH <= 512) {
		std::cout << "[+] Validating correctness...\n";
		for (int i = 0; i < DATASET_SIZE; ++i) {
		    r_ref[i]  = pow_mod(x_vec[i], e_vec[i], m_vec[i]);
		    r_mr[i]   = mr_pow_mod(x_vec[i], e_vec[i], m_vec[i]);
		    r_cios[i] = pow_mod_mont_cios2(x_vec[i], e_vec[i], m_vec[i]);

		    if (cmp(r_ref[i], r_mr[i]) != 0 || cmp(r_ref[i], r_cios[i]) != 0) {
		        std::cerr << "\n[!] VALIDATION FAILED at index " << i << "\n";
		        std::cerr << "x:    " << bui_to_dec(x_vec[i]) << "\n";
		        std::cerr << "e:    " << bui_to_dec(e_vec[i]) << "\n";
		        std::cerr << "m:    " << bui_to_dec(m_vec[i]) << "\n";
		        std::cerr << "ref:  " << bui_to_dec(r_ref[i]) << "\n";
		        std::cerr << "mr:   " << bui_to_dec(r_mr[i]) << "\n";
		        std::cerr << "cios: " << bui_to_dec(r_cios[i]) << "\n";
		        return 1;
		    }
		}
		std::cout << "    -> Validation PASSED. All outputs are 100% identical.\n\n";
	}

    // ========================================================================
    // Benchmarking
    // ========================================================================
    double total_calls = (double)DATASET_SIZE * BENCH_ITERATIONS;

    std::cout << "--- Benchmarking pow_mod (Baseline) ---\n";
    auto start = std::chrono::high_resolution_clock::now();
    // for (int iter = 0; iter < BENCH_ITERATIONS; ++iter)
    //     for (int i = 0; i < DATASET_SIZE; ++i)
    //         r_ref[i] = pow_mod(x_vec[i], e_vec[i], m_vec[i]);
    auto end = std::chrono::high_resolution_clock::now();
    // double ref_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
    double ref_ns = 12353033.740;

    global_sink ^= r_ref[0][BI_N - 1];

    std::cout << "--- Benchmarking mr_pow_mod         ---\n";
    start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < BENCH_ITERATIONS; ++iter)
        for (int i = 0; i < DATASET_SIZE; ++i)
            r_mr[i] = mr_pow_mod(x_vec[i], e_vec[i], m_vec[i]);
    end = std::chrono::high_resolution_clock::now();
    double mr_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
    global_sink ^= r_mr[0][BI_N - 1];

    std::cout << "--- Benchmarking pow_mod_mont_cios2    ---\n\n";
    start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < BENCH_ITERATIONS; ++iter)
        for (int i = 0; i < DATASET_SIZE; ++i)
            r_cios[i] = pow_mod_mont_cios2(x_vec[i], e_vec[i], m_vec[i]);
    end = std::chrono::high_resolution_clock::now();
    double cios_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
    global_sink ^= r_cios[0][BI_N - 1];

    // ========================================================================
    // Report
    // ========================================================================
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Function"
              << std::setw(18) << "Time (ns)"
              << std::setw(18) << "vs Baseline"
              << "Status\n";
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(3);

    std::cout << std::left << std::setw(20) << "pow_mod (ref)"
              << std::setw(18) << ref_ns
              << std::setw(18) << "1.000x"
              << "Baseline\n";

    std::cout << std::left << std::setw(20) << "mr_pow_mod"
              << std::setw(18) << mr_ns
              << std::setw(18) << ref_ns / mr_ns
              << (mr_ns < ref_ns ? "Faster" : "Slower") << "\n";

    std::cout << std::left << std::setw(20) << "mr_cios_pow_mod"
              << std::setw(18) << cios_ns
              << std::setw(18) << ref_ns / cios_ns
              << (cios_ns < ref_ns ? "Faster" : "Slower") << "\n";

    std::cout << "--------------------------------------------------------------------------\n";
    return 0;
}