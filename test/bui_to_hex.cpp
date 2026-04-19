#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <random>
#include <iomanip>
#include <cstdlib>
#include <sstream>

// Include your bigint library here
#include "../bigint.h"

// ========================================================================
// 1. Edge Case Test Data Generator
// ========================================================================
struct TestCase {
    bui val;
    bool uppercase;
    bool split;
};

TestCase generate_edge_case_bui(std::mt19937& gen) {
    std::uniform_int_distribution<u32> val_dist(0, 0xFFFFFFFF);
    std::uniform_int_distribution<int> edge_dist(0, 100);
    std::uniform_int_distribution<int> limb_dist(0, BI_N - 1);
    std::uniform_int_distribution<int> bool_dist(0, 1);

    TestCase tc;

    // Default zero initialization
    for (int i = 0; i < BI_N; ++i) tc.val[i] = 0;

    // Randomize flags
    tc.uppercase = bool_dist(gen);
    tc.split = bool_dist(gen);

    int edge = edge_dist(gen);

    if (edge < 10) {
        // Edge Case 1: Pure Zero
        return tc;
    }
    else if (edge < 20) {
        // Edge Case 2: Only the lowest limb has data (small numbers)
        tc.val[BI_N - 1] = val_dist(gen);
    }
    else if (edge < 30) {
        // Edge Case 3: Fully maxed out (all FFs)
        for (int i = 0; i < BI_N; ++i) tc.val[i] = 0xFFFFFFFF;
    }
    else {
        // Edge Case 4: Random length big integer
        int start_limb = limb_dist(gen);

        // Inject Edge Case: Force leading zeros on the Highest Active Limb
        // This explicitly tests the `first_limb` zero-stripping logic in your string formatters.
        if (edge_dist(gen) < 50) {
            tc.val[start_limb] = val_dist(gen) & 0x00000FFF; // Max 3 hex chars
        } else {
            tc.val[start_limb] = val_dist(gen);
            if (tc.val[start_limb] == 0) tc.val[start_limb] = 1; // Ensure it remains the highest limb
        }

        // Fill the rest of the limbs normally
        for (int i = start_limb + 1; i < BI_N; ++i) {
            tc.val[i] = val_dist(gen);
        }
    }
    return tc;
}

inline std::string bui_to_hex_old(const bui &a, bool uppercase = false, bool split = false) {
    std::ostringstream o;
    o << std::hex << std::setfill('0');
    if (uppercase) o << std::uppercase;
    u32 hl = highest_limb(a);
    u32 idx = BI_N - hl - 1;
    o << a[idx];
    if (split && idx != BI_N - 1) o << ' ';
    for (++idx; idx < BI_N; ++idx) {
        o << std::setw(8) << a[idx]; // u32 = 8 hex
        if (split && idx != BI_N - 1) o << ' ';
    }
    return o.str();
}


// ========================================================================
// 2. The Benchmark Runner
// ========================================================================
int main() {
    std::cout << "==================================================\n";
    std::cout << "       HEX FORMATTER BENCHMARK REPORT             \n";
    std::cout << "==================================================\n";

    // 1. Setup Data
    const int DATASET_SIZE = 10000;
    const int BENCH_ITERATIONS = 1000;
    std::vector<TestCase> test_data;
    test_data.reserve(DATASET_SIZE);

    std::mt19937 gen(42); // Fixed seed for reproducible tests

    std::cout << "[+] Generating " << DATASET_SIZE << " edge-case test objects...\n";
    for (int i = 0; i < DATASET_SIZE; ++i) {
        test_data.push_back(generate_edge_case_bui(gen));
    }

    // 2. Validate Correctness (Requirement #1)
    std::cout << "[+] Validating correctness between old and new...\n";
    for (const auto& tc : test_data) {
        std::string res_old = bui_to_hex_old(tc.val, tc.uppercase, tc.split);
        std::string res_new = bui_to_hex(tc.val, tc.uppercase, tc.split);

        if (res_old != res_new) {
            std::cerr << "\n[!] VALIDATION FAILED! PROGRAM HALTED.\n";
            std::cerr << "--------------------------------------------------\n";
            std::cerr << "Params       : uppercase=" << tc.uppercase << ", split=" << tc.split << "\n";
            std::cerr << "Old Output   : \"" << res_old << "\"\n";
            std::cerr << "New Output   : \"" << res_new << "\"\n";
            std::cerr << "--------------------------------------------------\n";
            std::exit(1);
        }
    }
    std::cout << "    -> Validation PASSED. Outputs are 100% identical.\n\n";

    // 3. Benchmarking
    std::cout << "--- Benchmarking (" << BENCH_ITERATIONS << " iters x "
              << DATASET_SIZE << " objects) ---\n\n";

    size_t checksum_old = 0;
    size_t checksum_new = 0;

    // --- Bench Old ---
    auto start_old = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& tc : test_data) {
            std::string res = bui_to_hex_old(tc.val, tc.uppercase, tc.split);
            checksum_old += res.length(); // Anti-dead-code elimination
        }
    }
    auto end_old = std::chrono::high_resolution_clock::now();

    // --- Bench New ---
    auto start_new = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < BENCH_ITERATIONS; ++i) {
        for (const auto& tc : test_data) {
            std::string res = bui_to_hex(tc.val, tc.uppercase, tc.split);
            checksum_new += res.length(); // Anti-dead-code elimination
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
    std::cout << "[Old Version: bui_to_hex_old]\n";
    std::cout << "Total Time   : " << std::fixed << std::setprecision(2) << time_old_ms.count() << " ms\n";
    std::cout << "Time per call: " << std::fixed << std::setprecision(2) << ns_per_call_old << " ns\n\n";

    std::cout << "[New Version: bui_to_hex]\n";
    std::cout << "Total Time   : " << std::fixed << std::setprecision(2) << time_new_ms.count() << " ms\n";
    std::cout << "Time per call: " << std::fixed << std::setprecision(2) << ns_per_call_new << " ns\n\n";

    double speedup = ((ns_per_call_old - ns_per_call_new) / ns_per_call_old) * 100.0;
    std::cout << "=> SPEEDUP   : +" << std::fixed << std::setprecision(2) << speedup << "%\n";
    std::cout << "==================================================\n";

    if (checksum_old != checksum_new) {
        std::cerr << "Warning: Checksums don't match! (Compiler glitch)\n";
    }

    return 0;
}