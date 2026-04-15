#include <iostream>
#define BI_BIT 512
#include "bigint.h"

constexpr u32 POLY_R = 14;
struct Poly : std::array<bui, POLY_R>{};

void printBuiA(const bui *poly, int n) {
	printf("{");
	for (int i = 0; i < n; ++i) {
		printf("%s, ", bui_to_dec(poly[i]).c_str());
	}
	printf("}\n");
}

static void poly_mul_mod_ip(Poly &A, const Poly &B, const bui& m) {
	bool a_skips[POLY_R] = {false};
	bool b_skips[POLY_R] = {false};
	for (int i = 0; i < POLY_R; i++) {
		a_skips[i] = bui_is0(A[i]);
		b_skips[i] = bui_is0(B[i]);
	}
	Poly C{};
	for (int i = 0; i < POLY_R; ++i) {
		if (a_skips[i]) continue;
		for (int j = 0; j < POLY_R; ++j) {
			if (b_skips[j]) continue;
			bui p = A[i];
			mul_mod_ip(p, B[j], m);
			add_mod_ip(C[(i + j) % POLY_R], p, m);
		}
	}
	A = C;
}

static void poly_sqr_mod_ip(Poly &A, const bui& m) {
	bool a_skips[POLY_R] = {false};
	for (int i = 0; i < POLY_R; i++) {
		a_skips[i] = bui_is0(A[i]);
	}
	Poly C{};
	for (int i = 0; i < POLY_R; ++i) {
		if (a_skips[i]) continue;
		for (int j = 0; j < POLY_R; ++j) {
			if (a_skips[j]) continue;
			bui p = A[i];
			mul_mod_ip(p, A[j], m);
			add_mod_ip(C[(i + j) % POLY_R], p, m);
		}
	}
	A = C;
}

Poly poly_pow_1x(const bui &n) {
	Poly base{};
	base[0] = bui1(); base[1] = bui1();
	Poly res{};
	res[0] = bui1();
	u32 hb = highest_bit(n);
	for (u32 i = 0; i < hb; ++i) {
		if (get_bit(n, i)) {
			poly_mul_mod_ip(res, base, n);
			printf("%5d: R1: ", i);
			printBuiA(res.data(), res.size());
		}
		poly_sqr_mod_ip(base, n);
		printf("%5d: B1: ", i);
		printBuiA(base.data(), base.size());
	}
	return res;
}

// a = (a + b) % m
inline void add_true_mod_ip(bui &a, bui b, const bui &m) {
	if (add_ip_n_imp(a.data(), b.data(), BI_N) || cmp(a, m) >= 0) {
		sub_ip(a, m);
	}
}

static void poly_mul_mod_mont_ip(Poly &A, const Poly &B, const MontgomeryReducer &mr) {
	bool a_skips[POLY_R] = {false};
	bool b_skips[POLY_R] = {false};
	for (int i = 0; i < POLY_R; ++i) {
		a_skips[i] = bui_is0(A[i]);
		b_skips[i] = bui_is0(B[i]);
	}
	Poly C{};
	for (int i = 0; i < POLY_R; ++i) {
		if (a_skips[i]) continue;
		for (int j = 0; j < POLY_R; ++j) {
			if (b_skips[j]) continue;
			bui p = mr.multiply(A[i], B[j]);
			add_true_mod_ip(C[(i + j) % POLY_R], p, mr.modulus);
		}
	}
	A = C;
}

static void poly_sqr_mod_mont_ip(Poly &A, const MontgomeryReducer &mr) {
	bool a_skips[POLY_R] = {false};
	for (int i = 0; i < POLY_R; ++i) a_skips[i] = bui_is0(A[i]);
	Poly C{};
	for (int i = 0; i < POLY_R; ++i) {
		if (a_skips[i]) continue;
		for (int j = 0; j < POLY_R; ++j) {
			if (a_skips[j]) continue;
			bui p = mr.multiply(A[i], A[j]);
			add_true_mod_ip(C[(i + j) % POLY_R], p, mr.modulus);
		}
	}
	A = C;
}

void printOg(Poly p, const MontgomeryReducer& mr) {
	for (int i = 0; i < POLY_R; ++i) {
		if (!bui_is0(p[i])) p[i] = mr.convertOut(p[i]);
	}
	printBuiA(p.data(), p.size());
}

