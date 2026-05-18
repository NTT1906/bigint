#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstring>

// Ensure this matches your local setup
#define BI_BIT 4096
// #define BI_BIT 256
#include "../bigint.h"
volatile uw global_sink = 0;

BI_ALWAYS_INLINE void mul_imp2(const uw* a, const uw* b, uw* r, const uw n) {
	std::fill_n(r, 2 * n, 0);
	for (uw i = 0; i < n; ++i) {
		if (a[n - 1 - i] == 0) continue;
		udw c = 0;
		uw k = 2 * n - 1 - i;
		BI_UNROLL(BI_UNROLL_THRESHOLD)
		for (uw j = 0; j < n; ++j) {
			udw p = (udw)a[n - 1 - i] * b[n - 1 - j] + r[k] + c;
			r[k--] = (uw)p;
			c = p >> BI_SBU32;
		}
		r[k] = c;
	}
}

int main() {
    std::cout << "========================================================\n";
    std::cout << "        MUL_IMP BENCHMARK REPORT (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    const int DATASET_SIZE = 5000;
    const int BENCH_ITERATIONS = 500; // Tune this depending on BI_BIT size to save time

    std::vector<bui> a_vec(DATASET_SIZE, bui{});
    std::vector<bui> b_vec(DATASET_SIZE, bui{});

    // We will dump output into these to simulate real workloads
    std::vector<bul> r_vec(DATASET_SIZE, bul{});

    std::mt19937 gen(123456);
    std::uniform_int_distribution<uw> val_dist(1, 0xFFFFFFFF);

    std::cout << "[+] Generating " << DATASET_SIZE << " edge-case test objects...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        int edge = gen() % 100;

        if (edge < 10) {
            // Edge Case 1: Pure Zero (Keep both as 0)
        }
        else if (edge < 30) {
            // Edge Case 2: Small numbers (Data only at the LSW end)
            a_vec[i][BI_N - 1] = val_dist(gen);
            b_vec[i][BI_N - 1] = val_dist(gen);
            if (gen() % 2) a_vec[i][BI_N - 2] = val_dist(gen);
            if (gen() % 2) b_vec[i][BI_N - 2] = val_dist(gen);
        } else if (edge < 50) {
            // Edge Case 3: Massive numbers (Data at the MSW end)
            a_vec[i][0] = val_dist(gen);
            b_vec[i][0] = val_dist(gen);
            a_vec[i][BI_N / 2] = val_dist(gen); // Throw something in the middle
        } else {
            // Edge Case 4: Random lengths
            uw active_limbs_a = (gen() % BI_N) + 1;
            uw active_limbs_b = (gen() % BI_N) + 1;

            for(uw k = 0; k < active_limbs_a; ++k)
                a_vec[i][BI_N - 1 - k] = val_dist(gen);
            for(uw k = 0; k < active_limbs_b; ++k)
                b_vec[i][BI_N - 1 - k] = val_dist(gen);
        }
    }

    // ========================================================================
    // Validation
    // ========================================================================
    std::cout << "[+] Validating correctness...\n";
    std::vector<uw> r_ref(BI_N * 2, 0);
    std::vector<uw> r_fast(BI_N * 2, 0);
    std::vector<uw> r_2(BI_N * 2, 0);

    for (int i = 0; i < DATASET_SIZE; ++i) {
        mul_imp(a_vec[i].data(), b_vec[i].data(), r_ref.data(), BI_N);
        mul_imp_fast(a_vec[i].data(), b_vec[i].data(), r_fast.data(), BI_N);
        mul_imp2(a_vec[i].data(), b_vec[i].data(), r_2.data(), BI_N);

        bool fast_fail = memcmp(r_ref.data(), r_fast.data(), BI_N * 2 * sizeof(uw)) != 0;
        bool imp2_fail = memcmp(r_ref.data(), r_2.data(), BI_N * 2 * sizeof(uw)) != 0;

        if (fast_fail || imp2_fail) {
            std::cerr << "\n[!] VALIDATION FAILED! PROGRAM HALTED.\n";
            std::cerr << "Index     : " << i << "\n";
            if (fast_fail) std::cerr << "Fast Failed\n";
            else std::cerr << "Imp2 Failed\n";
            std::cerr << "\nA           : ";
            for(int j=0; j < BI_N*2; ++j) {
                std::cerr << std::hex << std::setw(8) << std::setfill('0') << (j < BI_N ? 0 : a_vec[i].data()[j - BI_N]) << " ";
            }
            std::cerr << "\nB           : ";
            for(int j=0; j < BI_N*2; ++j) {
                std::cerr << std::hex << std::setw(8) << std::setfill('0') << (j < BI_N ? 0 : b_vec[i].data()[j - BI_N]) << " ";
            }
            std::cerr << "\nRef Output  : ";
            for(int j=0; j < BI_N*2; ++j) std::cerr << std::hex << std::setw(8) << std::setfill('0') << r_ref[j] << " ";
            std::cerr << "\nFast Output : ";
            for(int j=0; j < BI_N*2; ++j) std::cerr << std::hex << std::setw(8) << std::setfill('0') << r_fast[j] << " ";
            std::cerr << "\nImp2 Output : ";
            for(int j=0; j < BI_N*2; ++j) std::cerr << std::hex << std::setw(8) << std::setfill('0') << r_2[j] << " ";
            std::exit(1);
        }
    }
    std::cout << "    -> Validation PASSED. All outputs are 100% identical.\n\n";

    // ========================================================================
    // Benchmarking
    // ========================================================================
    double total_calls = (double)DATASET_SIZE * BENCH_ITERATIONS;

    std::cout << "--- Benchmarking mul_imp (Baseline) ---\n";
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (int j = 0; j < DATASET_SIZE; ++j) {
            mul_imp(a_vec[j].data(), b_vec[j].data(), r_vec[j].data(), BI_N);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    double ref_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
    global_sink ^= r_vec[0][0]; // Sink

    std::cout << "--- Benchmarking mul_imp_fast       ---\n";
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (int j = 0; j < DATASET_SIZE; ++j) {
            mul_imp_fast(a_vec[j].data(), b_vec[j].data(), r_vec[j].data(), BI_N);
        }
    }
    end = std::chrono::high_resolution_clock::now();
    double fast_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
    global_sink ^= r_vec[0][0]; // Sink

    std::cout << "--- Benchmarking mul_imp2           ---\n\n";
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (int j = 0; j < DATASET_SIZE; ++j) {
            mul_imp2(a_vec[j].data(), b_vec[j].data(), r_vec[j].data(), BI_N);
        }
    }
    end = std::chrono::high_resolution_clock::now();
    double p2_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
    global_sink ^= r_vec[0][0]; // Sink

    // ========================================================================
    // Clean Report Output
    // ========================================================================
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(18) << "Function"
              << std::setw(18) << "Time (ns)"
              << std::setw(18) << "vs Baseline"
              << "Status\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(3);

    std::cout << std::left << std::setw(18) << "mul_imp (ref)"
              << std::setw(18) << ref_ns
              << std::setw(18) << "1.000x"
              << "Baseline\n";

    std::cout << std::left << std::setw(18) << "mul_imp_fast"
              << std::setw(18) << fast_ns
              << std::setw(18) << ref_ns / fast_ns
    << (fast_ns < ref_ns ? "Faster" : "Slower") << "\n";

    std::cout << std::left << std::setw(18) << "mul_imp2"
              << std::setw(18) << p2_ns
              << std::setw(18) << ref_ns / p2_ns
              << (p2_ns < ref_ns ? "Faster" : "Slower") << "\n";

    std::cout << "--------------------------------------------------------------------------\n";

    return 0;
}

