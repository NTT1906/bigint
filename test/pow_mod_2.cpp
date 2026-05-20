#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>
#include <string>
#include <cstring>
#include <cmath>

#ifndef BI_BIT
#define BI_BLEN (64)
#endif
#ifndef DATASET_SIZE
#define DATASET_SIZE 5
#endif
#ifndef BENCH_ITERATIONS
#define BENCH_ITERATIONS 10
#endif
#include "../bigint.h"

volatile uw global_sink = 0;

struct PowCase {
	const char* name;
	std::vector<bui> base;
	std::vector<bui> exp;
	std::vector<bui> mod;
};

static bui random_odd_bui(std::mt19937& gen) {
	std::uniform_int_distribution<uw> dist(0, 0xffffffffu);
	bui x{};
	for (auto& limb : x)
		limb = dist(gen);
	x[BI_LEN - 1] |= 1;
	return x;
}

static bui random_bui(std::mt19937& gen) {
	std::uniform_int_distribution<uw> dist(0, 0xffffffffu);
	bui x{};
	for (auto& limb : x)
		limb = dist(gen);
	return x;
}

static void force_bit_size(bui& x, uw bits, std::mt19937& gen) {
	if (bits == 0) { x = {}; return; }
	if (bits > BI_BLEN) bits = BI_BLEN;
	uw nlimbs = (bits + 31) / 32;
	uw zero_limbs = BI_LEN - nlimbs;
	for (uw i = 0; i < zero_limbs; ++i)
		x[i] = 0;
	x[zero_limbs] |= 1u << 31;
	for (uw i = zero_limbs + 1; i < BI_LEN; ++i)
		if (i != BI_LEN - 1 || !(x[BI_LEN - 1] & 1))
			x[i] = std::uniform_int_distribution<uw>(0, 0xffffffffu)(gen);
	x[BI_LEN - 1] |= 1;
}

static void make_nonzero(bui& x) {
	if (bui_is0(x))
		x = bui_from_u32(1);
}

static PowCase make_case(const char* name, int size) {
	PowCase c{name};
	c.base.resize(size);
	c.exp.resize(size);
	c.mod.resize(size);
	return c;
}

static void fill_case(PowCase& c, std::mt19937& gen) {
	for (size_t i = 0; i < c.base.size(); ++i) {
		c.mod[i] = random_odd_bui(gen);
		c.base[i] = random_bui(gen);
		while (cmp(c.base[i], c.mod[i]) >= 0)
			c.base[i] = random_bui(gen);
		c.exp[i] = random_bui(gen);
		make_nonzero(c.exp[i]);

		if (std::strcmp(c.name, "exp_256bit") == 0) {
			c.mod[i] = random_odd_bui(gen);
			c.base[i] = random_bui(gen);
			while (cmp(c.base[i], c.mod[i]) >= 0)
				c.base[i] = random_bui(gen);
			force_bit_size(c.exp[i], 256, gen);
		} else if (std::strcmp(c.name, "exp_512bit") == 0) {
			c.mod[i] = random_odd_bui(gen);
			c.base[i] = random_bui(gen);
			while (cmp(c.base[i], c.mod[i]) >= 0)
				c.base[i] = random_bui(gen);
			force_bit_size(c.exp[i], 512, gen);
		} else if (std::strcmp(c.name, "exp_1024bit") == 0) {
			c.mod[i] = random_odd_bui(gen);
			c.base[i] = random_bui(gen);
			while (cmp(c.base[i], c.mod[i]) >= 0)
				c.base[i] = random_bui(gen);
			force_bit_size(c.exp[i], 1024, gen);
		} else if (std::strcmp(c.name, "exp_2048bit") == 0) {
			c.mod[i] = random_odd_bui(gen);
			c.base[i] = random_bui(gen);
			while (cmp(c.base[i], c.mod[i]) >= 0)
				c.base[i] = random_bui(gen);
			force_bit_size(c.exp[i], 2048, gen);
		} else if (std::strcmp(c.name, "exp_3072bit") == 0) {
			c.mod[i] = random_odd_bui(gen);
			c.base[i] = random_bui(gen);
			while (cmp(c.base[i], c.mod[i]) >= 0)
				c.base[i] = random_bui(gen);
			force_bit_size(c.exp[i], 3072, gen);
		} else if (std::strcmp(c.name, "mod_2048bit") == 0) {
			force_bit_size(c.mod[i], 2048, gen);
			c.base[i] = random_bui(gen);
			force_bit_size(c.base[i], 2048, gen); // Constrain base to 2048 bits

			while (cmp(c.base[i], c.mod[i]) >= 0) {
				c.base[i] = random_bui(gen);
				force_bit_size(c.base[i], 2048, gen); // Constrain again on retry
			}
			c.exp[i] = random_bui(gen);
			make_nonzero(c.exp[i]);

		} else if (std::strcmp(c.name, "mod_1024bit") == 0) {
			force_bit_size(c.mod[i], 1024, gen);
			c.base[i] = random_bui(gen);
			force_bit_size(c.base[i], 1024, gen); // Constrain base to 1024 bits

			while (cmp(c.base[i], c.mod[i]) >= 0) {
				c.base[i] = random_bui(gen);
				force_bit_size(c.base[i], 1024, gen); // Constrain again on retry
			}
			c.exp[i] = random_bui(gen);
			make_nonzero(c.exp[i]);
		}
	}
}