Poly poly_pow_1x_mont(const bui &n) {
	MontgomeryReducer mr(n);
	Poly base{}; base[0] = mr.convertedOne; base[1] = mr.convertedOne;
	Poly res{};  res[0] = mr.convertedOne;

	u32 hb = highest_bit(n);
	for (u32 i = 0; i < hb; ++i) {
		if (get_bit(n, i)) {
			poly_mul_mod_mont_ip(res, base, mr);
		}
		poly_sqr_mod_mont_ip(base, mr);
	}
	// convert result coefficients back to standard form
	for (int i = 0; i < POLY_R; ++i) {
		if (!bui_is0(res[i])) res[i] = mr.convertOut(res[i]);
	}
	return res;
}

// TODO: gate-away small number?
static bool aks_like_prime(const bui &n) {
	if (!get_bit(n, 0)) return false;
	Poly p = poly_pow_1x_mont(n);
	bui b1 = bui1();
	if (cmp(p[0], b1) != 0) return false;
	u32 k;
	bui q;
	u32divmod(n, POLY_R, q,  k);
	if (cmp(p[k], b1) != 0) return false;
	for (u32 i = 1; i < POLY_R; ++i) {
		if (i == k) continue;
		if (!bui_is0(p[i])) return false;
	}
	return true;
}

#include <chrono>

// --- MONTGOMERY 2 POLYNOMIAL PIPELINE ---

static void poly_mul_mod_mont2_ip(Poly &A, const Poly &B, const MontgomeryReducer2 &mr) {
    bool a_skips[POLY_R] = {false};
    bool b_skips[POLY_R] = {false};
    for (int i = 0; i < POLY_R; ++i) {
        a_skips[i] = bui_is0(A[i]);
        b_skips[i] = bui_is0(B[i]);
    }
    Poly C{};
    for (int i = 0; i < POLY_R; ++i) {
        if (a_skips[i]) continue;
        for (int j = 0; j < POLY_R; ++j) {
            if (b_skips[j]) continue;
            bui p = mr.multiply(A[i], B[j]);
            add_true_mod_ip(C[(i + j) % POLY_R], p, mr.modulus);
        }
    }
    A = C;
}

static void poly_sqr_mod_mont2_ip(Poly &A, const MontgomeryReducer2 &mr) {
    bool a_skips[POLY_R] = {false};
    for (int i = 0; i < POLY_R; ++i) a_skips[i] = bui_is0(A[i]);
    Poly C{};
    for (int i = 0; i < POLY_R; ++i) {
        if (a_skips[i]) continue;
        for (int j = 0; j < POLY_R; ++j) {
            if (a_skips[j]) continue;
            bui p = mr.multiply(A[i], A[j]);
            add_true_mod_ip(C[(i + j) % POLY_R], p, mr.modulus);
        }
    }
    A = C;
}

Poly poly_pow_1x_mont2(const bui &n) {
    MontgomeryReducer2 mr(n);
    Poly base{}; base[0] = mr.convertedOne; base[1] = mr.convertedOne;
    Poly res{};  res[0] = mr.convertedOne;

    u32 hb = highest_bit(n);
    for (u32 i = 0; i < hb; ++i) {
        if (get_bit(n, i)) {
            poly_mul_mod_mont2_ip(res, base, mr);
        }
        poly_sqr_mod_mont2_ip(base, mr);
    }

    for (int i = 0; i < POLY_R; ++i) {
        if (!bui_is0(res[i])) res[i] = mr.convertOut(res[i]);
    }
    return res;
}

static bool aks_like_prime2(const bui &n) {
    if (!get_bit(n, 0)) return false;
    Poly p = poly_pow_1x_mont2(n);
    bui b1 = bui1();
    if (cmp(p[0], b1) != 0) return false;
    u32 k;
    bui q;
    u32divmod(n, POLY_R, q,  k);
    if (cmp(p[k], b1) != 0) return false;
    for (u32 i = 1; i < POLY_R; ++i) {
        if (i == k) continue;
        if (!bui_is0(p[i])) return false;
    }
    return true;
}

static bool has_small_factor(const bui &n) {
	static const int SMALL_PRIMES[] = {
		2, 3, 5, 7,11,13,17,19,23,29,31,37,41,
	   43,47,53,59,61,67,71,73,79,83,89,97
   };
	for (int p : SMALL_PRIMES) {
		u32 r = 0; bui tmp;
		u32divmod(n, (u32)p, tmp, r);
		if (r == 0) return cmp(n, bui_from_u32((u32)p)) != 0;
	}
	return false;
}

static bui gen_prime() {
	bui x;
	do {
		x = random_odd();
		// printf("Testing: %s\n", bui_to_dec(x).c_str());
	}
	while (has_small_factor(x) || !aks_like_prime(x));
	return x;
}

