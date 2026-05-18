#include <iostream>
#include <chrono>
#include <cassert>

// Set bit size for the test (512 is good for fast, heavy testing)
#define BI_BIT 1024
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
    std::cerr << "A = " << bui_to_dec(a) << "\n";
    std::cerr << "B = " << bui_to_dec(b) << "\n";
    std::cerr << "Result   = " << bui_to_dec(res) << "\n";
    std::cerr << "Expected = " << bui_to_dec(expected) << "\n";
    exit(1);
}

inline void old_divmod_knuth(const bui& a, const bui& b, bui& quot, bui& rem) {
	assert(!bui_is0(b));
	int cm = cmp(a, b);
	if (cm < 0) {
		quot = {};
		rem = a;
		return;
	}
	if (cm == 0) {
		quot = bui1();
		rem = {};
		return;
	}

	// 1. Normalize
	bul r = bui_to_bul(a);
	bui d = b;
	u32 d_lead_pow = highest_limb(b);
	u32 d_msw_idx = BI_N - 1 - d_lead_pow;
	u32 d0 = d[d_msw_idx];
	const u32 norm_shift = d0 == 0 ? 0 : BI_SBU32 - highest_bit(d0);

	if (norm_shift > 0) {
		shift_left_ip(d, norm_shift);
		shift_left_ip(r, norm_shift);
	}

	// Recalculate divisor info after normalization
	d_lead_pow = highest_limb(d);
	d_msw_idx = BI_N - 1 - d_lead_pow;
	d0 = d[d_msw_idx];
	const u32 d1 = (d_msw_idx + 1 < BI_N) ? d[d_msw_idx + 1] : 0;
	const u32 n = d_lead_pow + 1; // number of limbs in divisor

	// 2. Fast path for single-limb divisor (n = 1)
	if (n == 1) {
		bul q_bul = {};
		u32 r_temp = 0;
		u32 d_val = d[BI_N-1];

		for (int i = 0; i < BI_N * 2; ++i) {
			u64 dividend = (u64)r_temp << BI_SBU32 | r[i];
			q_bul[i] = (u32)(dividend / d_val);
			r_temp = (u32)(dividend % d_val);
		}
		std::copy(q_bul.begin() + BI_N, q_bul.end(), quot.begin());
		rem = bui_from_u32(r_temp >> norm_shift); // Denormalize remainder instantly
		return;
	}

	// 3. Knuth Division Loop
	quot = {};
	u32 r_lead_pow = highest_limb(r);
	const int m = (int)r_lead_pow - (int)d_lead_pow;

	for (int j = m; j >= 0; --j) {
		u32 r_idx = (BI_N * 2 - 1) - (j + n);

		u32 u_jn = r[r_idx];
		u32 u_jn1 = (r_idx + 1 < BI_N * 2) ? r[r_idx + 1] : 0;
		u32 u_jn2 = (r_idx + 2 < BI_N * 2) ? r[r_idx + 2] : 0;

		u64 r_top = ((u64)u_jn << BI_SBU32) | u_jn1;
		u64 qhat, rhat;

		// Calculate initial guess
		if (u_jn == d0) {
			qhat = 0xFFFFFFFFULL;
			rhat = (u64)u_jn1 + d0;
		} else {
			qhat = r_top / d0;
			rhat = r_top % d0;
		}

		// Knuth's correction step (Refactored to completely avoid overflow)
		while (n > 1) {
			if (rhat >= (1ULL << BI_SBU32)) break;
			if (qhat * d1 <= (rhat << BI_SBU32) + u_jn2) break;
			qhat--;
			rhat += d0;
		}

		// Multiply and subtract safely
		u64 borrow = 0;
		u32 d_lsw_idx = BI_N - 1;

		for (u32 i = 0; i < n; ++i) {
			u32 r_i = r_idx + n - i;
			u32 d_i = d_lsw_idx - i;

			u64 sub = qhat * d[d_i] + borrow;
			// Safe subtraction prevents u64 underflow
			borrow = (sub >> BI_SBU32) + (r[r_i] < (u32)sub);
			// if (r[r_i] < (u32)sub)
				// borrow = (sub >> BI_SBU32) + 1;
			// else
				// borrow = (sub >> BI_SBU32);
			r[r_i] -= (u32)sub;
		}

		bool is_negative = borrow > r[r_idx];
		r[r_idx] = r[r_idx] - (u32)borrow;

		// Store quotient digit
		if (j < BI_N) {
			u32 q_idx = BI_N - 1 - j;
			quot[q_idx] = (u32)qhat;
		}

		// Add back if guess was too high
		if (is_negative) {
			if (j < BI_N) {
				u32 q_idx = BI_N - 1 - j;
				quot[q_idx] = quot[q_idx] - 1;
			}

			u64 carry = 0;
			for (u32 i = 0; i < n; ++i) {
				u32 r_i = r_idx + n - i;
				u32 d_i = d_lsw_idx - i;

				u64 sum = (u64)r[r_i] + d[d_i] + carry;
				r[r_i] = (u32)sum;
				carry = sum >> BI_SBU32;
			}
			r[r_idx] = (u32)(r[r_idx] + carry);
		}
	}

	// 4. Denormalize remainder
	if (norm_shift > 0)
		shift_right_ip(r, norm_shift);
	rem = bul_low(r);
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
    long long time_knuth_old = 0;
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
        else if (i % 17 == 0) b[0] = 0;                     // highest limb = 0

        // Prevent division by zero
        if (bui_is0(b)) b = bui_from_u32(1);

        // 1. Run Old mod_native
        auto start = std::chrono::high_resolution_clock::now();
        bui rem_old = mod_native_deprecated(a, b);
        auto end = std::chrono::high_resolution_clock::now();
        time_old += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        // 2. Run New nmod_native (Sliding Window)
        start = std::chrono::high_resolution_clock::now();
        bui rem_new = nmod_native(a, b);
        end = std::chrono::high_resolution_clock::now();
        time_new += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        // 3. Run current divmod_knuth
        bui q_knuth, rem_knuth;
        start = std::chrono::high_resolution_clock::now();
        divmod_knuth(a, b, q_knuth, rem_knuth);
        end = std::chrono::high_resolution_clock::now();
        time_knuth += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        // 3.1 Run old_divmod_knuth
        bui q_knuth_old, rem_knuth_old;
        start = std::chrono::high_resolution_clock::now();
        old_divmod_knuth(a, b, q_knuth_old, rem_knuth_old);
        end = std::chrono::high_resolution_clock::now();
        time_knuth_old += std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

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
        if (cmp(rem_old, rem_knuth_old) != 0) {
            print_fail("old_divmod_knuth", a, b, rem_knuth_old, rem_old);
        }
        if (cmp(rem_old, rem_divmod) != 0) {
            print_fail("divmod", a, b, rem_divmod, rem_old);
        }

        // --- VERIFICATION 2: Is current Knuth mathematically exact? ---
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

        // --- VERIFICATION 2.1: Is old Knuth mathematically exact? ---
        bui qb_old = mul_low(q_knuth_old, b);
        bui a_verify_old = add(qb_old, rem_knuth_old);
        if (cmp(a, a_verify_old) != 0) {
            std::cerr << "\n[!] MATHEMATICAL FAILURE IN OLD KNUTH ALGORITHM!\n";
            std::cerr << "A != Q * B + R\n";
            exit(1);
        }
        if (cmp(rem_knuth_old, b) >= 0) {
            std::cerr << "\n[!] MATHEMATICAL FAILURE IN OLD KNUTH ALGORITHM!\n";
            std::cerr << "Remainder is greater than or equal to Divisor!\n";
            exit(1);
        }

        // --- VERIFICATION 3: Test BUL % BUI Overloads ---
        bul a_bul = random_bul();
        bui rem_bul_old = mod_native_deprecated(a_bul, b);
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
    std::cout << "Old mod_native      : " << time_old / (NUM_TESTS * 1.0)      << " ns\n";
    std::cout << "New nmod_native     : " << time_new / (NUM_TESTS * 1.0)      << " ns  (Sliding Window)\n";
    std::cout << "Knuth divmod (new)  : " << time_knuth / (NUM_TESTS * 1.0)    << " ns\n";
    std::cout << "Knuth divmod (old)  : " << time_knuth_old / (NUM_TESTS * 1.0)<< " ns\n";
    std::cout << "Old divmod          : " << time_divmod / (NUM_TESTS * 1.0)   << " ns\n";

    if (time_knuth < time_knuth_old) {
        std::cout << "\nNew Knuth is " << (double)time_knuth_old / time_knuth << "x faster than old Knuth.\n";
    } else {
        std::cout << "\nOld Knuth is " << (double)time_knuth / time_knuth_old << "x faster than new Knuth.\n";
    }

    return 0;
}
/*
4096bit
=== PERFORMANCE BENCHMARK ===
Old mod_native      : 64955.1 ns
New nmod_native     : 54743.6 ns  (Sliding Window)
Knuth divmod (new)  : 332.953 ns
Knuth divmod (old)  : 518.64 ns
Old divmod          : 66470.4 ns

512bit
=== PERFORMANCE BENCHMARK ===
Old mod_native      : 1409.89 ns
New nmod_native     : 910.844 ns  (Sliding Window)
Knuth divmod (new)  : 81.055 ns
Knuth divmod (old)  : 104.21 ns
Old divmod          : 2017.15 ns

1024bit
=== PERFORMANCE BENCHMARK ===
Old mod_native      : 6578.68 ns
New nmod_native     : 4092.61 ns  (Sliding Window)
Knuth divmod (new)  : 118.936 ns
Knuth divmod (old)  : 158.192 ns
Old divmod          : 5761.79 ns

New Knuth is 1.33006x faster than old Knuth.

*/