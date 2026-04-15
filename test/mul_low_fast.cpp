#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>

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
    std::cout << " MUL_LOW_FAST OLD vs NEW (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    const int NUM_TESTS = 300000;
    std::vector<BenchResult> results;

    std::vector<bui> a_data(NUM_TESTS);
    std::vector<bui> b_data(NUM_TESTS);

    std::mt19937 rng(123);

    std::cout << "Generating inputs + edge cases...\n";

    for (int i = 0; i < NUM_TESTS; ++i) {
        randomize_ip(a_data[i]);
        randomize_ip(b_data[i]);

        // ===== EDGE CASES =====
        if (i % 10 == 0) {
            a_data[i] = bui0();
        } else if (i % 11 == 0) {
            b_data[i] = bui0();
        } else if (i % 12 == 0) {
            a_data[i] = bui1();
        } else if (i % 13 == 0) {
            b_data[i] = bui1();
        } else if (i % 14 == 0) {
            a_data[i] = b_data[i];
        } else if (i % 15 == 0) {
            for (u32 j = 0; j < BI_N; ++j)
                a_data[i][j] = 0xFFFFFFFF;
        }
    }

    std::cout << "Running correctness verification...\n";

    for (int i = 0; i < NUM_TESTS; ++i) {
        bui r1 = mul_low_fast_old(a_data[i], b_data[i]);
        bui r2 = mul_low_fast(a_data[i], b_data[i]);

        if (cmp(r1, r2) != 0) {
            std::cout << "==== FAILURE ====\n";
            std::cout << "Index: " << i << "\n";
            std::cout << "A = " << bui_to_hex(a_data[i], true, true) << "\n";
            std::cout << "B = " << bui_to_hex(b_data[i], true, true) << "\n";
            std::cout << "OLD= " << bui_to_hex(r1, true, true) << "\n";
            std::cout << "NEW= " << bui_to_hex(r2, true, true) << "\n";
            exit(1);
        }
    }

    std::cout << "[SUCCESS] All results match!\n\n";

    std::vector<bui> sink(NUM_TESTS);

    // =========================
    // OLD
    // =========================
    auto start_old = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_TESTS; ++i)
        sink[i] = mul_low_fast_old(a_data[i], b_data[i]);
    auto end_old = std::chrono::high_resolution_clock::now();

    // =========================
    // NEW
    // =========================
    auto start_new = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_TESTS; ++i)
        sink[i] = mul_low_fast(a_data[i], b_data[i]);
    auto end_new = std::chrono::high_resolution_clock::now();

    double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
    double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;

    results.push_back({"mul_low_fast", old_ns, new_ns, old_ns / new_ns});

    // =========================
    // Report
    // =========================
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Operation"
              << std::setw(18) << "Old (ns/call)"
              << std::setw(18) << "New (ns/call)"
              << "Speedup\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(2);
    for (const auto& r : results) {
        std::cout << std::left << std::setw(20) << r.name
                  << std::setw(18) << r.old_ns
                  << std::setw(18) << r.new_ns
                  << r.speedup << "x\n";
    }

    std::cout << "--------------------------------------------------------------------------\n";

    return 0;
}

// ======================================================== MUL_LOW_FAST OLD vs NEW (BI_BIT = 2048) ========================================================
// Generating inputs + edge cases...
// Running correctness verification...
// [SUCCESS] All results match!
// --------------------------------------------------------------------------
// Operation Old (ns/call) New (ns/call) Speedup
// --------------------------------------------------------------------------
// mul_low_fast 3132.24 1152.81 2.72x
// --------------------------------------------------------------------------
// ========================================================
//  MUL_LOW_FAST OLD vs NEW (BI_BIT = 512)
// ========================================================
//
// Generating inputs + edge cases...
// Running correctness verification...
// [SUCCESS] All results match!
//
// --------------------------------------------------------------------------
// Operation           Old (ns/call)     New (ns/call)     Speedup
// --------------------------------------------------------------------------
// mul_low_fast        91.45             77.87             1.17x
// --------------------------------------------------------------------------