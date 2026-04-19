#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstdlib>

// Include your bigint library here
#include "../bigint.h"

// ========================================================================
// 1. Edge Case Test Data Generator
// ========================================================================
std::string generate_edge_case_dec(std::mt19937& gen) {
    // Max decimal digits for 512 bits is ~154. We generate up to 140 to avoid overflow.
    std::uniform_int_distribution<int> len_dist(1, 140);
    std::uniform_int_distribution<int> edge_dist(0, 100);
    std::uniform_int_distribution<int> char_dist('0', '9');

    int len = len_dist(gen);
    std::string s = "";

    // Inject Edge Case: Leading Spaces
    if (edge_dist(gen) < 30) s += "   \t  ";

    // Inject Edge Case: Optional '+'
    if (edge_dist(gen) < 40) s += "+";

    // Inject Edge Case: Leading Zeros
    if (edge_dist(gen) < 50) {
        int zero_count = edge_dist(gen) % 10 + 1;
        s.append(zero_count, '0');
    }

    // Generate Core Digits
    for (int i = 0; i < len; ++i) {
        // Inject Edge Case: Random underscores
        if (edge_dist(gen) < 15) s += "_";

        s += (char)char_dist(gen);
    }

    // Inject Edge Case: Trailing Garbage (Letters/Spaces)
    // Both algorithms must stop cleanly here!
    if (edge_dist(gen) < 30) s += "  ABc def";

    return s;
}


inline bui bui_from_dec_old(const std::string& s) {
    assert(!s.empty() && "bui_from_dec: empty string");
    u32 i = 0;
    // skip leading spaces and optional '+'
    while (isspace(s[i])) ++i;
    if (s[i] == '+') ++i;
    assert(s[i] != '-' && "bui_from_dec: negative not supported");
    // skip leading zeros, underscores, spaces
    while (s[i] == '0' || s[i] == '_') ++i;
    bool any_digit = false;
    // process each digit in the decimal string
    bui n10 = bui_from_u32(10u);
    bui out{}, tmp{};
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c == '_' || isspace(c)) continue;
        if (c < '0' || c > '9') break;
        any_digit = true;
        mul_ip(out, n10);
        tmp[BI_N - 1] = c - '0';
        add_ip(out, tmp);
    }
    assert(any_digit && "bui_from_dec: no digits found");
    return out;
}

// ========================================================================
// 2. The Benchmark Runner
// ========================================================================
int main() {
    std::cout << "==================================================\n";
    std::cout << "           DEC PARSER BENCHMARK REPORT            \n";
    std::cout << "==================================================\n";

    const int DATASET_SIZE = 10000;
    const int BENCH_ITERATIONS = 1000;
    std::vector<std::string> test_data;
    test_data.reserve(DATASET_SIZE);

    std::mt19937 gen(12345);

    std::cout << "[+] Generating " << DATASET_SIZE << " edge-case decimal strings...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        test_data.push_back(generate_edge_case_dec(gen));
    }

    // Validation (Requirement #1)
    std::cout << "[+] Validating correctness between old and new...\n";
    for (const auto& s : test_data) {
        bui res_old = bui_from_dec_old(s);
        bui res_new = bui_from_dec(s); // Uses your new fixed version below

        if (cmp(res_old, res_new) != 0) {
            std::cerr << "\n[!] VALIDATION FAILED! PROGRAM HALTED.\n";
            std::cerr << "--------------------------------------------------\n";
            std::cerr << "Input String : \"" << s << "\"\n";
            std::cerr << "Old Result   : " << bui_to_hex(res_old, true, true) << "\n";
            std::cerr << "New Result   : " << bui_to_hex(res_new, true, true) << "\n";
            std::cerr << "--------------------------------------------------\n";
            std::exit(1);
        }
    }
    std::cout << "    -> Validation PASSED. Outputs are 100% identical.\n\n";

    // Benchmarking
    std::cout << "--- Benchmarking (" << BENCH_ITERATIONS << " iters x "
              << DATASET_SIZE << " strings) ---\n\n";

    u32 checksum_old = 0;
    u32 checksum_new = 0;

    auto start_old = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& s : test_data) {
            checksum_old ^= bui_from_dec_old(s)[0];
        }
    }
    auto end_old = std::chrono::high_resolution_clock::now();

    auto start_new = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& s : test_data) {
            checksum_new ^= bui_from_dec(s)[0];
        }
    }
    auto end_new = std::chrono::high_resolution_clock::now();

    // Calculate Nanoseconds per Call (Requirement #3)
    double total_calls = (double)DATASET_SIZE * BENCH_ITERATIONS;
    double time_old_ms = std::chrono::duration<double, std::milli>(end_old - start_old).count();
    double time_new_ms = std::chrono::duration<double, std::milli>(end_new - start_new).count();

    double ns_per_call_old = (time_old_ms * 1'000'000.0) / total_calls;
    double ns_per_call_new = (time_new_ms * 1'000'000.0) / total_calls;

    // Report (Requirement #4)
    std::cout << "[Old Version: bui_from_dec_old]\n";
    std::cout << "Total Time   : " << std::fixed << std::setprecision(2) << time_old_ms << " ms\n";
    std::cout << "Time per call: " << std::fixed << std::setprecision(2) << ns_per_call_old << " ns\n\n";

    std::cout << "[New Version: bui_from_dec]\n";
    std::cout << "Total Time   : " << std::fixed << std::setprecision(2) << time_new_ms << " ms\n";
    std::cout << "Time per call: " << std::fixed << std::setprecision(2) << ns_per_call_new << " ns\n\n";

    double speedup = ((ns_per_call_old - ns_per_call_new) / ns_per_call_old) * 100.0;
    std::cout << "=> SPEEDUP   : +" << std::fixed << std::setprecision(2) << speedup << "%\n";
    std::cout << "==================================================\n";

    if (checksum_old != checksum_new) std::cerr << "Warning: Checksum glitch!\n";

    return 0;
}