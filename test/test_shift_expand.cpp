#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cassert>

// Push BI_BIT high to expose the stack-copy and division overheads
#define BI_BIT 512
#include "../bigint.h"

struct BenchResult {
    std::string name;
    double old_ns;
    double new_ns;
    double speedup;
};

int main() {
    std::cout << "========================================================\n";
    std::cout << " EXTREME SHIFT & MODULO BENCHMARK (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    const int NUM_TESTS = 1000000;
    std::vector<BenchResult> results;

    std::cout << "Pre-generating " << NUM_TESTS << " random inputs & injecting edge cases...\n";

    std::vector<bui> x_data(NUM_TESTS);
    std::vector<bui> m_data(NUM_TESTS);
    std::vector<u32> k_data(NUM_TESTS);

    for (int i = 0; i < NUM_TESTS; ++i) {
        randomize_ip(x_data[i]);
        randomize_ip(m_data[i]);

        // Ensure valid modulus (odd and > 1)
        set_bit_ip(m_data[i], 0, 1);
        if (cmp(m_data[i], bui1()) <= 0) m_data[i] = bui_from_u32(3);

        k_data[i] = rand() % BI_BIT;

        // --- INJECT EDGE CASES ---
        if (i % 10 == 0) {
            k_data[i] = 0;                                 // Edge: k = 0
        } else if (i % 11 == 0) {
            k_data[i] = (rand() % BI_N) * 32;        // Edge: Perfect limb boundary
        } else if (i % 12 == 0) {
            x_data[i] = bui0();                            // Edge: x = 0
        } else if (i % 13 == 0) {
            m_data[i] = bui_from_u32(0xFFFFFFFF);          // Edge: 1-limb fast-path divisor
        } else if (i % 14 == 0) {
            x_data[i] = m_data[i];                         // Edge: x == m
        }
    }

    std::cout << "Running mathematical verifications and benchmarks...\n\n";

    // ==========================================
    // 1. Expand Shift (bui -> bul)
    // ==========================================
    {
        std::vector<bul> old_res(NUM_TESTS);
        std::vector<bul> new_res(NUM_TESTS);

        auto start_old = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            old_res[i] = shift_left_expand(x_data[i], k_data[i]);
        }
        auto end_old = std::chrono::high_resolution_clock::now();

        auto start_new = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            new_res[i] = shift_left_expand_fused(x_data[i], k_data[i]);
        }
        auto end_new = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_TESTS; ++i) {
            if (cmp(old_res[i], new_res[i]) != 0) {
                std::printf("== Result== \n");
                std::printf("x=\t%s\n", bui_to_dec(x_data[i]).c_str());
                std::printf("k=\t%d\n", k_data[i]);
                // std::printf("OLD:\t%s\n", bul_to_hex(old_res[i], true, true).c_str());
                // std::printf("NEW:\t%s\n", bul_to_hex(new_res[i], true, true).c_str());

                std::cerr << "[!] FAILED: shift_left_expand mismatch at index " << i << "\n";
                exit(1);
            }
        }

        double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
        double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;
        results.push_back({"Expand Shift", old_ns, new_ns, old_ns / new_ns});
    }

    // ==========================================
    // 2. Shift Left Modulo (bui -> bui)
    // ==========================================
    {
        std::vector<bui> old_res(NUM_TESTS);
        std::vector<bui> new_res(NUM_TESTS);

        auto start_old = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            old_res[i] = shift_left_mod(x_data[i], k_data[i], m_data[i]);
        }
        auto end_old = std::chrono::high_resolution_clock::now();

        auto start_new = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            new_res[i] = shift_left_mod2(x_data[i], k_data[i], m_data[i]);
        }
        auto end_new = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_TESTS; ++i) {
            if (cmp(old_res[i], new_res[i]) != 0) {
                std::printf("== Result== \n");
                std::printf("x=\t%s\n", bui_to_dec(x_data[i]).c_str());
                std::printf("m=\t%s\n", bui_to_dec(m_data[i]).c_str());
                std::printf("k=\t%d\n", k_data[i]);
                std::printf("OLD:\t%s\n", bui_to_hex(old_res[i], true, true).c_str());
                std::printf("NEW:\t%s\n", bui_to_hex(new_res[i], true, true).c_str());
                std::cerr << "[!] FAILED: shift_left_mod mismatch at index " << i << "\n";
                exit(1);
            }
        }

        double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
        double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;
        results.push_back({"Shift Modulo", old_ns, new_ns, old_ns / new_ns});
    }

    // ==========================================
    // Print Report
    // ==========================================
    std::cout << "[SUCCESS] Mathematical Verification Passed for ALL Edge Cases!\n\n";
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(18) << "Operation"
              << std::setw(18) << "Old (ns/call)"
              << std::setw(18) << "New (ns/call)"
              << "Speedup\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(2);
    for (const auto& r : results) {
        std::cout << std::left << std::setw(18) << r.name
                  << std::setw(18) << r.old_ns
                  << std::setw(18) << r.new_ns
                  << r.speedup << "x\n";
    }
    std::cout << "--------------------------------------------------------------------------\n";

    return 0;
}

// ========================================================
//  EXTREME SHIFT & MODULO BENCHMARK (BI_BIT = 512)
// ========================================================
//
// Pre-generating 1000000 random inputs & injecting edge cases...
// Running mathematical verifications and benchmarks...
//
// [SUCCESS] Mathematical Verification Passed for ALL Edge Cases!
//
// --------------------------------------------------------------------------
// Operation         Old (ns/call)     New (ns/call)     Speedup
// --------------------------------------------------------------------------
// Expand Shift      48.87             22.06             2.22x
// Shift Modulo      7520.02           8477.80           0.89x
// Shift NModulo     7367.95           8160.56           0.90x
// --------------------------------------------------------------------------

// ========================================================
//  EXTREME SHIFT & MODULO BENCHMARK (BI_BIT = 512)
// ========================================================
//
// Pre-generating 1000000 random inputs & injecting edge cases...
// Running mathematical verifications and benchmarks...
//
// [SUCCESS] Mathematical Verification Passed for ALL Edge Cases!
//
// --------------------------------------------------------------------------
// Operation         Old (ns/call)     New (ns/call)     Speedup
// --------------------------------------------------------------------------
// Expand Shift      61.11             23.32             2.62x
// Shift NModulo     8174.78           6639.09           1.23x
// --------------------------------------------------------------------------
// Running mathematical verifications and benchmarks...
//
// [SUCCESS] Mathematical Verification Passed for ALL Edge Cases!
