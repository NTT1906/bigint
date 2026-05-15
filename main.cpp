#include <iostream>
#define BI_BIT 512
#include "bigint.h"

void test_divmod_imp() {
    std::cout << "Testing divmod_knuth_imp...\n";

    // Test 1: Simple division
    {
        bui a = bui_from_u32(100);
        bui b = bui_from_u32(3);
        bui q{}, r{}, q2{}, r2{};

        divmod_knuth2(a, b, q, r);
        divmod_knuth_imp(a.data(), BI_N, b.data(), BI_N, q2.data(), BI_N, r2.data(), BI_N);

        if (cmp(q, q2) != 0 || cmp(r, r2) != 0) {
            std::cerr << "Test 1 FAILED!\n";
            std::exit(1);
        }
        std::cout << "  Test 1 (100/3): q=" << bui_to_dec(q2) << " r=" << bui_to_dec(r2) << " PASS\n";
    }

    // Test 2: Random values comparing divmod_knuth2 vs divmod_knuth_imp
    {
        std::mt19937 gen(123456);
        std::uniform_int_distribution<u32> dist(0, 0xffffffffu);

        for (int i = 0; i < 1000; ++i) {
            bui a{}, b{};
            for (u32 j = 0; j < BI_N; ++j) {
                a[j] = dist(gen);
                b[j] = dist(gen);
            }
            if (bui_is0(b)) b[BI_N-1] = 1;

            // Make b have fewer limbs
            if (i % 5 == 0) {
                for (u32 j = 0; j < BI_N/2; ++j)
                    b[j] = 0;
                b[BI_N/2] |= 1u << 31;
            }

            bui q1{}, r1{}, q2{}, r2{};
            divmod_knuth2(a, b, q1, r1);
            divmod_knuth_imp(a.data(), BI_N, b.data(), BI_N, q2.data(), BI_N, r2.data(), BI_N);

            if (cmp(q1, q2) != 0 || cmp(r1, r2) != 0) {
                std::cerr << "Random test " << i << " FAILED!\n";
                std::exit(1);
            }

            // Verify identity
            bui qb = mul_low(q2, b);
            bui check = add(qb, r2);
            if (cmp(check, a) != 0 || cmp(r2, b) >= 0) {
                std::cerr << "Random test " << i << " IDENTITY FAILED!\n";
                std::exit(1);
            }
        }
        std::cout << "  Test 2 (1000 random): PASS\n";
    }

    // Test 3: edge cases
    {
        // a < b
        {
            bui a = bui_from_u32(5);
            bui b = bui_from_u32(100);
            bui q{}, r{};
            divmod_knuth_imp(a.data(), BI_N, b.data(), BI_N, q.data(), BI_N, r.data(), BI_N);
            if (!bui_is0(q) || cmp(r, a) != 0) {
                std::cerr << "Test 3a (a < b) FAILED!\n";
                std::exit(1);
            }
            std::cout << "  Test 3a (a < b): PASS\n";
        }

        // a == b
        {
            bui a = bui_from_u32(12345);
            bui b = bui_from_u32(12345);
            bui q{}, r{};
            divmod_knuth_imp(a.data(), BI_N, b.data(), BI_N, q.data(), BI_N, r.data(), BI_N);
            bui one = bui1();
            if (cmp(q, one) != 0 || !bui_is0(r)) {
                std::cerr << "Test 3b (a == b) FAILED!\n";
                std::exit(1);
            }
            std::cout << "  Test 3b (a == b): PASS\n";
        }

        // b is single limb
        {
            bui a{};
            for (auto& x : a) x = 0x12345678;
            bui b = bui_from_u32(0xfedcba98);
            bui q1{}, r1{}, q2{}, r2{};
            divmod_knuth2(a, b, q1, r1);
            divmod_knuth_imp(a.data(), BI_N, b.data(), BI_N, q2.data(), BI_N, r2.data(), BI_N);
            if (cmp(q1, q2) != 0 || cmp(r1, r2) != 0) {
                std::cerr << "Test 3c (single limb b) FAILED!\n";
                std::exit(1);
            }
            std::cout << "  Test 3c (single limb b): PASS\n";
        }
    }

    // Test 4: bul/bui (dividing a 2N-width value by N-width using _imp)
    // This tests the TODO cases in the codebase
    {
        std::mt19937 gen(42);
        std::uniform_int_distribution<u32> dist(0, 0xffffffffu);

        for (int i = 0; i < 100; ++i) {
            bul a{};  // 2N capacity
            bui b{};  // N capacity
            for (u32 j = 0; j < BI_2N; ++j) a[j] = dist(gen);
            for (u32 j = 0; j < BI_N; ++j) b[j] = dist(gen);
            if (bui_is0(b)) b[BI_N-1] = 1;

            // Make b have fewer limbs sometimes
            if (i % 5 == 0) {
                for (u32 j = 0; j < BI_N/2; ++j)
                    b[j] = 0;
                b[BI_N/2] |= 1u << 31;
            }

            // If a < b, skip (since bul/bui cmp not directly testable with simple verify)
            // Actually let's use _imp to compute q, r
            bul q{};
            bui r{};

            divmod_knuth_imp(a.data(), BI_2N, b.data(), BI_N, q.data(), BI_2N, r.data(), BI_N);

            // Verify: r < b
            if (cmp(r, b) >= 0) {
                std::cerr << "Test 4 (bul/bui) FAILED: remainder >= divisor at iter " << i << "\n";
                std::exit(1);
            }
        }
        std::cout << "  Test 4 (100 bul/bui): r < b check PASS\n";
    }

    std::cout << "All tests PASSED!\n";
}

int main() {
    test_divmod_imp();
    return 0;
}
