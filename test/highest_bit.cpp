#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstdlib>

// Adjust BI_BIT if needed for your local setup
// #define BI_BIT 24000
#define BI_BIT 4096
#define BI_NFORCE_UNROLL
#include "../bigint.h"

// ========================================================================
// Prevent dead-code elimination
// ========================================================================
volatile u32 global_sink = 0;

// ========================================================================
// Old Implementations
// ========================================================================

inline u32 highest_bit_old_bui(const bui &x) {
BI_UNROLL(BI_UNROLL_THRESHOLD)
    for (u32 i = 0; i < BI_N; ++i)
       if (x[i] != 0) return highest_bit(x[i]) + (BI_N - i - 1) * BI_SBU32;
    return 0;
}

inline u32 highest_bit_old_bul(const bul &x) {
BI_UNROLL(BI_UNROLL_THRESHOLD)
    for (u32 i = 0; i < BI_N * 2; ++i)
       if (x[i] != 0) return highest_bit(x[i]) + (BI_N * 2 - i - 1) * BI_SBU32;
    return 0;
}

// ========================================================================
// New Implementations (Fixed indices + Compile-time routing)
// ========================================================================
inline u32 highest_bit_new_bui(const bui &x) {
    if BI_OP_CONSTEXPR (BI_N <= 16) {
        for (u32 i = 0; i < BI_N; ++i)
           if (x[i] != 0) return highest_bit(x[i]) + (BI_N - i - 1) * BI_SBU32;
        return 0;
    } else {
        u32 i = 0;
        for (; i + 3 < BI_N; i += 4) {
           if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
              if (x[i  ]) return highest_bit(x[i  ]) + (BI_N - i - 1) * BI_SBU32;
              if (x[i+1]) return highest_bit(x[i+1]) + (BI_N - i - 2) * BI_SBU32;
              if (x[i+2]) return highest_bit(x[i+2]) + (BI_N - i - 3) * BI_SBU32; // Fixed!
              return highest_bit(x[i+3]) + (BI_N - i - 4) * BI_SBU32;             // Fixed!
           }
        }
        for (; i < BI_N; ++i)
           if (x[i] != 0) return highest_bit(x[i]) + (BI_N - i - 1) * BI_SBU32;
        return 0;
    }
}

inline u32 highest_bit_new_bul(const bul &x) {
    if BI_OP_CONSTEXPR (BI_N * 2 <= 16) {
        for (u32 i = 0; i < BI_N * 2; ++i)
           if (x[i] != 0) return highest_bit(x[i]) + (BI_N * 2 - i - 1) * BI_SBU32;
        return 0;
    } else {
        u32 i = 0;
        for (; i + 3 < BI_N * 2; i += 4) {
           if (x[i] | x[i+1] | x[i+2] | x[i+3]) {
              if (x[i  ]) return highest_bit(x[i  ]) + (BI_N * 2 - i - 1) * BI_SBU32;
              if (x[i+1]) return highest_bit(x[i+1]) + (BI_N * 2 - i - 2) * BI_SBU32;
              if (x[i+2]) return highest_bit(x[i+2]) + (BI_N * 2 - i - 3) * BI_SBU32; // Fixed!
              return highest_bit(x[i+3]) + (BI_N * 2 - i - 4) * BI_SBU32;             // Fixed!
           }
        }
        for (; i < BI_N * 2; ++i)
           if (x[i] != 0) return highest_bit(x[i]) + (BI_N * 2 - i - 1) * BI_SBU32;
        return 0;
    }
}

