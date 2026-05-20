#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>

#define BI_BIT_WIDTH 512 // Test with 2048 or 512
#include "../bigint.h"

// ============================
// Prevent optimization
// ============================
volatile uw global_sink = 0;

// Hardware instruction you proved is fastest
BI_ALWAYS_INLINE uw u32_divmod_single_hw(uw hi, uw lo, uw b, uw* rem) {
    uw q, r;
    __asm__("divl %4"
        : "=a"(q), "=d"(r)
        : "0"(lo), "1"(hi), "rm"(b));
    *rem = r;
    return q;
}

// ============================
// Old Version (Static Loop)
// ============================
inline void u32divmod_old(const bui& a, uw b, bui& q, uw& r) {
    q = {};
    r = 0;
    for (uw i = 0; i < BI_N; ++i)
        q[i] = u32_divmod_single_hw(r, a[i], b, &r);
}

inline void u32divmod_old(const bul &a, uw d, bul &q, uw& r) {
    q = {};
    r = 0;
    for (uw i = 0; i < BI_N * 2; ++i)
        q[i] = u32_divmod_single_hw(r, a[i], d, &r);
}

// ============================
// New Version (Dynamic Shrinking)
// ============================
inline void u32divmod_new(const bui& a, uw b, bui& q, uw& r) {
    q = {};
    r = 0;
    uw hl = highest_limb(a);
    if (hl == 0 && a[BI_N - 1] == 0) return;
    for (uw i = BI_N - 1 - hl; i < BI_N; ++i)
        q[i] = u32_divmod_single_hw(r, a[i], b, &r);
}

inline void u32divmod_new(const bul &a, uw d, bul &q, uw& r) {
    q = {};
    r = 0;
    uw hl = highest_limb(a);
    if (hl == 0 && a[BI_N * 2 - 1] == 0) return;
    for (uw i = BI_N * 2 - 1 - hl; i < BI_N * 2; ++i)
        q[i] = u32_divmod_single_hw(r, a[i], d, &r);
}

// ============================
// Benchmark Runner
// ============================
struct BenchResult {
    std::string name;
    double old_ns;
    double new_ns;
    double speedup;
};

int main() {
    std::cout << "========================================================\n";
    std::cout << " DIVMOD ALGORITHM BENCHMARK (BI_BIT = " << BI_BIT_WIDTH << ")\n";
    std::cout << "========================================================\n\n";

    const int NUM_TESTS = 200000;
    std::vector<BenchResult> results;
    std::mt19937 rng(123);

    std::vector<bui> a_data(NUM_TESTS);
    std::vector<bul> big_data(NUM_TESTS);
    std::vector<uw> d_data(NUM_TESTS);

    std::cout << "Generating varying-length inputs...\n";

    for (int i = 0; i < NUM_TESTS; ++i) {
        // Generate full random data
        bui temp_a; randomize_ip(temp_a);
        bul temp_big; randomize_ip(temp_big);

        // Randomly truncate the length to simulate real-world numbers
        uw active_limbs_bui = (rng() % BI_N) + 1;
        uw active_limbs_bul = (rng() % (BI_N * 2)) + 1;

        // Clear the top limbs to create leading zeros
        a_data[i] = {};
        for(uw j = BI_N - active_limbs_bui; j < BI_N; ++j) {
            a_data[i][j] = temp_a[j];
        }

        big_data[i] = {};
        for(uw j = (BI_N * 2) - active_limbs_bul; j < BI_N * 2; ++j) {
            big_data[i][j] = temp_big[j];
        }

        uw d = rng();
        if (d == 0) d = 3;
        d_data[i] = d;
    }

    std::cout << "Running correctness check...\n";
    for (int i = 0; i < NUM_TESTS; ++i) {
        bui q1, q2;
        uw r1, r2;
        u32divmod_old(a_data[i], d_data[i], q1, r1);
        u32divmod_new(a_data[i], d_data[i], q2, r2);

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
        bui q; uw r;

        auto start_old = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            u32divmod_old(a_data[i], d_data[i], q, r);
            global_sink ^= q[BI_N - 1] ^ r;
        }
        auto end_old = std::chrono::high_resolution_clock::now();

        auto start_new = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            u32divmod_new(a_data[i], d_data[i], q, r);
            global_sink ^= q[BI_N - 1] ^ r;
        }
        auto end_new = std::chrono::high_resolution_clock::now();

        double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
        double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;

        results.push_back({"bui divmod", old_ns, new_ns, old_ns / new_ns});
    }

    // =========================
    // bul benchmark
    // =========================
    {
        bul q;

        auto start_old = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            uw r;
            u32divmod_old(big_data[i], d_data[i], q, r);
            global_sink ^= q[(BI_N * 2) - 1] ^ r;
        }
        auto end_old = std::chrono::high_resolution_clock::now();

        auto start_new = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            uw r;
            u32divmod_new(big_data[i], d_data[i], q, r);
            global_sink ^= q[(BI_N * 2) - 1] ^ r;
        }
        auto end_new = std::chrono::high_resolution_clock::now();

        double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
        double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;

        results.push_back({"bul divmod", old_ns, new_ns, old_ns / new_ns});
    }

    // =========================
    // Report
    // =========================
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(18) << "Operation"
              << std::setw(18) << "Old (ns)"
              << std::setw(18) << "New (ns)"
              << "Speedup\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(2);
    for (auto& r : results) {
        std::cout << std::left << std::setw(18) << r.name
                  << std::setw(18) << r.old_ns
                  << std::setw(18) << r.new_ns
                  << r.speedup << "x\n";
    }
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << "\nSink: " << global_sink << "\n";

    return 0;
}
// ========================================================
//  DIVMOD ALGORITHM BENCHMARK (BI_BIT = 2048)
// ========================================================
//
// Generating varying-length inputs...
// Running correctness check...
// [SUCCESS] All results match!
//
// --------------------------------------------------------------------------
// Operation         Old (ns)          New (ns)          Speedup
// --------------------------------------------------------------------------
// bui divmod        158.43            82.50             1.92x
// bul divmod        413.73            176.28            2.35x
// --------------------------------------------------------------------------
//
// Sink: 0
// ========================================================
//  DIVMOD ALGORITHM BENCHMARK (BI_BIT = 512)
// ========================================================
//
// Generating varying-length inputs...
// Running correctness check...
// [SUCCESS] All results match!
//
// --------------------------------------------------------------------------
// Operation         Old (ns)          New (ns)          Speedup
// --------------------------------------------------------------------------
// bui divmod        27.23             16.84             1.62x
// bul divmod        75.52             47.33             1.60x
// --------------------------------------------------------------------------