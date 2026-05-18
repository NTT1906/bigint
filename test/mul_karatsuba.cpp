#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>

#define BI_BIT (4096 * 1)
#include "../bigint.h"

// ===== Result struct =====
struct BenchResult {
    std::string name;
    double karatsuba_ns;
    double mul_ns;
    double mul_ip_ns;
};

// ===== Prevent optimization =====
static volatile u32 sink = 0;

inline void consume(const bul& x) {
    sink ^= x[0];
}

// ===== Edge case injector =====
void inject_edge_cases(bui& a, bui& b, int i) {
    if (i % 10 == 0) {
        a = bui0();
    } else if (i % 11 == 0) {
        b = bui0();
    } else if (i % 12 == 0) {
        a = bui1();
    } else if (i % 13 == 0) {
        b = bui1();
    } else if (i % 14 == 0) {
        a = b;
    } else if (i % 15 == 0) {
        for (u32 j = 0; j < BI_N; ++j)
            a[j] = 0xFFFFFFFF;
    }
}

// ===== Main =====
int main() {
    std::cout << "========================================================\n";
    std::cout << " KARATSUBA vs MUL_REF (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    const int NUM_TESTS = 200000;

    std::vector<bui> A(NUM_TESTS);
    std::vector<bui> B(NUM_TESTS);

    std::cout << "Generating inputs + edge cases...\n";

    for (int i = 0; i < NUM_TESTS; ++i) {
        randomize_ip(A[i]);
        randomize_ip(B[i]);
        inject_edge_cases(A[i], B[i], i);
    }

    // ===== Correctness =====
    std::cout << "Running correctness verification...\n";

    for (int i = 0; i < NUM_TESTS; ++i) {
        bul r1 = karatsuba(A[i], B[i]);
        bul r2 = mul(A[i], B[i]);

        if (cmp(r1, r2) != 0) {
            std::cout << "==== FAILURE ====\n";
            std::cout << "Index: " << i << "\n";
            std::cout << "A = " << bui_to_hex(A[i], true, true) << "\n";
            std::cout << "B = " << bui_to_hex(B[i], true, true) << "\n";
            std::cout << "KA= " << bui_to_hex(r1.high(), true, true) << ' ' << bui_to_hex(r1.low(), true, true) << "\n";
            std::cout << "MU= " << bui_to_hex(r2.high(), true, true) << ' ' << bui_to_hex(r2.low(), true, true) << "\n";
            std::cout << "A = " << bui_to_dec(A[i]) << "\n";
            std::cout << "B = " << bui_to_dec(B[i]) << "\n";
            std::cout << "KARATSUBA = " << bul_to_dec(r1) << "\n";
            std::cout << "MUL_REF   = " << bul_to_dec(r2) << "\n";
            return 1;
        }
    }

    std::cout << "[SUCCESS] All results match!\n\n";

    // ===== Benchmark =====

    // --- Karatsuba ---
    auto start_k = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_TESTS; ++i) {
        bul r = karatsuba(A[i], B[i]);
        consume(r);
    }
    auto end_k = std::chrono::high_resolution_clock::now();

    // --- mul() ---
    auto start_m = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_TESTS; ++i) {
        bul r = mul(A[i], B[i]);
        consume(r);
    }
    auto end_m = std::chrono::high_resolution_clock::now();

    // --- mul_ip() ---
    auto start_ip = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < NUM_TESTS; ++i) {
        bui tmp = A[i];
        mul_ip(tmp, B[i]);
        sink ^= tmp[0];
    }
    auto end_ip = std::chrono::high_resolution_clock::now();

    // ===== Compute =====
    double karatsuba_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end_k - start_k).count()
        / (double)NUM_TESTS;

    double mul_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end_m - start_m).count()
        / (double)NUM_TESTS;

    double mul_ip_ns =
        std::chrono::duration_cast<std::chrono::nanoseconds>(end_ip - start_ip).count()
        / (double)NUM_TESTS;

    // ===== Report =====
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(20) << "Operation"
              << std::setw(18) << "Karatsuba (ns)"
              << std::setw(18) << "mul (ns)"
              << std::setw(18) << "mul_ip (ns)"
              << "Speedup(K/mul)\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(2);

    std::cout << std::left << std::setw(20) << "multiply"
              << std::setw(18) << karatsuba_ns
              << std::setw(18) << mul_ns
              << std::setw(18) << mul_ip_ns
              << (mul_ns / karatsuba_ns) << "x\n";

    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << "Sink: " << sink << "\n";

    return 0;
}