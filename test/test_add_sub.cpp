#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <random>
#include <cstring>

#define BI_BIT 4096
#include "../bigint.h"

ALWAYS_INLINE u32 add_ip_n_imp_intrin(u32* a, const u32* b, u32 n) {
    unsigned char c = 0;
    while (n-- > 0)
        c = _addcarry_u32(c, a[n], b[n], &a[n]);
    return c;
}

ALWAYS_INLINE u32 add_ip_n_imp_soft(u32* a, const u32* b, u32 n) {
    u32 c = 0;

    u32 i = n;

    // process 4 limbs per iteration
    while (i >= 4) {
        i -= 4;

        u64 s0 = (u64)a[i+3] + b[i+3] + c;
        a[i+3] = (u32)s0;
        c = s0 >> 32;

        u64 s1 = (u64)a[i+2] + b[i+2] + c;
        a[i+2] = (u32)s1;
        c = s1 >> 32;

        u64 s2 = (u64)a[i+1] + b[i+1] + c;
        a[i+1] = (u32)s2;
        c = s2 >> 32;

        u64 s3 = (u64)a[i+0] + b[i+0] + c;
        a[i+0] = (u32)s3;
        c = s3 >> 32;
    }

    // tail
    while (i--) {
        u64 s = (u64)a[i] + b[i] + c;
        a[i] = (u32)s;
        c = s >> 32;
    }

    return c;
}

ALWAYS_INLINE u32 sub_ip_n_imp_intrin(u32* a, const u32* b, u32 n) {
     unsigned char br = 0;
     while (n-- > 0)
         br = _subborrow_u32(br, a[n], b[n], &a[n]);
     return br;
}

ALWAYS_INLINE u32 sub_ip_n_imp_soft(u32* a, const u32* b, u32 n) {
    u32 br = 0;

    u32 i = n;

    while (i >= 4) {
        i -= 4;

        u64 d0 = (u64)a[i+3] - b[i+3] - br;
        a[i+3] = (u32)d0;
        br = (d0 >> 32) & 1;

        u64 d1 = (u64)a[i+2] - b[i+2] - br;
        a[i+2] = (u32)d1;
        br = (d1 >> 32) & 1;

        u64 d2 = (u64)a[i+1] - b[i+1] - br;
        a[i+1] = (u32)d2;
        br = (d2 >> 32) & 1;

        u64 d3 = (u64)a[i+0] - b[i+0] - br;
        a[i+0] = (u32)d3;
        br = (d3 >> 32) & 1;
    }

    while (i--) {
        u64 d = (u64)a[i] - b[i] - br;
        a[i] = (u32)d;
        br = (d >> 32) & 1;
    }

    return br;
}

struct BenchResult {
    std::string name;
    double soft_ns;
    double intrin_ns;
    double speedup;
};

int main() {
    std::cout << "========================================================\n";
    std::cout << " ADD/SUB LIMB BENCHMARK (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    const int NUM_TESTS = 1000000;
    std::vector<BenchResult> results;

    std::mt19937 rng(123);

    // Data
    std::vector<std::vector<u32>> A(NUM_TESTS, std::vector<u32>(BI_N));
    std::vector<std::vector<u32>> B(NUM_TESTS, std::vector<u32>(BI_N));

    std::cout << "Generating inputs + edge cases...\n";

    for (int i = 0; i < NUM_TESTS; ++i) {
        for (u32 j = 0; j < BI_N; ++j) {
            A[i][j] = rng();
            B[i][j] = rng();
        }

        // Edge injections
        if (i % 10 == 0) {
            std::memset(A[i].data(), 0, BI_N * sizeof(u32));
        } else if (i % 11 == 0) {
            std::memset(B[i].data(), 0, BI_N * sizeof(u32));
        } else if (i % 12 == 0) {
            std::fill(A[i].begin(), A[i].end(), 0xFFFFFFFF);
        } else if (i % 13 == 0) {
            std::fill(B[i].begin(), B[i].end(), 0xFFFFFFFF);
        } else if (i % 14 == 0) {
            A[i] = B[i];
        }
    }

    std::cout << "Running correctness checks...\n";

    // =========================
    // ADD correctness
    // =========================
    for (int i = 0; i < NUM_TESTS; ++i) {
        auto a1 = A[i];
        auto a2 = A[i];

        u32 c1 = add_ip_n_imp_soft(a1.data(), B[i].data(), BI_N);
        u32 c2 = add_ip_n_imp_intrin(a2.data(), B[i].data(), BI_N);

        if (c1 != c2 || std::memcmp(a1.data(), a2.data(), BI_N * sizeof(u32)) != 0) {
            std::cerr << "[FAIL] ADD mismatch at " << i << "\n";
            exit(1);
        }
    }

    // =========================
    // SUB correctness
    // =========================
    for (int i = 0; i < NUM_TESTS; ++i) {
        auto a1 = A[i];
        auto a2 = A[i];

        u32 b1 = sub_ip_n_imp_soft(a1.data(), B[i].data(), BI_N);
        u32 b2 = sub_ip_n_imp_intrin(a2.data(), B[i].data(), BI_N);

        if (b1 != b2 || std::memcmp(a1.data(), a2.data(), BI_N * sizeof(u32)) != 0) {
            std::cerr << "[FAIL] SUB mismatch at " << i << "\n";
            exit(1);
        }
    }

    std::cout << "[SUCCESS] Correctness verified!\n\n";

    // =========================
    // Benchmark ADD
    // =========================
    {
        auto start_soft = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            auto tmp = A[i];
            add_ip_n_imp_soft(tmp.data(), B[i].data(), BI_N);
        }
        auto end_soft = std::chrono::high_resolution_clock::now();

        auto start_intrin = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            auto tmp = A[i];
            add_ip_n_imp_intrin(tmp.data(), B[i].data(), BI_N);
        }
        auto end_intrin = std::chrono::high_resolution_clock::now();

        double soft_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_soft - start_soft).count() / NUM_TESTS;
        double intrin_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_intrin - start_intrin).count() / NUM_TESTS;

        results.push_back({"ADD", soft_ns, intrin_ns, soft_ns / intrin_ns});
    }

    // =========================
    // Benchmark SUB
    // =========================
    {
        auto start_soft = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            auto tmp = A[i];
            sub_ip_n_imp_soft(tmp.data(), B[i].data(), BI_N);
        }
        auto end_soft = std::chrono::high_resolution_clock::now();

        auto start_intrin = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            auto tmp = A[i];
            sub_ip_n_imp_intrin(tmp.data(), B[i].data(), BI_N);
        }
        auto end_intrin = std::chrono::high_resolution_clock::now();

        double soft_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_soft - start_soft).count() / NUM_TESTS;
        double intrin_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_intrin - start_intrin).count() / NUM_TESTS;

        results.push_back({"SUB", soft_ns, intrin_ns, soft_ns / intrin_ns});
    }

    // =========================
    // Report
    // =========================
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(10) << "Op"
              << std::setw(18) << "Soft (ns)"
              << std::setw(18) << "Intrin (ns)"
              << "Speedup\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(2);
    for (auto& r : results) {
        std::cout << std::left << std::setw(10) << r.name
                  << std::setw(18) << r.soft_ns
                  << std::setw(18) << r.intrin_ns
                  << r.speedup << "x\n";
    }
    std::cout << "--------------------------------------------------------------------------\n";

    return 0;
}