// Validate all implementations produce the same result
static void validate_case(PowCase& c, const std::vector<bui>& ref) {
	for (size_t i = 0; i < c.base.size(); ++i) {
		bui r0 = ref[i];

		// Helper lambda to check results and print to std::cerr on failure
		auto check_result = [&](const bui& test_val, const char* method_name) {
			if (cmp(r0, test_val) != 0) {
				std::cerr << "\n[!] VALIDATION FAILED in category: " << c.name << " at index " << i << "\n";
				std::cerr << "Method:   " << method_name << " failed against pow_mod (ref)\n";
				std::cerr << "Base (x): " << bui_to_dec(c.base[i]) << "\n";
				std::cerr << "Exp (e):  " << bui_to_dec(c.exp[i]) << "\n";
				std::cerr << "Mod (m):  " << bui_to_dec(c.mod[i]) << "\n";
				std::cerr << "Ref (y):  " << bui_to_dec(r0) << "\n";
				std::cerr << "Got:      " << bui_to_dec(test_val) << "\n";
				std::exit(1);
			}
		};

		bui r1 = pow_mod(c.base[i], c.exp[i], c.mod[i]);
		check_result(r1, "pow_mod2"); // Sanity check

		bui r3 = pow_mod_window(c.base[i], c.exp[i], c.mod[i]);
		check_result(r3, "pow_mod_window");

		bui r4 = pow_mod_mont_cios2(c.base[i], c.exp[i], c.mod[i]);
		check_result(r4, "pow_mod_mont_cios2");

		bui r5 = pow_mod_mont_cios3(c.base[i], c.exp[i], c.mod[i]);
		check_result(r5, "pow_mod_mont_cios3");

		bui r6 = pow_mod_mont_window(c.base[i], c.exp[i], c.mod[i]);
		check_result(r6, "pow_mod_mont_window");
	}
}

template <typename Fn>
double bench_pow(Fn fn, const std::vector<bui>& base, const std::vector<bui>& exp, const std::vector<bui>& mod, std::vector<bui>& out, int iterations) {
	const double total_calls = (double)base.size() * iterations;
	auto start = std::chrono::high_resolution_clock::now();
	for (int iter = 0; iter < iterations; ++iter)
		for (size_t i = 0; i < base.size(); ++i)
			out[i] = fn(base[i], exp[i], mod[i]);
	auto end = std::chrono::high_resolution_clock::now();

	global_sink ^= out[0][BI_LEN - 1];
	return std::chrono::duration<double, std::nano>(end - start).count() / total_calls;
}