// ========================================================================
// Benchmark Runner
// ========================================================================
int main() {
    std::cout << "========================================================\n";
    std::cout << "      HIGHEST_BIT BENCHMARK REPORT (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    const int DATASET_SIZE = 10000;
    const int BENCH_ITERATIONS = 1000; // 10k * 10k = 100 Million calls

    std::vector<bui> test_bui(DATASET_SIZE, bui{});
    std::vector<bul> test_bul(DATASET_SIZE, bul{});

    std::mt19937 gen(12345);
    std::uniform_int_distribution<u32> val_dist(1, 0xFFFFFFFF);

    std::cout << "[+] Generating " << DATASET_SIZE << " edge-case test objects...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        int edge = gen() % 100;

        if (edge < 10) {
            // Edge Case 1: Pure Zero
        }
        else if (edge < 30) {
            // Edge Case 2: Small numbers (Data only at the very end LSW)
            test_bui[i][BI_N - 1] = val_dist(gen);
            test_bul[i][BI_N * 2 - 1] = val_dist(gen);
        }
        else if (edge < 50) {
            // Edge Case 3: Massive numbers (Data at the very beginning MSW)
            test_bui[i][0] = val_dist(gen);
            test_bul[i][0] = val_dist(gen);
        }
        else {
            // Edge Case 4: Random lengths
            u32 active_limbs_bui = (gen() % BI_N) + 1;
            u32 active_limbs_bul = (gen() % (BI_N * 2)) + 1;

            test_bui[i][BI_N - active_limbs_bui] = val_dist(gen);
            test_bul[i][BI_N * 2 - active_limbs_bul] = val_dist(gen);
        }
    }

    // ========================================================================
    // Validation
    // ========================================================================
    std::cout << "[+] Validating correctness...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        u32 res_old_bui = highest_bit_old_bui(test_bui[i]);
        u32 res_new_bui = highest_bit_new_bui(test_bui[i]);

        if (res_old_bui != res_new_bui) {
            std::cerr << "\n[!] VALIDATION FAILED! PROGRAM HALTED.\n";
            std::cerr << "BUI Index : " << i << "\n";
            std::cerr << "Old Result: " << res_old_bui << "\n";
            std::cerr << "New Result: " << res_new_bui << "\n";
            std::exit(1);
        }

        u32 res_old_bul = highest_bit_old_bul(test_bul[i]);
        u32 res_new_bul = highest_bit_new_bul(test_bul[i]);

        if (res_old_bul != res_new_bul) {
            std::cerr << "\n[!] VALIDATION FAILED! PROGRAM HALTED.\n";
            std::cerr << "BUL Index : " << i << "\n";
            std::cerr << "Old Result: " << res_old_bul << "\n";
            std::cerr << "New Result: " << res_new_bul << "\n";
            std::exit(1);
        }
    }
    std::cout << "    -> Validation PASSED. Outputs are 100% identical.\n\n";

    // ========================================================================
    // Benchmarking
    // ========================================================================
    double total_calls = (double)DATASET_SIZE * BENCH_ITERATIONS;
    u32 chk_bui_old = 0, chk_bui_new = 0;
    u32 chk_bul_old = 0, chk_bul_new = 0;

    std::cout << "--- Benchmarking BUI ---\n";
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bui) chk_bui_old ^= highest_bit_old_bui(a);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double old_bui_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bui) chk_bui_new ^= highest_bit_new_bui(a);
    }
    end = std::chrono::high_resolution_clock::now();
    double new_bui_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;

    std::cout << "--- Benchmarking BUL ---\n\n";
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bul) chk_bul_old ^= highest_bit_old_bul(a);
    }
    end = std::chrono::high_resolution_clock::now();
    double old_bul_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bul) chk_bul_new ^= highest_bit_new_bul(a);
    }
    end = std::chrono::high_resolution_clock::now();
    double new_bul_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;

    // ========================================================================
    // Clean Report Output
    // ========================================================================
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(18) << "Function"
              << std::setw(18) << "Old (ns)"
              << std::setw(18) << "New (ns)"
              << "Speedup\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(3);

    std::cout << std::left << std::setw(18) << "highest_bit(bui)"
              << std::setw(18) << old_bui_ns
              << std::setw(18) << new_bui_ns
              << old_bui_ns / new_bui_ns << "x\n";

    std::cout << std::left << std::setw(18) << "highest_bit(bul)"
              << std::setw(18) << old_bul_ns
              << std::setw(18) << new_bul_ns
              << old_bul_ns / new_bul_ns << "x\n";

    std::cout << "--------------------------------------------------------------------------\n";

    global_sink = chk_bui_old ^ chk_bui_new ^ chk_bul_old ^ chk_bul_new;

    return 0;
}

