void karatsuba(
    const u32* a,
    const u32* b,
    u32* r,
    u32 n,
    u32* scratch
)
{
    if (n <= cutoff) {
        mul_imp(a, b, r, n);
        return;
    }

    u32 half = n / 2;

    const u32* a1 = a;
    const u32* a0 = a + half;
    const u32* b1 = b;
    const u32* b0 = b + half;

    u32* z0 = r + n;
    u32* z2 = r;
    u32* z1 = scratch;

    u32* tmp_a = z1 + 2*half;
    u32* tmp_b = tmp_a + (half + 1);

    // z0 = a0*b0
    karatsuba(a0, b0, z0, half, scratch);

    // z2 = a1*b1
    karatsuba(a1, b1, z2, half, scratch);

    // tmp_a = a0 + a1
    u32 carry_a = add_n(a0, a1, tmp_a + 1, half);
    tmp_a[0] = carry_a;

    // tmp_b = b0 + b1
    u32 carry_b = add_n(b0, b1, tmp_b + 1, half);
    tmp_b[0] = carry_b;

    // z1 = tmp_a * tmp_b
    karatsuba(tmp_a, tmp_b, z1, half + 1, scratch);

    // z1 = z1 - z0 - z2
    sub_n(z1 + 2, z0, z1 + 2, 2*half);
    sub_n(z1 + 2, z2, z1 + 2, 2*half);

    // combine...
}