int main() {
	std::cout << "POW_MOD CATEGORY BENCHMARK REPORT (BI_BIT = " << BI_BLEN << ")\n";

	std::vector<PowCase> cases;
	cases.push_back(make_case("random_full", DATASET_SIZE));
	cases.push_back(make_case("exp_256bit", DATASET_SIZE));
	cases.push_back(make_case("exp_512bit", DATASET_SIZE));
	cases.push_back(make_case("exp_1024bit", DATASET_SIZE));
	cases.push_back(make_case("exp_2048bit", DATASET_SIZE));
	cases.push_back(make_case("exp_3072bit", DATASET_SIZE));
	cases.push_back(make_case("mod_1024bit", DATASET_SIZE));
	cases.push_back(make_case("mod_2048bit", DATASET_SIZE));

	std::mt19937 gen(123456);
	std::cout << "[+] Generating " << DATASET_SIZE << " test objects per category...\n";
	for (auto& c : cases)
		fill_case(c, gen);

	std::cout << "[+] Validating (pow_mod as reference)...\n";
	std::vector<bui> ref(DATASET_SIZE);
	for (auto& c : cases) {
		for (size_t i = 0; i < c.base.size(); ++i)
			ref[i] = pow_mod(c.base[i], c.exp[i], c.mod[i]);
		validate_case(c, ref);
	}

	std::cout << "[+] Benching " << BENCH_ITERATIONS << "x" << DATASET_SIZE << " per category...\n";

	constexpr int N = 7;
	const char* names[N] = {
		"pow_mod",
		"pow_mod2",
		"pow_mod_window",
		"mr_pow_mod",
		"pow_mod_mont_cios2",
		"pow_mod_mont_cios3",
		"pow_mod_mont_window"
	};

	auto fns = std::vector<std::function<bui(const bui&, const bui&, const bui&)>>{
		pow_mod,
		pow_mod2,
		pow_mod_window,
		mr_pow_mod,
		pow_mod_mont_cios2,
		pow_mod_mont_cios3,
		pow_mod_mont_window
	};

	std::vector sums(N, 0.0);
	std::vector tmp(N, std::vector<bui>(DATASET_SIZE));
	std::vector all_results(cases.size(), std::vector<double>(N));

	std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------------\n";
	std::cout << std::left << std::setw(18) << "Case";
	for (auto & name : names)
		std::cout << std::setw(24) << name;
	std::cout << "\n";
	std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------------\n";
	std::cout << std::fixed << std::setprecision(3);

	for (size_t ci = 0; ci < cases.size(); ++ci) {
		auto& c = cases[ci];
		std::cout << std::left << std::setw(18) << c.name;

		for (int j = 0; j < N; ++j) {
			double ns = bench_pow(fns[j], c.base, c.exp, c.mod, tmp[j], BENCH_ITERATIONS);
			all_results[ci][j] = ns;
			sums[j] += ns;
			std::cout << std::setw(24) << ns;
		}
		std::cout << "\n";
	}

	std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------------\n";
	std::cout << std::left << std::setw(18) << "AVG";
	for (int j = 0; j < N; ++j)
		std::cout << std::setw(24) << (sums[j] / cases.size());
	std::cout << "\n";
	std::cout << "-------------------------------------------------------------------------------------------------------------------------------------------------------------\n";


	std::cout << "\n\nSPEEDUP MATRIX (Row vs Column)\n";
	std::cout << "Read as: 'Row method is X times faster than Column method' (Values > 1 mean Row is faster)\n";
	std::cout << std::string(161, '-') << "\n";

	// Print column headers
	std::cout << std::left << std::setw(22) << "Row \\ Col";
	for (int j = 0; j < N; ++j) {
		std::cout << std::setw(24) << names[j];
	}
	std::cout << "\n";
	std::cout << std::string(161, '-') << "\n";

	// Print matrix body
	for (int i = 0; i < N; ++i) {
		std::cout << std::left << std::setw(22) << names[i];
		for (int j = 0; j < N; ++j) {
			// Speedup = Time_Col / Time_Row
			// Using sums[j] / sums[i] directly calculates the ratio across all benchmarks
			double speedup = (sums[i] > 0.0) ? (sums[j] / sums[i]) : 0.0;

			// Format to 2 decimal places (e.g., 1.00x)
			std::cout << std::setw(24) << std::fixed << std::setprecision(2) << speedup;
		}
		std::cout << "\n";
	}
	std::cout << std::string(161, '-') << "\n\n";
	return 0;
}