/*
========================================================
      HIGHEST_BIT BENCHMARK REPORT (BI_BIT = 2048)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. Outputs are 100% identical.

--- Benchmarking BUI ---
--- Benchmarking BUL ---

--------------------------------------------------------------------------
Function          Old (ns)          New (ns)          Speedup
--------------------------------------------------------------------------
highest_bit(bui)  19.057            11.029            1.728x
highest_bit(bul)  27.022            22.508            1.201x
--------------------------------------------------------------------------
========================================================
      HIGHEST_BIT BENCHMARK REPORT (BI_BIT = 512)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. Outputs are 100% identical.

--- Benchmarking BUI ---
--- Benchmarking BUL ---

--------------------------------------------------------------------------
Function          Old (ns)          New (ns)          Speedup
--------------------------------------------------------------------------
highest_bit(bui)  4.831             4.112             1.175x
highest_bit(bul)  23.061            20.199            1.142x
--------------------------------------------------------------------------
========================================================
      HIGHEST_BIT BENCHMARK REPORT (BI_BIT = 256)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. Outputs are 100% identical.

--- Benchmarking BUI ---
--- Benchmarking BUL ---

--------------------------------------------------------------------------
Function          Old (ns)          New (ns)          Speedup
--------------------------------------------------------------------------
highest_bit(bui)  4.966             4.096             1.213x
highest_bit(bul)  3.888             3.723             1.044x
--------------------------------------------------------------------------
========================================================
      HIGHEST_BIT BENCHMARK REPORT (BI_BIT = 128)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. Outputs are 100% identical.

--- Benchmarking BUI ---
--- Benchmarking BUL ---

--------------------------------------------------------------------------
Function          Old (ns)          New (ns)          Speedup
--------------------------------------------------------------------------
highest_bit(bui)  4.506             5.067             0.889x
highest_bit(bul)  5.220             4.798             1.088x
--------------------------------------------------------------------------
========================================================
      HIGHEST_BIT BENCHMARK REPORT (BI_BIT = 192)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. Outputs are 100% identical.

--- Benchmarking BUI ---
--- Benchmarking BUL ---

--------------------------------------------------------------------------
Function          Old (ns)          New (ns)          Speedup
--------------------------------------------------------------------------
highest_bit(bui)  3.529             4.405             0.801x
highest_bit(bul)  4.134             4.069             1.016x
--------------------------------------------------------------------------
========================================================
      HIGHEST_BIT BENCHMARK REPORT (BI_BIT = 576)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. Outputs are 100% identical.

--- Benchmarking BUI ---
--- Benchmarking BUL ---

--------------------------------------------------------------------------
Function          Old (ns)          New (ns)          Speedup
--------------------------------------------------------------------------
highest_bit(bui)  10.281            6.560             1.567x
highest_bit(bul)  13.934            8.486             1.642x
--------------------------------------------------------------------------
========================================================
      HIGHEST_BIT BENCHMARK REPORT (BI_BIT = 24000)
========================================================

[+] Generating 10000 edge-case test objects...
[+] Validating correctness...
    -> Validation PASSED. Outputs are 100% identical.

--- Benchmarking BUI ---
--- Benchmarking BUL ---

--------------------------------------------------------------------------
Function          Old (ns)          New (ns)          Speedup
--------------------------------------------------------------------------
highest_bit(bui)  189.696           164.361           1.154x
highest_bit(bul)  327.115           275.332           1.188x
--------------------------------------------------------------------------
*/