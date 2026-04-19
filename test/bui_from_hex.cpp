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
std::string generate_edge_case_hex(std::mt19937& gen) {
    std::uniform_int_distribution<int> len_dist(1, 128); // 1 to 128 chars (up to 512 bits)
    std::uniform_int_distribution<int> edge_dist(0, 100);
    std::uniform_int_distribution<int> char_dist(0, 21);
    const char* hex_chars = "0123456789ABCDEFabcdef";

    int len = len_dist(gen);
    std::string s = "";

    // Inject Edge Case: Leading Spaces
    if (edge_dist(gen) < 30) s += "   \t  ";

    // Inject Edge Case: "0x" or "0X" prefix
    int prefix_chance = edge_dist(gen);
    if (prefix_chance < 20) s += "0x";
    else if (prefix_chance < 40) s += "0X";

    for (int i = 0; i < len; ++i) {
        // Inject Edge Case: Random underscores (like 12_AB_34)
        if (edge_dist(gen) < 15) s += "_";

        // Inject Edge Case: Random inline spaces
        if (edge_dist(gen) < 10) s += " ";

        s += hex_chars[char_dist(gen)];
    }

    // Inject Edge Case: Trailing Spaces
    if (edge_dist(gen) < 30) s += "    \t";

    return s;
}

inline bui bui_from_hex_old(const std::string& s) {
    assert(!s.empty() && "bui_from_hex: empty string");
    u32 i = 0;
    // skip leading spaces
    while (i < s.size() && isspace(s[i])) ++i;
    // optional "0x" or "0X" prefix
    if (i + 1 < s.size() && s[i] == '0' && (s[i+1] == 'x' || s[i+1] == 'X')) i += 2;

    bool any_digit = false;
    bui out{};
    bui tmp{};
    bui n16 = bui_from_u32(16u);

    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c == '_' || isspace(c)) continue;
        int val = hex_val(c);
        if (val < 0) break;
        any_digit = true;
        mul_ip(out, n16);
        tmp[BI_N - 1] = (u32)val;
        add_ip(out, tmp);
    }
    assert(any_digit && "bui_from_hex: no digits found");
    return out;
}

// ========================================================================
// 2. The Benchmark Runner
// ========================================================================
int main() {
    std::cout << "==================================================\n";
    std::cout << "           HEX PARSER BENCHMARK REPORT            \n";
    std::cout << "==================================================\n";

    // 1. Setup Data
    const int DATASET_SIZE = 10000;
    const int BENCH_ITERATIONS = 1000;
    std::vector<std::string> test_data;
    test_data.reserve(DATASET_SIZE);

    std::mt19937 gen(42); // Fixed seed for reproducible tests

    std::cout << "[+] Generating " << DATASET_SIZE << " edge-case test strings...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        test_data.push_back(generate_edge_case_hex(gen));
    }

    // 2. Validate Correctness (Requirement #1)
    std::cout << "[+] Validating correctness between old and new...\n";
    for (const auto& s : test_data) {
        bui res_old = bui_from_hex_old(s);
        bui res_new = bui_from_hex(s);

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

    // 3. Benchmarking
    std::cout << "--- Benchmarking (" << BENCH_ITERATIONS << " iters x "
              << DATASET_SIZE << " strings) ---\n\n";

    u32 checksum_old = 0;
    u32 checksum_new = 0;

    // --- Bench Old ---
    auto start_old = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& s : test_data) {
            bui res = bui_from_hex_old(s);
            checksum_old ^= res[0]; // Anti-dead-code elimination
        }
    }
    auto end_old = std::chrono::high_resolution_clock::now();

    // --- Bench New ---
    auto start_new = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& s : test_data) {
            bui res = bui_from_hex(s);
            checksum_new ^= res[0]; // Anti-dead-code elimination
        }
    }
    auto end_new = std::chrono::high_resolution_clock::now();

    // 4. Calculate Nanoseconds per Call (Requirement #3)
    double total_calls = (double)DATASET_SIZE * BENCH_ITERATIONS;

    std::chrono::duration<double, std::milli> time_old_ms = end_old - start_old;
    std::chrono::duration<double, std::milli> time_new_ms = end_new - start_new;

    double ns_per_call_old = (time_old_ms.count() * 1'000'000.0) / total_calls;
    double ns_per_call_new = (time_new_ms.count() * 1'000'000.0) / total_calls;

    // 5. Clean Report Output (Requirement #4)
    std::cout << "[Old Version: bui_from_hex_old]\n";
    std::cout << "Total Time   : " << std::fixed << std::setprecision(2) << time_old_ms.count() << " ms\n";
    std::cout << "Time per call: " << std::fixed << std::setprecision(2) << ns_per_call_old << " ns\n\n";

    std::cout << "[New Version: bui_from_hex]\n";
    std::cout << "Total Time   : " << std::fixed << std::setprecision(2) << time_new_ms.count() << " ms\n";
    std::cout << "Time per call: " << std::fixed << std::setprecision(2) << ns_per_call_new << " ns\n\n";

    double speedup = ((ns_per_call_old - ns_per_call_new) / ns_per_call_old) * 100.0;
    std::cout << "=> SPEEDUP   : +" << std::fixed << std::setprecision(2) << speedup << "%\n";
    std::cout << "==================================================\n";

    // Print checksums invisibly to ensure compiler doesn't throw away the loops
    if (checksum_old != checksum_new) {
        std::cerr << "Warning: Checksums don't match! (Compiler glitch)\n";
    }

    return 0;
}