#include <iostream>
#include <chrono>
#define BI_BIT 2048
#include "bigint.h" // Make sure to use your updated bigint.h

// Helper to generate a random base 'a' in the range [2, n-2]
bui get_random_base(const bui& n, const bui& n_minus_3) {
    bui a;
    randomize_ip(a);

    // a % (n-3) gives [0, n-4]
    // Then +2 gives [2, n-2]
    a = mod_native_deprecated(a, n_minus_3);
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
	while (has_small_factor(x) || !is_prime_miller_rabin(x, 40));
	return x;
}

int main() {
    bui t = gen_prime();
    printf("%s\n", bui_to_dec(t).c_str());
    // Your 154-digit test prime
    bui p = bui_from_dec("9862580434556848933093118044369795906452209005604134993142891065799068045921485909427627718142455707644541651618163328127809698482899632857003280134349623");

    std::cout << "Testing Modulus: " << bui_to_dec(p) << "\n\n";

    constexpr int iter = 100;
    auto start = std::chrono::high_resolution_clock::now();

    bool res;
    for (u32 i = 0; i < iter; ++i) {
        res = is_prime_miller_rabin(p, 40); // 40 rounds
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto time = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    std::cout << "Miller-Rabin Result: " << (res ? "PRIME" : "COMPOSITE") << "\n";
    std::cout << "Total Time (100x):   " << time / 1000000.0 << " ms\n";
    std::cout << "Avg Time per Check:  " << (time / 1000000.0) / iter << " ms\n";

    return 0;
}