/*
========================================================
        MUL_IMP BENCHMARK REPORT (BI_BIT = 4096)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. All outputs are 100% identical.

--- Benchmarking mul_imp (Baseline) ---
--- Benchmarking mul_imp_fast       ---
--- Benchmarking mul_imp2           ---

--------------------------------------------------------------------------
Function          Time (ns)         vs Baseline       Status
--------------------------------------------------------------------------
mul_imp (ref)     2560.616          1.000x            Baseline
mul_imp_fast      1195.081          2.143             Faster
mul_imp2          2300.879          1.113             Faster
--------------------------------------------------------------------------

========================================================
        MUL_IMP BENCHMARK REPORT (BI_BIT = 256)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. All outputs are 100% identical.

--- Benchmarking mul_imp (Baseline) ---
--- Benchmarking mul_imp_fast       ---
--- Benchmarking mul_imp2           ---

--------------------------------------------------------------------------
Function          Time (ns)         vs Baseline       Status
--------------------------------------------------------------------------
mul_imp (ref)     29.328            1.000x            Baseline
mul_imp_fast      37.388            0.784             Slower
mul_imp2          34.735            0.844             Slower
--------------------------------------------------------------------------
========================================================
        MUL_IMP BENCHMARK REPORT (BI_BIT = 512)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. All outputs are 100% identical.

--- Benchmarking mul_imp (Baseline) ---
--- Benchmarking mul_imp_fast       ---
--- Benchmarking mul_imp2           ---

--------------------------------------------------------------------------
Function          Time (ns)         vs Baseline       Status
--------------------------------------------------------------------------
mul_imp (ref)     52.454            1.000x            Baseline
mul_imp_fast      65.629            0.799             Slower
mul_imp2          55.997            0.937             Slower
--------------------------------------------------------------------------

*/