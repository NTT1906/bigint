#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>

#define BI_BIT 4096
#include "../bigint.h"

struct BenchResult {
    std::string name;
    double old_ns;
    double new_ns;
    double speedup;
};

inline bui mul_low_fast_based_u128(const bui& a, const bui& b) {
#if !defined(__SIZEOF_INT128__)
#error "__int128 required for this implementation"
#endif
    using u128 = __uint128_t;
    u128 acc[BI_N] = {};
    for (u32 k = 0; k < BI_N; ++k) {
        u128 s = 0;
        for (u32 i = 0; i <= k; ++i)
            s += (u128)a[BI_N - 1 - i] * b[BI_N - 1 - (k - i)];
        acc[k] = s;
    }
    bui r{};
    u128 c = 0;
    for (u32 k = 0; k < BI_N; ++k) {
        u128 s = acc[k] + c;
        r[BI_N - 1 - k] = (u32)s;
        c = s >> 32;
    }
    return r;
}

int main() {
    std::cout << "========================================================\n";
    std::cout << " MUL_LOW_FAST BENCHMARK (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    const int NUM_TESTS = 500000; // lower than shift test (mul is heavier)
    std::vector<BenchResult> results;

    std::cout << "Pre-generating " << NUM_TESTS << " inputs & injecting edge cases...\n";

    std::vector<bui> a_data(NUM_TESTS);
    std::vector<bui> b_data(NUM_TESTS);

    std::mt19937 rng(123);

    for (int i = 0; i < NUM_TESTS; ++i) {
        randomize_ip(a_data[i]);
        randomize_ip(b_data[i]);

        // ===== EDGE CASE INJECTION =====
        if (i % 10 == 0) {
            a_data[i] = bui0(); // zero
        } else if (i % 11 == 0) {
            b_data[i] = bui0();
        } else if (i % 12 == 0) {
            a_data[i] = bui1(); // one
        } else if (i % 13 == 0) {
            b_data[i] = bui1();
        } else if (i % 14 == 0) {
            a_data[i] = b_data[i]; // equal
        } else if (i % 15 == 0) {
            // max limb stress
            for (u32 j = 0; j < BI_N; ++j)
                a_data[i][j] = 0xFFFFFFFF;
        } else if (i % 16 == 0) {
            for (u32 j = 0; j < BI_N; ++j)
                b_data[i][j] = 0xFFFFFFFF;
        }
    }

    std::cout << "Running correctness verification...\n";

    // ==========================================
    // Correctness Check
    // ==========================================
    for (int i = 0; i < NUM_TESTS; ++i) {
        bui r1 = mul_low_fast(a_data[i], b_data[i]);
        bui r2 = mul_low_fast_based_u128(a_data[i], b_data[i]);

        if (cmp(r1, r2) != 0) {
            std::cout << "==== FAILURE DETECTED ====\n";
            std::cout << "Index: " << i << "\n";
            std::cout << "A = " << bui_to_hex(a_data[i], true, true) << "\n";
            std::cout << "B = " << bui_to_hex(b_data[i], true, true) << "\n";
            std::cout << "ROW = " << bui_to_hex(r1, true, true) << "\n";
            std::cout << "U128= " << bui_to_hex(r2, true, true) << "\n";
            exit(1);
        }
    }

    std::cout << "[SUCCESS] All results match!\n\n";

    // ==========================================
    // Benchmark: row-wise
    // ==========================================
    std::vector<bui> sink(NUM_TESTS);

    auto start_old = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_TESTS; ++i) {
        sink[i] = mul_low_fast(a_data[i], b_data[i]);
    }
    auto end_old = std::chrono::high_resolution_clock::now();

    // ==========================================
    // Benchmark: u128
    // ==========================================
    auto start_new = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_TESTS; ++i) {
        sink[i] = mul_low_fast_based_u128(a_data[i], b_data[i]);
    }
    auto end_new = std::chrono::high_resolution_clock::now();

    double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
    double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;

    results.push_back({
        "mul_low_fast",
        old_ns,
        new_ns,
        old_ns / new_ns
    });

    // ==========================================
    // Report
    // ==========================================
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Operation"
              << std::setw(18) << "Row (ns/call)"
              << std::setw(18) << "u128 (ns/call)"
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