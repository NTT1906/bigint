# bigint.h
Big unsigned interger implementation in C++17.
```c++
struct bui : std::array<u32, BI_N> {};
struct bul : std::array<u32, BI_N * 2> {};
```
Features:
- add/sub/mul/div/mod bigint
- add_mod, mul_mod, pow_mod (naive), pow_mod (montgomery - 10x naive)
- shift left, shift right, shift left mod
- bui_from_dec(): decimal string to bigint BI_BIT bit (512)
- bui_to_dec(): bigint to decimal string
- bul_to_dec(): long bigint to decimal string
- bul_to_hex(): bigint to hex string
- bul_high, bul_low: MSH and MSH of long bigint

IDK if this is big endian or little endian but if I want to assign 1 to bui, `a[BI_N - 1] = 1;` and if I want to assign `0x12345678_9ABCDEF0_11223344_55667788` then:
```c++
a[BI_N - 4] = 0x55667788u;
a[BI_N - 3] = 0x11223344u;
a[BI_N - 2] = 0x9ABCDEF0u;
a[BI_N - 1] = 0x12345678u;
```

### Example:
#### 1. Include bigint.h
You need to define either `BI_BIT` (bits) or `BI_N` (limbs) showing the size of bigint where each limb is an u32. 
```c++
#define BI_BIT 512
#include "bigint.h"
or
#define BI_N (512 / 32)
#include "bigint.h"
```
#### 2. Basic
```c++
// main.cpp
#include <iostream>
#include <iomanip>
#include <string>

// choose bigint size (512 bits in your header default)
#define BI_BIT 512
#include "bigint.h"

int main() {
    using std::cout;
    using std::endl;

    // --- create bigints from decimal strings ---
    bui A = bui_from_dec("123456789012345678901234567890");
    bui B = bui_from_dec("98765432109876543210987654321");

    cout << "A (dec) : " << bui_to_dec(A) << "\n";
    cout << "B (dec) : " << bui_to_dec(B) << "\n";
    cout << "A (hex) : " << bui_to_hex(A, true) << "\n";
    cout << "B (hex) : " << bui_to_hex(B, true) << "\n\n";

    // --- add / subtract ---
    bui sum = add(A, B);
    cout << "A + B = " << bui_to_dec(sum) << "\n";

    bui diff = A;
    // assume A > B for demonstration; if not swap
    if (cmp(diff, B) >= 0) {
        sub_ip(diff, B);
        cout << "A - B = " << bui_to_dec(diff) << "\n";
    } else {
        bui tmp = B;
        sub_ip(tmp, A);
        cout << "B - A = " << bui_to_dec(tmp) << "\n";
    }

    // --- multiplication (full 2N result and low half) ---
    bul prod;
    mul_ref(A, B, prod);                // full 2N product
    cout << "A * B (low half as dec): " << bul_to_dec(prod) << "\n";
    cout << "A * B (low half only): " << bui_to_dec(bul_low(prod)) << "\n\n";

    // --- division/modulo ---
    bui q, r;
    divmod(A, B, q, r);                 // q = A / B, r = A % B
    cout << "A / B = " << bui_to_dec(q) << ",  A % B = " << bui_to_dec(r) << "\n\n";

    // --- modular operations ---
    // choose a modulus m (must be odd for Montgomery)
    bui m = bui_from_dec("1000000000000000000000000000037"); // example prime-like modulus

    // reduce values modulo m
    bui A_mod = mod_native(A, m);
    bui B_mod = mod_native(B, m);
    cout << "A mod m = " << bui_to_dec(A_mod) << "\n";
    cout << "B mod m = " << bui_to_dec(B_mod) << "\n";

    // modular multiply (uses mul_mod_ip)
    bui C = A_mod;
    mul_mod_ip(C, B_mod, m);
    cout << "A * B mod m = " << bui_to_dec(C) << "\n\n";

    // --- modular exponentiation (naive) ---
    bui e = bui_from_dec("65537");      // common exponent
    bui naive_pow = pow_mod(A_mod, e, m);
    cout << "naive A^65537 mod m = " << bui_to_dec(naive_pow) << "\n";

    // --- Montgomery exponentiation (faster) ---
    bui mont_pow = mr_pow_mod(A_mod, e, m); // mr_pow_mod constructs a MontgomeryReducer internally
    cout << "Montgomery A^65537 mod m = " << bui_to_dec(mont_pow) << "\n\n";

    // --- modular inverse (extended gcd) ---
    bui inv;
    if (mod_inverse(A_mod, m, inv)) {
        cout << "A^{-1} mod m = " << bui_to_dec(inv) << "\n";
        // verify: (A_mod * inv) % m == 1
        bui check = A_mod;
        mul_mod_ip(check, inv, m);
        cout << "verify (A * inv) mod m = " << bui_to_dec(check) << "\n";
    } else {
        cout << "A has no inverse modulo m\n";
    }

    // --- shifts ---
    bui sh = A;
    shift_left_ip(sh, 20);              // sh <<= 20 bits
    cout << "A << 20 (dec) = " << bui_to_dec(sh) << "\n";

    bui sh_mod = shift_left_mod(A_mod, 100, m); // (A * 2^100) % m
    cout << "A * 2^100 mod m = " << bui_to_dec(sh_mod) << "\n";

    // --- convert back to hex/dec strings for display (already used above) ---
    cout << "\nDone.\n";
    return 0;
}
```