/*
PS D:\code\clion\rsa\bigint> g++ -std=c++17 -DNDEBUG -DBI_BIT=4096 -O3 D:\code\clion\rsa\bigint\test\pow_mod_2.cpp -o pow_mod2.exe | .\pow_mod2.exe
POW_MOD CATEGORY BENCHMARK REPORT (BI_BIT = 4096)
[+] Generating 5 test objects per category...
[+] Validating (pow_mod as reference)...
[+] Benching 10x5 per category...
-------------------------------------------------------------------------------------------------------------------------------------------------------------
Case              pow_mod                 pow_mod2                pow_mod_window          pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
PS D:\code\clion\rsa\bigint> ^C
PS D:\code\clion\rsa\bigint> g++ -std=c++17 -DNDEBUG -DBI_BIT=4096 -O3 D:\code\clion\rsa\bigint\test\pow_mod_2.cpp -o pow_mod2.exe | .\pow_mod2.exe
POW_MOD CATEGORY BENCHMARK REPORT (BI_BIT = 4096)
[+] Generating 5 test objects per category...
[+] Validating (pow_mod as reference)...
[+] Benching 10x5 per category...
-------------------------------------------------------------------------------------------------------------------------------------------------------------
Case              pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-------------------------------------------------------------------------------------------------------------------------------------------------------------
random_full       163356588.000           139238440.000           105441574.000           175372990.000           139551086.000           86821944.000            67381874.000
exp_256bit        10012522.000            8440504.000             6942568.000             12626518.000            8914936.000             5873418.000             4865576.000
exp_512bit        20597464.000            17096092.000            13708774.000            23590972.000            17410064.000            11412036.000            9168024.000
exp_1024bit       40868004.000            34646128.000            26829144.000            44962468.000            35025652.000            21707618.000            17529528.000
exp_2048bit       81444236.000            69434576.000            53119740.000            87965294.000            70288664.000            43558802.000            34275266.000
exp_3072bit       124019700.000           103934612.000           78957972.000            131454240.000           104570186.000           65904570.000            50755280.000
mod_1024bit       28059914.000            24714274.000            19996360.000            52349874.000            140212066.000           86476488.000            67390572.000
mod_2048bit       62901960.000            54598578.000            42671614.000            96182784.000            138526010.000           86404690.000            66867642.000
-------------------------------------------------------------------------------------------------------------------------------------------------------------
AVG               66407548.500            56512900.500            43458468.250            78063142.500            81812333.000            51019945.750            39779220.250
-------------------------------------------------------------------------------------------------------------------------------------------------------------


SPEEDUP MATRIX (Row vs Column)
Read as: 'Row method is X times faster than Column method' (Values > 1 mean Row is faster)
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
Row \ Col             pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
pow_mod               1.00                    0.85                    0.65                    1.18                    1.23                    0.77                    0.60
pow_mod2              1.18                    1.00                    0.77                    1.38                    1.45                    0.90                    0.70
pow_mod_window        1.53                    1.30                    1.00                    1.80                    1.88                    1.17                    0.92
mr_pow_mod            0.85                    0.72                    0.56                    1.00                    1.05                    0.65                    0.51
pow_mod_mont_cios2    0.81                    0.69                    0.53                    0.95                    1.00                    0.62                    0.49
pow_mod_mont_cios3    1.30                    1.11                    0.85                    1.53                    1.60                    1.00                    0.78
pow_mod_mont_window   1.67                    1.42                    1.09                    1.96                    2.06                    1.28                    1.00
-----------------------------------------------------------------------------------------------------------------------------------------------------------------

PS D:\code\clion\rsa\bigint> g++ -std=c++17 -DNDEBUG -DBI_BIT=512 -O3 D:\code\clion\rsa\bigint\test\pow_mod_2.cpp -o pow_mod2.exe | .\pow_mod2.exe
POW_MOD CATEGORY BENCHMARK REPORT (BI_BIT = 512)
[+] Generating 5 test objects per category...
[+] Validating (pow_mod as reference)...
[+] Benching 10x5 per category...
-------------------------------------------------------------------------------------------------------------------------------------------------------------
Case              pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-------------------------------------------------------------------------------------------------------------------------------------------------------------
random_full       377106.000              364898.000              291476.000              274502.000              213822.000              173466.000              147918.000
exp_256bit        196850.000              186396.000              162024.000              177478.000              109266.000              96736.000               76078.000
exp_512bit        406472.000              409626.000              325778.000              274298.000              225458.000              174668.000              144870.000
exp_1024bit       409616.000              420898.000              361624.000              309614.000              228348.000              176526.000              145696.000
exp_2048bit       408814.000              458370.000              335852.000              324600.000              244586.000              192394.000              272150.000
exp_3072bit       438958.000              410130.000              327356.000              311500.000              216284.000              180572.000              143494.000
mod_1024bit       389836.000              381700.000              304918.000              324518.000              212534.000              179252.000              143280.000
mod_2048bit       434504.000              407754.000              330618.000              318914.000              209268.000              180364.000              156136.000
-------------------------------------------------------------------------------------------------------------------------------------------------------------
AVG               382769.500              379971.500              304955.750              289428.000              207445.750              169247.250              153702.750
-------------------------------------------------------------------------------------------------------------------------------------------------------------


SPEEDUP MATRIX (Row vs Column)
Read as: 'Row method is X times faster than Column method' (Values > 1 mean Row is faster)
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
Row \ Col             pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
pow_mod               1.00                    0.99                    0.80                    0.76                    0.54                    0.44                    0.40
pow_mod2              1.01                    1.00                    0.80                    0.76                    0.55                    0.45                    0.40
pow_mod_window        1.26                    1.25                    1.00                    0.95                    0.68                    0.55                    0.50
mr_pow_mod            1.32                    1.31                    1.05                    1.00                    0.72                    0.58                    0.53
pow_mod_mont_cios2    1.85                    1.83                    1.47                    1.40                    1.00                    0.82                    0.74
pow_mod_mont_cios3    2.26                    2.25                    1.80                    1.71                    1.23                    1.00                    0.91
pow_mod_mont_window   2.49                    2.47                    1.98                    1.88                    1.35                    1.10                    1.00
-----------------------------------------------------------------------------------------------------------------------------------------------------------------

PS D:\code\clion\rsa\bigint> g++ -std=c++17 -DNDEBUG -DBI_BIT=64 -O3 D:\code\clion\rsa\bigint\test\pow_mod_2.cpp -o pow_mod2.exe | .\pow_mod2.exe
POW_MOD CATEGORY BENCHMARK REPORT (BI_BIT = 64)
[+] Generating 5 test objects per category...
[+] Validating (pow_mod as reference)...
[+] Benching 10x5 per category...
-------------------------------------------------------------------------------------------------------------------------------------------------------------
Case              pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-------------------------------------------------------------------------------------------------------------------------------------------------------------
random_full       3790.000                3024.000                2970.000                2158.000                846.000                 868.000                 928.000
exp_256bit        4052.000                3242.000                3438.000                2152.000                1038.000                872.000                 916.000
exp_512bit        3856.000                3128.000                3002.000                2156.000                868.000                 870.000                 920.000
exp_1024bit       4136.000                3432.000                3238.000                2268.000                860.000                 872.000                 908.000
exp_2048bit       4108.000                3302.000                3202.000                2054.000                836.000                 844.000                 944.000
exp_3072bit       4156.000                3704.000                3282.000                2020.000                876.000                 866.000                 954.000
mod_1024bit       3646.000                2642.000                2772.000                2390.000                906.000                 912.000                 982.000
mod_2048bit       3838.000                2860.000                2944.000                2172.000                880.000                 878.000                 2822.000
-------------------------------------------------------------------------------------------------------------------------------------------------------------
AVG               3947.750                3166.750                3106.000                2171.250                888.750                 872.750                 1171.750
-------------------------------------------------------------------------------------------------------------------------------------------------------------


SPEEDUP MATRIX (Row vs Column)
Read as: 'Row method is X times faster than Column method' (Values > 1 mean Row is faster)
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
Row \ Col             pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
pow_mod               1.00                    0.80                    0.79                    0.55                    0.23                    0.22                    0.30
pow_mod2              1.25                    1.00                    0.98                    0.69                    0.28                    0.28                    0.37
pow_mod_window        1.27                    1.02                    1.00                    0.70                    0.29                    0.28                    0.38
mr_pow_mod            1.82                    1.46                    1.43                    1.00                    0.41                    0.40                    0.54
pow_mod_mont_cios2    4.44                    3.56                    3.49                    2.44                    1.00                    0.98                    1.32
pow_mod_mont_cios3    4.52                    3.63                    3.56                    2.49                    1.02                    1.00                    1.34
pow_mod_mont_window   3.37                    2.70                    2.65                    1.85                    0.76                    0.74                    1.00
-----------------------------------------------------------------------------------------------------------------------------------------------------------------

PS D:\code\clion\rsa\bigint> g++ -std=c++17 -DNDEBUG -DBI_BIT=1024 -O3 D:\code\clion\rsa\bigint\test\pow_mod_2.cpp -o pow_mod2.exe | .\pow_mod2.exe
POW_MOD CATEGORY BENCHMARK REPORT (BI_BIT = 1024)
[+] Generating 5 test objects per category...
[+] Validating (pow_mod as reference)...
[+] Benching 10x5 per category...
-------------------------------------------------------------------------------------------------------------------------------------------------------------
Case              pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-------------------------------------------------------------------------------------------------------------------------------------------------------------
random_full       3130578.000             2745020.000             2165746.000             2929554.000             2307712.000             1342760.000             1110102.000
exp_256bit        795166.000              669402.000              572590.000              854416.000              622614.000              368458.000              300004.000
exp_512bit        1621398.000             1384024.000             1087612.000             1815278.000             1221422.000             735212.000              575300.000
exp_1024bit       3419310.000             2844072.000             2144448.000             3040580.000             2446760.000             1371658.000             1060998.000
exp_2048bit       3654746.000             2884532.000             2157152.000             2934224.000             2442230.000             1402518.000             1143058.000
exp_3072bit       3232640.000             2783380.000             2163378.000             2919666.000             2345444.000             1363152.000             1201574.000
mod_1024bit       3308890.000             2813564.000             2142598.000             2923432.000             2355550.000             1388538.000             1082286.000
mod_2048bit       3181462.000             2754256.000             2132698.000             2919960.000             2280610.000             1367318.000             1071746.000
-------------------------------------------------------------------------------------------------------------------------------------------------------------
AVG               2793023.750             2359781.250             1820777.750             2542138.750             2002792.750             1167451.750             943133.500
-------------------------------------------------------------------------------------------------------------------------------------------------------------


SPEEDUP MATRIX (Row vs Column)
Read as: 'Row method is X times faster than Column method' (Values > 1 mean Row is faster)
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
Row \ Col             pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
pow_mod               1.00                    0.84                    0.65                    0.91                    0.72                    0.42                    0.34
pow_mod2              1.18                    1.00                    0.77                    1.08                    0.85                    0.49                    0.40
pow_mod_window        1.53                    1.30                    1.00                    1.40                    1.10                    0.64                    0.52
mr_pow_mod            1.10                    0.93                    0.72                    1.00                    0.79                    0.46                    0.37
pow_mod_mont_cios2    1.39                    1.18                    0.91                    1.27                    1.00                    0.58                    0.47
pow_mod_mont_cios3    2.39                    2.02                    1.56                    2.18                    1.72                    1.00                    0.81
pow_mod_mont_window   2.96                    2.50                    1.93                    2.70                    2.12                    1.24                    1.00
-----------------------------------------------------------------------------------------------------------------------------------------------------------------

PS D:\code\clion\rsa\bigint> g++ -std=c++17 -DNDEBUG -DBI_BIT=8192 -O3 D:\code\clion\rsa\bigint\test\pow_mod_2.cpp -o pow_mod2.exe | .\pow_mod2.exe
POW_MOD CATEGORY BENCHMARK REPORT (BI_BIT = 8192)
[+] Generating 5 test objects per category...
[+] Validating (pow_mod as reference)...
[+] Benching 10x5 per category...
-------------------------------------------------------------------------------------------------------------------------------------------------------------
Case              pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-------------------------------------------------------------------------------------------------------------------------------------------------------------
random_full       1249026538.000          1054368234.000          782883796.000           1306720316.000          1073051776.000          693337016.000           528890906.000
exp_256bit        39671600.000            33628718.000            26662582.000            48445306.000            35243720.000            23116188.000            19389042.000
exp_512bit        78960898.000            66801994.000            52724742.000            90080496.000            69163430.000            44614634.000            36616858.000
exp_1024bit       156715468.000           183705042.000           193434678.000           351039686.000           257182666.000           153234204.000           129554784.000
exp_2048bit       613254964.000           573910018.000           420570060.000           732112612.000           601298760.000           388201046.000           248316482.000
exp_3072bit       850604328.000           873163450.000           566207962.000           1074342062.000          853670762.000           548998648.000           463743408.000
mod_1024bit       168631112.000           99149766.000            112356674.000           465062168.000           2250629862.000          1106868782.000          999897540.000
mod_2048bit       384587032.000           297627722.000           299071600.000           844917248.000           1951010462.000          1406544320.000          1025669954.000
-------------------------------------------------------------------------------------------------------------------------------------------------------------
AVG               442681492.500           397794368.000           306739011.750           614089986.750           886406429.750           545614354.750           431509871.750
-------------------------------------------------------------------------------------------------------------------------------------------------------------


SPEEDUP MATRIX (Row vs Column)
Read as: 'Row method is X times faster than Column method' (Values > 1 mean Row is faster)
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
Row \ Col             pow_mod                 pow_mod2                pow_mod_window          mr_pow_mod              pow_mod_mont_cios2      pow_mod_mont_cios3      pow_mod_mont_window
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
pow_mod               1.00                    0.90                    0.69                    1.39                    2.00                    1.23                    0.97
pow_mod2              1.11                    1.00                    0.77                    1.54                    2.23                    1.37                    1.08
pow_mod_window        1.44                    1.30                    1.00                    2.00                    2.89                    1.78                    1.41
mr_pow_mod            0.72                    0.65                    0.50                    1.00                    1.44                    0.89                    0.70
pow_mod_mont_cios2    0.50                    0.45                    0.35                    0.69                    1.00                    0.62                    0.49
pow_mod_mont_cios3    0.81                    0.73                    0.56                    1.13                    1.62                    1.00                    0.79
pow_mod_mont_window   1.03                    0.92                    0.71                    1.42                    2.05                    1.26                    1.00
-----------------------------------------------------------------------------------------------------------------------------------------------------------------
*/