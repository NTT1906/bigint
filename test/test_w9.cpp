#define BI_BIT 256
#include <iostream>
using namespace std;
#include "../bigint.h"

bui gcd(bui a, bui b) {
	while (!bui_is0(b)) {
		bui r = mod_native(a, b);
		a = b;
		b = r;
	}
	return a;
}

int main() {
	bui p = bui_from_dec("877874183433764619725116985867");
	bui q = bui_from_dec("289934651451236989763726480387");

	bui n = p;
	mul_ip(n, q);  // n = p*q

	bui p1 = p, q1 = q;
	sub_ip(p1, bui1());   // p-1
	sub_ip(q1, bui1());   // q-1

	bui phi = p1;
	mul_ip(phi, q1);      // phi = (p-1)*(q-1)

	// choose e
	bui e;
	do {
		randomize_ip(e);
		e = mod_native(e, phi);
	} while (cmp(e, bui1()) <= 0 || cmp(gcd(e, phi), bui1()) != 0);

	// compute d = e^{-1} mod phi
	bui d;
	if (!mod_inverse(e, phi, d)) {
		cout << "No inverse found\n";
		return 0;
	}

	bui m = bui_from_dec("89895324328489433311101010346");
	bui s = mr_pow_mod(m, d, n);
	bui m_r = mr_pow_mod(s, e, n);

	// ===== Output =====
	cout << "s  : " << bui_to_dec(s) << endl;
	cout << "m_r: " << bui_to_dec(m_r) << endl;
	cout << "m  : " << bui_to_dec(m) << endl;

	return 0;
}