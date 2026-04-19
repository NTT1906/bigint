#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <sstream>

// Include your bigint library here
#include "../bigint.h"



inline std::string bui_to_dec_old(const bui& x) {
    if (bui_is0(x)) return "0";
    std::vector<u32> parts;
    bui n = x, q{};
    u32 r = 0;
    while (!bui_is0(n)) {
        u32 BASE = 1000000000u;
        u32_divmod(n, BASE, q, r);
        parts.push_back(r);
        n = q;
    }
    std::ostringstream oss;
    oss << parts.back();
    for (int i = (int)parts.size() - 2; i >= 0; --i)
        oss << std::setw(9) << std::setfill('0') << parts[i];
    return oss.str();
}

inline std::string bul_to_dec_old(const bul& x) {
    if (bu_is0_imp(x.data(), BI_N * 2)) return "0";
    std::vector<u32> parts;
    bul n = x, q{};
    while (!bu_is0_imp(n.data(), BI_N * 2)) {
        u32 BASE = 1000000000u;
        u32 r = u32_divmod_bul(n, BASE, q);
        parts.push_back(r);
        n = q;
    }
    std::ostringstream oss;
    oss << parts.back();
    for (int i = (int)parts.size() - 2; i >= 0; --i)
        oss << std::setw(9) << std::setfill('0') << parts[i];
    return oss.str();
}

// ========================================================================
// Prevent dead-code elimination
// ========================================================================
volatile size_t global_sink = 0;

// ========================================================================
// Benchmark Runner
// ========================================================================
int main() {
    std::cout << "========================================================\n";
    std::cout << "        DEC FORMATTER BENCHMARK REPORT (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    // Since divmod is an O(N^2) heavy operation, we scale the iterations carefully
    const int DATASET_SIZE = 10000;
    const int BENCH_ITERATIONS = 1000;

    std::vector<bui> test_bui;
    std::vector<bul> test_bul;
    test_bui.reserve(DATASET_SIZE);
    test_bul.reserve(DATASET_SIZE);

    std::mt19937 gen(42);

    std::cout << "[+] Generating " << DATASET_SIZE << " edge-case test objects...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        bui a{};
        bul big_a{};

        int edge = gen() % 100;
        if (edge < 5) {
            // Edge Case 1: Pure Zero (already init to 0)
        } else if (edge < 15) {
            // Edge Case 2: Small numbers (only LSW active)
            a[BI_N - 1] = gen();
            big_a[BI_N * 2 - 1] = gen();
        } else if (edge < 25) {
            // Edge Case 3: Fully maxed out (all FFs)
            for (int j = 0; j < BI_N; ++j) a[j] = 0xFFFFFFFF;
            for (int j = 0; j < BI_N * 2; ++j) big_a[j] = 0xFFFFFFFF;
        } else {
            // Edge Case 4: Random length big integers
            randomize_ip(a);
            randomize_ip(big_a);

            // Randomly truncate the length to simulate real-world variance
            u32 active_limbs_bui = (gen() % BI_N) + 1;
            u32 active_limbs_bul = (gen() % (BI_N * 2)) + 1;

            // Clear top limbs
            for(u32 j = 0; j < BI_N - active_limbs_bui; ++j) a[j] = 0;
            for(u32 j = 0; j < (BI_N * 2) - active_limbs_bul; ++j) big_a[j] = 0;
        }
        test_bui.push_back(a);
        test_bul.push_back(big_a);
    }

    // ========================================================================
    // Validation (Requirement #1)
    // ========================================================================
    std::cout << "[+] Validating bui_to_dec correctness...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        std::string res_old = bui_to_dec_old(test_bui[i]);
        std::string res_new = bui_to_dec(test_bui[i]);
        if (res_old != res_new) {
            std::cerr << "\n[!] VALIDATION FAILED! PROGRAM HALTED.\n";
            std::cerr << "Old: " << res_old << "\n";
            std::cerr << "New: " << res_new << "\n";
            std::exit(1);
        }
    }

    std::cout << "[+] Validating bul_to_dec correctness...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        std::string res_old = bul_to_dec_old(test_bul[i]);
        std::string res_new = bul_to_dec(test_bul[i]);
        if (res_old != res_new) {
            std::cerr << "\n[!] VALIDATION FAILED! PROGRAM HALTED.\n";
            std::cerr << "Old: " << res_old << "\n";
            std::cerr << "New: " << res_new << "\n";
            std::exit(1);
        }
    }
    std::cout << "    -> Validation PASSED. Outputs are 100% identical.\n\n";

    // ========================================================================
    // Benchmarking bui
    // ========================================================================
    std::cout << "--- Benchmarking BUI (" << BENCH_ITERATIONS << " iters x "
              << DATASET_SIZE << " objects) ---\n";

    size_t chk_old_bui = 0, chk_new_bui = 0;

    auto start_old_bui = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bui) chk_old_bui += bui_to_dec_old(a).length();
    }
    auto end_old_bui = std::chrono::high_resolution_clock::now();

    auto start_new_bui = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bui) chk_new_bui += bui_to_dec(a).length();
    }
    auto end_new_bui = std::chrono::high_resolution_clock::now();

    // ========================================================================
    // Benchmarking bul
    // ========================================================================
    std::cout << "--- Benchmarking BUL (" << BENCH_ITERATIONS << " iters x "
              << DATASET_SIZE << " objects) ---\n\n";

    size_t chk_old_bul = 0, chk_new_bul = 0;

    auto start_old_bul = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bul) chk_old_bul += bul_to_dec_old(a).length();
    }
    auto end_old_bul = std::chrono::high_resolution_clock::now();

    auto start_new_bul = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& a : test_bul) chk_new_bul += bul_to_dec(a).length();
    }
    auto end_new_bul = std::chrono::high_resolution_clock::now();

    // ========================================================================
    // Calculate and Print Results (Requirements #3 & #4)
    // ========================================================================
    double total_calls = (double)DATASET_SIZE * BENCH_ITERATIONS;

    double old_bui_ns = std::chrono::duration<double, std::nano>(end_old_bui - start_old_bui).count() / total_calls;
    double new_bui_ns = std::chrono::duration<double, std::nano>(end_new_bui - start_new_bui).count() / total_calls;

    double old_bul_ns = std::chrono::duration<double, std::nano>(end_old_bul - start_old_bul).count() / total_calls;
    double new_bul_ns = std::chrono::duration<double, std::nano>(end_new_bul - start_new_bul).count() / total_calls;

    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(18) << "Operation"
              << std::setw(18) << "Old (ns)"
              << std::setw(18) << "New (ns)"
              << "Speedup\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(2);

    std::cout << std::left << std::setw(18) << "bui_to_dec"
              << std::setw(18) << old_bui_ns
              << std::setw(18) << new_bui_ns
              << old_bui_ns / new_bui_ns << "x\n";

    std::cout << std::left << std::setw(18) << "bul_to_dec"
              << std::setw(18) << old_bul_ns
              << std::setw(18) << new_bul_ns
              << old_bul_ns / new_bul_ns << "x\n";

    std::cout << "--------------------------------------------------------------------------\n";

    global_sink = chk_old_bui ^ chk_new_bui ^ chk_old_bul ^ chk_new_bul;

    return 0;
}