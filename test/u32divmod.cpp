#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>

#define BI_BIT 24000
#include "../bigint.h"

// ============================
// Prevent optimization
// ============================
volatile u32 global_sink = 0;

// ============================
// Soft version
// ============================
BI_ALWAYS_INLINE u32 u32_divmod_single_soft(u32 hi, u32 lo, u32 b, u32* rem) {
    u64 dividend = (u64)hi << 32 | lo;
    *rem = (u32)(dividend % b);
    return (u32)(dividend / b);
}

// ============================
// HW version (divl)
// ============================
BI_ALWAYS_INLINE u32 u32_divmod_single_hw(u32 hi, u32 lo, u32 b, u32* rem) {
    u32 q, r;
    __asm__("divl %4"
        : "=a"(q), "=d"(r)
        : "0"(lo), "1"(hi), "rm"(b));
    *rem = r;
    return q;
}

// ============================
// bui version
// ============================
inline void u32divmod_soft(const bui& a, u32 b, bui& q, u32& r) {
    q = {};
    r = 0;
    for (u32 i = 0; i < BI_N; ++i)
        q[i] = u32_divmod_single_soft(r, a[i], b, &r);
}

inline void u32divmod_hw(const bui& a, u32 b, bui& q, u32& r) {
    q = {};
    r = 0;
    for (u32 i = 0; i < BI_N; ++i)
        q[i] = u32_divmod_single_hw(r, a[i], b, &r);
}

// ============================
// bul version
// ============================
inline u32 u32_divmod_soft(const bul &a, u32 d, bul &q) {
    u32 r = 0;
    for (u32 i = 0; i < BI_N * 2; ++i)
        q[i] = u32_divmod_single_soft(r, a[i], d, &r);
    return r;
}

inline u32 u32_divmod_hw(const bul &a, u32 d, bul &q) {
    u32 r = 0;
    for (u32 i = 0; i < BI_N * 2; ++i)
        q[i] = u32_divmod_single_hw(r, a[i], d, &r);
    return r;
}

// ============================
// Benchmark
// ============================
struct BenchResult {
    std::string name;
    double soft_ns;
    double hw_ns;
    double speedup;
};

int main() {
    std::cout << "========================================================\n";
    std::cout << " DIVMOD BENCHMARK (FIXED) (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    const int NUM_TESTS = 200000;
    std::vector<BenchResult> results;

    std::mt19937 rng(123);

    std::vector<bui> a_data(NUM_TESTS);
    std::vector<bul> big_data(NUM_TESTS);
    std::vector<u32> d_data(NUM_TESTS);

    std::cout << "Generating inputs + edge cases...\n";

    for (int i = 0; i < NUM_TESTS; ++i) {
        randomize_ip(a_data[i]);
        randomize_ip(big_data[i]);

        u32 d = rng();
        if (d == 0) d = 3;

        // edge cases
        if (i % 10 == 0) d = 1;
        else if (i % 11 == 0) d = 0xFFFFFFFF;
        else if (i % 12 == 0) d = 2;

        d_data[i] = d;
    }

    std::cout << "Running correctness check...\n";

    for (int i = 0; i < NUM_TESTS; ++i) {
        bui q1, q2;
        u32 r1, r2;

        u32divmod_soft(a_data[i], d_data[i], q1, r1);
        u32divmod_hw(a_data[i], d_data[i], q2, r2);

        if (cmp(q1, q2) != 0 || r1 != r2) {
            std::cerr << "[FAIL] bui mismatch at " << i << "\n";
            exit(1);
        }
    }

    std::cout << "[SUCCESS] All results match!\n\n";

    // =========================
    // bui benchmark
    // =========================
    {
        bui q;
        u32 r;

        auto start_soft = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            u32divmod_soft(a_data[i], d_data[i], q, r);
            global_sink ^= q[0] ^ r;
        }
        auto end_soft = std::chrono::high_resolution_clock::now();

        auto start_hw = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            u32divmod_hw(a_data[i], d_data[i], q, r);
            global_sink ^= q[0] ^ r;
        }
        auto end_hw = std::chrono::high_resolution_clock::now();

        double soft_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_soft - start_soft).count() / NUM_TESTS;
        double hw_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_hw - start_hw).count() / NUM_TESTS;

        results.push_back({"bui divmod", soft_ns, hw_ns, soft_ns / hw_ns});
    }

    // =========================
    // bul benchmark
    // =========================
    {
        bul q;

        auto start_soft = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            u32 r = u32_divmod_soft(big_data[i], d_data[i], q);
            global_sink ^= q[0] ^ r;
        }
        auto end_soft = std::chrono::high_resolution_clock::now();

        auto start_hw = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            u32 r = u32_divmod_hw(big_data[i], d_data[i], q);
            global_sink ^= q[0] ^ r;
        }
        auto end_hw = std::chrono::high_resolution_clock::now();

        double soft_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_soft - start_soft).count() / NUM_TESTS;
        double hw_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_hw - start_hw).count() / NUM_TESTS;

        results.push_back({"bul divmod", soft_ns, hw_ns, soft_ns / hw_ns});
    }

    // =========================
    // Report
    // =========================
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(18) << "Operation"
              << std::setw(18) << "Soft (ns)"
              << std::setw(18) << "HW (ns)"
              << "Speedup\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(2);
    for (auto& r : results) {
        std::cout << std::left << std::setw(18) << r.name
                  << std::setw(18) << r.soft_ns
                  << std::setw(18) << r.hw_ns
                  << r.speedup << "x\n";
    }

    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << "\nSink: " << global_sink << "\n";

    return 0;
}

// ========================================================
//  DIVMOD BENCHMARK (FIXED) (BI_BIT = 2048)
// ========================================================
//
// Generating inputs + edge cases...
// Running correctness check...
// [SUCCESS] All results match!
//
// --------------------------------------------------------------------------
// Operation         Soft (ns)         HW (ns)           Speedup
// --------------------------------------------------------------------------
// bui divmod        330.61            199.11            1.66x
// bul divmod        666.63            429.33            1.55x
// --------------------------------------------------------------------------
//
// Sink: 0
// ========================================================
//  DIVMOD BENCHMARK (FIXED) (BI_BIT = 512)
// ========================================================
//
// Generating inputs + edge cases...
// Running correctness check...
// [SUCCESS] All results match!
//
// --------------------------------------------------------------------------
// Operation         Soft (ns)         HW (ns)           Speedup
// --------------------------------------------------------------------------
// bui divmod        131.48            50.02             2.63x
// bul divmod        201.05            103.04            1.95x
// --------------------------------------------------------------------------
// Sink: 0