// Precomputed list of small primes (you can expand this up to the first 200-300 primes)
static const u32 SMALL_PRIMES[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
    73, 79, 83, 89, 97, 101, 103, 107, 109, 113, 127, 131, 137, 139, 149, 151,
    157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233,
    239, 241, 251, 257, 263, 269, 271, 277, 281, 283, 293, 307, 311, 313, 317,
    331, 337, 347, 349, 353, 359, 367, 373, 379, 383, 389, 397, 401, 409, 419,
    421, 431, 433, 439, 443, 449, 457, 461, 463, 467, 479, 487, 491, 499, 503,
    509, 521, 523, 541, 547, 557, 563, 569, 571, 577, 587, 593, 599, 601, 607,
    613, 617, 619, 631, 641, 643, 647, 653, 659, 661, 673, 677, 683, 691, 701,
    709, 719, 727, 733, 739, 743, 751
};
constexpr u32 NUM_SMALL_PRIMES = sizeof(SMALL_PRIMES) / sizeof(SMALL_PRIMES[0]);

// Helper to generate a random base 'a' in the range [2, n-2]
bui get_random_base(const bui& n, const bui& n_minus_3) {
	bui a;
	randomize_ip(a);

	// a % (n-3) gives [0, n-4]
	// Then +2 gives [2, n-2]
	a = mod_native(a, n_minus_3);
	add_ip(a, bui_from_u32(2));

	return a;
}

bool is_prime_miller_rabin(const bui& n, int k = 40) {
	// 1. Handle base cases
	if (bui_is0(n) || cmp(n, bui1()) == 0) return false;

	bui b2 = bui_from_u32(2);
	bui b3 = bui_from_u32(3);
	if (cmp(n, b2) == 0 || cmp(n, b3) == 0) return true;

	// If even, it's composite
	if (!get_bit(n, 0)) return false;

	// 2. Find d and s such that n - 1 = d * 2^s
	bui n_minus_1 = sub(n, bui1());
	bui n_minus_3 = sub(n, b3);

	bui d = n_minus_1;
	u32 s = 0;

	// Shift right until d is odd
	while (!get_bit(d, 0)) {
		shift_right_ip(d, 1);
		s++;
	}

	// 3. Initialize Montgomery Reducer exactly ONCE
	MontgomeryReducer2 mr(n);
	bui mont_one = mr.convertedOne;
	bui mont_n_minus_1 = mr.convertIn(n_minus_1);

	// 4. Witness Loop
	for (int i = 0; i < k; ++i) {
		bui a = get_random_base(n, n_minus_3);

		// Convert base 'a' into Montgomery form
		bui x = mr.convertIn(a);

		// x = a^d mod n (Computed entirely in Montgomery space)
		x = mr.pow(x, d);

		if (cmp(x, mont_one) == 0 || cmp(x, mont_n_minus_1) == 0) {
			continue; // a is a strong liar, pass this round
		}

		bool composite = true;
		for (u32 r = 1; r < s; ++r) {
			// x = x^2 mod n (Using blazing fast Montgomery multiplication!)
			x = mr.multiply(x, x);

			if (cmp(x, mont_n_minus_1) == 0) {
				composite = false; // Pass this round
				break;
			}
		}

		// If we reach here and composite is true, we found a witness!
		if (composite) {
			return false;
		}
	}

	return true; // Probably prime
}

bui gen_prime_sieve() {
    // We cap delta so we don't drift too far from true randomness
    // 0x100000 (roughly 1 million) is plenty of space to find a prime.
    const u32 MAX_DELTA = 0x100000;

    u32 mods[NUM_SMALL_PRIMES];

    while (true) {
        bui base = random_odd();
        // 2. Precalculate the remainders for this base
        // This is the ONLY time we do heavy BigInt division for small primes
        for (u32 i = 0; i < NUM_SMALL_PRIMES; ++i) {
            bui tmp;
            u32 r;
            // Assuming your u32divmod sets 'r' to (base % SMALL_PRIMES[i])
            u32divmod(base, SMALL_PRIMES[i], tmp, r);
            mods[i] = r;
        }

        // 3. The Quick Sieve Loop
        for (u32 delta = 0; delta < MAX_DELTA; delta += 2) {
            bool has_small_factor = false;

            // Check against all small primes using only 32-bit integer math!
            // Notice we start at i=1 (skip 2) because we know delta is even and base is odd.
            for (u32 i = 1; i < NUM_SMALL_PRIMES; ++i) {
                // The magic of Zimmermann's Sieve:
                if ((mods[i] + delta) % SMALL_PRIMES[i] == 0) {
                    has_small_factor = true;
                    break;
                }
            }

            // 4. If no small factors were found, we do the heavy test
            if (!has_small_factor) {
                bui candidate = base;

                // Add the winning delta to the base
                if (delta > 0) {
                    bui b_delta = bui_from_u32(delta);
                    add_ip(candidate, b_delta);
                }

                // If addition overflowed our bit limit, break and get a new base
                if (highest_bit(candidate) >= BI_BIT) {
                    break;
                }

            	// This will instantly weed out 99.99% of composites in milliseconds.
            	if (!is_prime_miller_rabin(candidate, 1)) {
            		continue; // Failed fast, try the next delta!
            	}

                // 5. Heavy Cryptographic Test (Miller-Rabin or your AKS)
                // Using 5 rounds for 1024+ bits, 40 rounds for <=512 bits
                if (aks_like_prime(candidate)) {
                    return candidate; // We found a prime!
                }
            }
        }
        // If we exhausted MAX_DELTA, the while(true) loop regenerates a new base and tries again.
    }
}

