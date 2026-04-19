#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

// Adjust BI_BIT if needed for your local setup
#define BI_BIT (4096 * 10)
#include "../bigint.h"

// ========================================================================
// Prevent dead-code elimination
// ========================================================================
volatile u32 global_sink = 0;

// ========================================================================
// Old Implementations
// ========================================================================
inline u32 highest_limb_old_bui(const bui &x) {
    for (u32 i = 0; i < BI_N; ++i)
       if (x[i] > 0) return BI_N - i - 1;
    return 0;
}

inline u32 highest_limb_old_bul(const bul &x) {
    for (u32 i = 0; i < BI_N * 2; ++i)
       if (x[i] > 0) return (BI_N * 2 - 1) - i;
    return 0;
}

// ========================================================================
// Benchmark Runner
// ========================================================================
int main() {
    std::cout << "========================================================\n";
    std::cout << "     HIGHEST_LIMB BENCHMARK REPORT (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    // const int DATASET_SIZE = 10000;
    // const int BENCH_ITERATIONS = 10000; // 10k * 10k = 100 Million calls
    const int DATASET_SIZE = 1000;
    const int BENCH_ITERATIONS = 100; // 10k * 10k = 100 Million calls

    std::vector<bui> test_bui(DATASET_SIZE, bui{});
    std::vector<bul> test_bul(DATASET_SIZE, bul{});

    std::mt19937 gen(777);
    std::uniform_int_distribution<u32> val_dist(1, 0xFFFFFFFF);

    std::cout << "[+] Generating " << DATASET_SIZE << " edge-case test objects...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        int edge = gen() % 100;

        if (edge < 10) {
            // Edge Case 1: Pure Zero (Leave as default 0s)
            // Stresses the worst-case scenario for scanning loops
        }
        else if (edge < 30) {
            // Edge Case 2: Small numbers (Data only at the very end LSW)
            // Forces the loop to scan almost the entire array before finding data
            test_bui[i][BI_N - 1] = val_dist(gen);
            test_bul[i][BI_N * 2 - 1] = val_dist(gen);
        }
        else if (edge < 50) {
            // Edge Case 3: Massive numbers (Data at the very beginning MSW)
            // Tests instant early-exit performance
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
    // Validation (Requirement #1)
    // ========================================================================
    std::cout << "[+] Validating correctness...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        u32 res_old_bui = highest_limb_old_bui(test_bui[i]);
        u32 res_new_bui = highest_limb(test_bui[i]);

        if (res_old_bui != res_new_bui) {
            std::cerr << "\n[!] VALIDATION FAILED! PROGRAM HALTED.\n";
            std::cerr << "BUI Index : " << i << "\n";
            std::cerr << "Old Result: " << res_old_bui << "\n";
            std::cerr << "New Result: " << res_new_bui << "\n";
            std::exit(1);
        }

        u32 res_old_bul = highest_limb_old_bul(test_bul[i]);
        u32 res_new_bul = highest_limb(test_bul[i]);

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
        for (const auto& a : test_bui) chk_bui_old ^= highest_limb_old_bui(a);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double old_bui_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bui) chk_bui_new ^= highest_limb(a);
    }
    end = std::chrono::high_resolution_clock::now();
    double new_bui_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;

    std::cout << "--- Benchmarking BUL ---\n\n";
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bul) chk_bul_old ^= highest_limb_old_bul(a);
    }
    end = std::chrono::high_resolution_clock::now();
    double old_bul_ns = std::chrono::duration<double, std::nano>(end - start).count() / total_calls;

    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bul) chk_bul_new ^= highest_limb(a);
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

    std::cout << std::left << std::setw(18) << "highest_limb(bui)"
              << std::setw(18) << old_bui_ns
              << std::setw(18) << new_bui_ns
              << old_bui_ns / new_bui_ns << "x\n";

    std::cout << std::left << std::setw(18) << "highest_limb(bul)"
              << std::setw(18) << old_bul_ns
              << std::setw(18) << new_bul_ns
              << old_bul_ns / new_bul_ns << "x\n";

    std::cout << "--------------------------------------------------------------------------\n";

    global_sink = chk_bui_old ^ chk_bui_new ^ chk_bul_old ^ chk_bul_new;

    return 0;
}
// ========================================================
//      HIGHEST_LIMB BENCHMARK REPORT (BI_BIT = 2048)
// ========================================================
//
// [+] Generating 10000 edge-case test objects...
// [+] Validating correctness...
//     -> Validation PASSED. Outputs are 100% identical.
//
// --- Benchmarking BUI (100 Million calls) ---
// --- Benchmarking BUL (100 Million calls) ---
//
// --------------------------------------------------------------------------
// Function          Old (ns)          New (ns)          Speedup
// --------------------------------------------------------------------------
// highest_limb(bui) 17.326            9.676             1.791x
// highest_limb(bul) 26.575            20.301            1.309x
// --------------------------------------------------------------------------

// ========================================================
//      HIGHEST_LIMB BENCHMARK REPORT (BI_BIT = 512)
// ========================================================
//
// [+] Generating 10000 edge-case test objects...
// [+] Validating correctness...
//     -> Validation PASSED. Outputs are 100% identical.
//
// --- Benchmarking BUI (100 Million calls) ---
// --- Benchmarking BUL (100 Million calls) ---
//
// --------------------------------------------------------------------------
// Function          Old (ns)          New (ns)          Speedup
// --------------------------------------------------------------------------
// highest_limb(bui) 2.870             5.310             0.540x
// highest_limb(bul) 12.740            6.544             1.947x
// --------------------------------------------------------------------------