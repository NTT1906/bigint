#include <iostream>
#include <chrono>
#include <cassert>

// Set bit size for the test (512 is good for fast, heavy testing)
#define BI_BIT 512
#include "../bigint.h"

// Helper to generate a random double-width (bul) number
bul random_bul() {
    bui high, low;
    randomize_ip(high);
    randomize_ip(low);
    return bul_from_2bui(high, low);
}

void print_fail(const std::string& name, const bui& a, const bui& b, const bui& res, const bui& expected) {
    std::cerr << "\n[!] TEST FAILED in " << name << "!\n";
    std::cerr << "A = " << bui_to_hex(a) << "\n";
    std::cerr << "B = " << bui_to_hex(b) << "\n";
    std::cerr << "Result   = " << bui_to_hex(res) << "\n";
    std::cerr << "Expected = " << bui_to_hex(expected) << "\n";
    exit(1);
}

int main() {
    std::cout << "========================================\n";
    std::cout << " BIGINT DIVISION HEAVY TEST & BENCHMARK \n";
    std::cout << "========================================\n\n";

    const int NUM_TESTS = 100000;
    std::cout << "Running " << NUM_TESTS << " randomized tests (BI_BIT = " << BI_BIT << ")...\n\n";

    long long time_old = 0;
    long long time_new = 0;
    long long time_knuth = 0;
    long long time_divmod = 0;

    for (int i = 0; i < NUM_TESTS; ++i) {
        bui a, b;
        randomize_ip(a);
        randomize_ip(b);

        // --- INJECT EDGE CASES ---
        if (i % 10 == 0) b = bui_from_u32(1);               // b = 1
        else if (i % 11 == 0) b = bui_from_u32(0xFFFFFFFF); // b = 1 limb max
        else if (i % 12 == 0) b = a;                        // a == b
        else if (i % 13 == 0) { b = a; randomize_ip(a); }   // randomly a < b or a > b
        else if (i % 14 == 0) a = bui0();                   // a = 0
        else if (i % 17 == 0) b[0] = 0;                   // highest limb = 0

        // Prevent division by zero
        if (bui_is0(b)) b = bui_from_u32(1);

        // 1. Run Old mod_native
        auto start = std::chrono::high_resolution_clock::now();
        bui rem_old = mod_native(a, b);
        auto end = std::chrono::high_resolution_clock::now();
        time_old += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        // 2. Run New nmod_native (Sliding Window)
        start = std::chrono::high_resolution_clock::now();
        bui rem_new = nmod_native(a, b);
        end = std::chrono::high_resolution_clock::now();
        time_new += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        // 3. Run divmod_knuth
        bui q_knuth, rem_knuth;
        start = std::chrono::high_resolution_clock::now();
        divmod_knuth(a, b, q_knuth, rem_knuth);
        end = std::chrono::high_resolution_clock::now();
        time_knuth += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        // 4. Run divmod
        bui q_divmod, rem_divmod;
        start = std::chrono::high_resolution_clock::now();
        divmod(a, b, q_divmod, rem_divmod);
        end = std::chrono::high_resolution_clock::now();
        time_divmod += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        // --- VERIFICATION 1: Do they all match? ---
        if (cmp(rem_old, rem_new) != 0) {
            print_fail("nmod_native (bui)", a, b, rem_new, rem_old);
        }
        if (cmp(rem_old, rem_knuth) != 0) {
            print_fail("divmod_knuth", a, b, rem_knuth, rem_old);
        }
        if (cmp(rem_old, rem_divmod) != 0) {
            print_fail("divmod", a, b, rem_divmod, rem_old);
        }

        // --- VERIFICATION 2: Is Knuth mathematically exact? (A == Q * B + R) ---
        bui qb = mul_low(q_knuth, b);
        bui a_verify = add(qb, rem_knuth);
        if (cmp(a, a_verify) != 0) {
            std::cerr << "\n[!] MATHEMATICAL FAILURE IN KNUTH ALGORITHM!\n";
            std::cerr << "A != Q * B + R\n";
            exit(1);
        }
        if (cmp(rem_knuth, b) >= 0) {
            std::cerr << "\n[!] MATHEMATICAL FAILURE IN KNUTH ALGORITHM!\n";
            std::cerr << "Remainder is greater than or equal to Divisor!\n";
            exit(1);
        }

        // --- VERIFICATION 3: Test BUL % BUI Overloads ---
        bul a_bul = random_bul();
        bui rem_bul_old = mod_native(a_bul, b);
        bui rem_bul_new = nmod_native(a_bul, b);

        if (cmp(rem_bul_old, rem_bul_new) != 0) {
            std::cerr << "\n[!] TEST FAILED in nmod_native (bul)!\n";
            std::cerr << "B = " << bui_to_hex(b) << "\n";
            exit(1);
        }

        // Progress tracker
        if ((i + 1) % 10000 == 0) {
            std::cout << "Passed " << (i + 1) << " / " << NUM_TESTS << " tests...\n";
        }
    }

    std::cout << "\n[SUCCESS] All " << NUM_TESTS << " tests passed flawlessly!\n\n";

    std::cout << "=== PERFORMANCE BENCHMARK ===\n";
    std::cout << "Old mod_native : " << time_old / (NUM_TESTS * 1.0)    << " ns\n";
    std::cout << "New nmod_native: " << time_new / (NUM_TESTS * 1.0)    << " ns  (Sliding Window)\n";
    std::cout << "Knuth divmod   : " << time_knuth / (NUM_TESTS * 1.0)  << " ns\n";
    std::cout << "Old divmod     : " << time_divmod / (NUM_TESTS * 1.0) << " ns\n";

    if (time_knuth < time_old) {
        std::cout << "\nKnuth is " << (double)time_old / time_knuth << "x faster than Old mod_native!\n";
    }

    return 0;
}

// ========================================
//  BIGINT DIVISION HEAVY TEST & BENCHMARK
// ========================================
//
// Running 100000 randomized tests (BI_BIT = 512)...
//
// Passed 10000 / 100000 tests...
// Passed 20000 / 100000 tests...
// Passed 30000 / 100000 tests...
// Passed 40000 / 100000 tests...
// Passed 50000 / 100000 tests...
// Passed 60000 / 100000 tests...
// Passed 70000 / 100000 tests...
// Passed 80000 / 100000 tests...
// Passed 90000 / 100000 tests...
// Passed 100000 / 100000 tests...
//
// [SUCCESS] All 100000 tests passed flawlessly!
//
// === PERFORMANCE BENCHMARK ===
// Old mod_native : 1331.37 ns
// New nmod_native: 937.682 ns  (Sliding Window)
// Knuth divmod   : 88.563 ns
// Old divmod     : 1426.52 ns
//
// Knuth is 15.033x faster than Old mod_native!