// --- BENCHMARK RUNNER ---

int main(int argc, char* argv[]) {
	bui t = gen_prime_sieve();
    printf("%s\n", bui_to_dec(t).c_str());
    // Standard test prime
    bui p = bui_from_dec("9862580434556848933093118044369795906452209005604134993142891065799068045921485909427627718142455707644541651618163328127809698482899632857003280134349623");
	// bui p = bui_from_dec("28942166494538928974351983448635246885495327598172021583158584507257207568312095228495916674334593973983156969423856360559966119233620790580881778750709815660800275386608528992118647689526009538128045762968748236192956963035921885865824655005361421203201014295495270310092437549451048332402656437075333666129261243001739478281702025725970619483575410702219761489204572394739664313491979903876113458594991510602653485732153435361992572543457367170517219693438104611344760782402312215424645784392557277234590777897630250758270544364136317573820743608295058535632229198310872108416627690696078175352883672088374221700481");

    std::cout << "Testing Modulus: " << bui_to_dec(p) << "\n\n";

    // --- Benchmark Original Montgomery ---
    std::cout << "Running Original Montgomery...\n";
	constexpr int iter = 5;
	auto start1 = std::chrono::high_resolution_clock::now();
	bool res1;
	for (u32 i = 0; i < iter; ++i)
	    res1 = aks_like_prime(p);
    auto end1 = std::chrono::high_resolution_clock::now();
    auto time1 = std::chrono::duration_cast<std::chrono::nanoseconds>(end1 - start1).count();

    // --- Benchmark Montgomery 2 ---
    std::cout << "Running Montgomery2...\n";
    auto start2 = std::chrono::high_resolution_clock::now();
    bool res2;
	for (u32 i = 0; i < iter; ++i)
		res2 = aks_like_prime2(p);
    auto end2 = std::chrono::high_resolution_clock::now();
    auto time2 = std::chrono::duration_cast<std::chrono::nanoseconds>(end2 - start2).count();

    // --- Results ---
    std::cout << "\n=== BENCHMARK RESULTS ===\n";
    std::cout << "Original AKS Result: " << (res1 ? "PRIME" : "COMPOSITE") << "\n";
    std::cout << "Original Time:       " << time1 << " ns (" << time1 / iter << " ns)\n\n";

    std::cout << "Mont 2 AKS Result:   " << (res2 ? "PRIME" : "COMPOSITE") << "\n";
    std::cout << "Mont 2 Time:         " << time2 << " ns (" << time2 / iter << " ns)\n\n";

    if (time2 < time1) {
        double speedup = (double)time1 / time2;
        std::cout << "Montgomery2 is " << speedup << "x FASTER!\n";
    } else {
        double speedup = (double)time2 / time1;
        std::cout << "Montgomery2 is " << speedup << "x SLOWER. (Check your compiler flags!)\n";
    }

    return 0;
}

// D:\code\clion\rsa\bigint\prime.exe
// 380898217734376977388469905095631688232819127245330094377467101779790842323034086401759539159320231483166447594777807909
// 602443802746428993648333477118469
// Testing Modulus: 9862580434556848933093118044369795906452209005604134993142891065799068045921485909427627718142455707644
// 541651618163328127809698482899632857003280134349623
//
// Running Original Montgomery...
// Running Montgomery2...
//
// === BENCHMARK RESULTS ===
// Original AKS Result: PRIME
// Original Time:       293951400 ns (58790280 ns)
//
// Mont 2 AKS Result:   PRIME
// Mont 2 Time:         271057100 ns (54211420 ns)