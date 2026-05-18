#include <cassert>
#include <iostream>
#include <random>

#define BI_BIT 512
#include "../bigint.h"

static bui random_odd_modulus(std::mt19937& gen) {
	std::uniform_int_distribution<u32> dist(0, 0xffffffffu);
	bui m{};
	for (auto& limb : m)
		limb = dist(gen);
	m[0] &= 0x3fffffffu;
	m[0] |= 1u << 29;
	m[BI_N - 1] |= 1u;
	if (cmp(m, bui_from_u32(3)) < 0)
		m = bui_from_u32(3);
	return m;
}

static bui random_reduced(const bui& m, std::mt19937& gen) {
	std::uniform_int_distribution<u32> dist(0, 0xffffffffu);
	bui x{};
	for (auto& limb : x)
		limb = dist(gen);
	return mod(x, m);
}

int main() {
	std::mt19937 gen(123456);

	for (int iter = 0; iter < 200; ++iter) {
		bui m = random_odd_modulus(gen);
		MontgomeryReducerCIOS3 mr(m);

		for (int sample = 0; sample < 50; ++sample) {
			bui x = random_reduced(m, gen);
			bui mx = mr.to_mont(x);

			bui sqr_mont = mr.sqr(mx);
			bui mul_mont = mr.mul(mx, mx);
			assert(cmp(sqr_mont, mul_mont) == 0);

			bui sqr_std = mr.from_mont(sqr_mont);
			bui ref = mod(::sqr(x), m);
			assert(cmp(sqr_std, ref) == 0);
		}
	}

	std::cout << "MontgomeryReducerCIOS3::sqr OK\n";
	return 0;
}
