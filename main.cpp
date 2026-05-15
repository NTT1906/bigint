#include <iostream>
#define BI_BIT 64
#include "bigint.h"

int main() {
    using std::cout;
    using std::endl;

    // {
    //     // bui x = bui_from_dec("123456789");
    //     // bui e = bui_from_dec("65537");
    //     // bui m = bui_from_dec("1000000007");
    //     bui x = bui_from_dec("6949805586317758496");
    //     bui e = bui_from_dec("15425568521676317348");
    //     bui m = bui_from_dec("16551092499265970731");
    //     bui r = pow_mod_mont_cios2(x, e, m);
    //     std::cout << bui_to_dec(r) << '\n'; // 8200633687040324010
    //     return 0;
    // }

    bui tt = bui_from_dec("10850230394766384103107228565732700591998183978729776235675428711444172751337127279965246958616283758446800440987816517962513658188119312923410696014659584");
    cout << "tt = " << bui_to_dec(tt) << '\n';

    bui ta = bui_from_dec("6");
    bui tb = bui_from_dec("5");
    bui tq{}, tr{};
    divmod(ta, tb, tq, tr);
    cout << "tq = " << bui_to_dec(tq) << ", tr = " << bui_to_dec(tr) << '\n';

    // --- create bigints from decimal strings ---
    bui A = bui_from_dec("123456789012345678901234567890");
    // bui B = bui_from_dec("98765432109876543210987654321");
    bui B = bui_from_hex("0x00000008FFFFFFFFFFFFFFFFFFFFFFFF");
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
    // A = bui_from_u32(2);
    // B = bui_from_u32(3);
    bui prod_low = mul_low_fast(A, B);
    cout << "A * B (narrowing): " << bui_to_dec(prod_low) << "\n";

    // --- division/modulo ---
    bui q, r;
    divmod(A, B, q, r);                 // q = A / B, r = A % B
    cout << "A / B = " << bui_to_dec(q) << ",  A % B = " << bui_to_dec(r) << "\n\n";

    // --- modular operations ---
    // choose a modulus m (must be odd for Montgomery)
    bui m = bui_from_dec("1000000000000000000000000000037"); // example prime-like modulus

    // reduce values modulo m
    bui A_mod = mod(A, m);
    bui B_mod = mod(B, m);
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
    auto t0 = std::chrono::steady_clock::now();
    bui mont_pow = mr_pow_mod(A_mod, e, m); // mr_pow_mod constructs a MontgomeryReducer internally
    auto t1 = std::chrono::steady_clock::now();
    auto dur = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::cout << "D: " << dur << "\n";
    cout << "Montgomery A^65537 mod m = " << bui_to_dec(mont_pow) << "\n\n";
    // --- Montgomery 2 exponentiation (faster) ---
    t0 = std::chrono::steady_clock::now();
    bui mont2_pow = mr_cios_pow_mod(A_mod, e, m); // mr_pow_mod constructs a MontgomeryReducer internally
    t1 = std::chrono::steady_clock::now();
    dur = std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
    std::cout << "D: " << dur << "\n";
    cout << "Montgomery A^65537 mod m = " << bui_to_dec(mont2_pow) << "\n\n";

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

    bui f1 = bui_binary_flood1(33);
    cout << "f1 = " << bui_to_dec(f1) << "\n";
    bul f2 = bul_binary_flood1(721);
    cout << "f2 = " << bul_to_dec(f2) << "\n";

    // --- convert back to hex/dec strings for display (already used above) ---
    cout << "\nDone.\n";
    return 0;
}
