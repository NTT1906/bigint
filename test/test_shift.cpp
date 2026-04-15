#include <iostream>
#include <chrono>
#include <vector>
#include <iomanip>
#include <cassert>

// Set bit size large enough to demonstrate the memory-bandwidth difference
#define BI_BIT 4096
#include "../bigint.h"

// Helper to generate a random double-width (bul) number
bul random_bul() {
    bui high, low;
    randomize_ip(high);
    randomize_ip(low);
    bul r{};
    std::copy(high.begin(), high.end(), r.begin());
    std::copy(low.begin(), low.end(), r.begin() + BI_N);
    return r;
}

// Structure to hold benchmark results
struct BenchResult {
    std::string name;
    std::string size_type;
    double old_ns;
    double new_ns;
    double speedup;
};

int main() {
    std::cout << "========================================================\n";
    std::cout << " BIGINT SHIFT BENCHMARK (BI_BIT = " << BI_BIT << ")\n";
    std::cout << "========================================================\n\n";

    const int NUM_TESTS = 200000;
    std::vector<BenchResult> results;

    std::cout << "Pre-generating " << NUM_TESTS << " random inputs to ensure clean timers...\n";

    // Pre-generate arrays
    std::vector<bui> bui_data(NUM_TESTS);
    std::vector<bul> bul_data(NUM_TESTS);
    std::vector<u32> shifts_bui(NUM_TESTS);
    std::vector<u32> shifts_bul(NUM_TESTS);

    for (int i = 0; i < NUM_TESTS; ++i) {
        randomize_ip(bui_data[i]);
        bul_data[i] = random_bul();
        shifts_bui[i] = rand() % BI_BIT;         // Random shift from 0 to BI_BIT - 1
        shifts_bul[i] = rand() % (BI_BIT * 2);   // Random shift from 0 to BI_BIT * 2 - 1
        // printf("%s\n", bui_to_dec(bui_data[i]).c_str());
    }

    std::cout << "Running verification and benchmarks...\n\n";

    // ==========================================
    // 1. Shift Left (bui)
    // ==========================================
    {
        std::vector<bui> old_arr = bui_data;
        std::vector<bui> new_arr = bui_data;

        auto start_old = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            shift_left_ip_imp(old_arr[i].data(), BI_N, shifts_bui[i]);
        }
        auto end_old = std::chrono::high_resolution_clock::now();

        auto start_new = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            shift_left_ip_fused_imp(new_arr[i].data(), BI_N, shifts_bui[i]);
        }
        auto end_new = std::chrono::high_resolution_clock::now();

        // Verification
        for (int i = 0; i < NUM_TESTS; ++i) {
            if (cmp(old_arr[i], new_arr[i]) != 0) {
                std::cerr << "[!] FAILED: Shift Left (bui) results mismatch at index " << i << "!\n";
                exit(1);
            }
        }

        double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
        double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;
        results.push_back({"Shift Left", "bui", old_ns, new_ns, old_ns / new_ns});
    }

    // ==========================================
    // 2. Shift Left (bul)
    // ==========================================
    {
        std::vector<bul> old_arr = bul_data;
        std::vector<bul> new_arr = bul_data;

        auto start_old = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            shift_left_ip_imp(old_arr[i].data(), BI_N * 2, shifts_bul[i]);
        }
        auto end_old = std::chrono::high_resolution_clock::now();

        auto start_new = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            shift_left_ip_fused_imp(new_arr[i].data(), BI_N * 2, shifts_bul[i]);
        }
        auto end_new = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_TESTS; ++i) {
            if (cmp(old_arr[i], new_arr[i]) != 0) {
                std::cerr << "[!] FAILED: Shift Left (bul) results mismatch at index " << i << "!\n";
                exit(1);
            }
        }

        double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
        double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;
        results.push_back({"Shift Left", "bul", old_ns, new_ns, old_ns / new_ns});
    }

    // ==========================================
    // 3. Shift Right (bui)
    // ==========================================
    {
        std::vector<bui> old_arr = bui_data;
        std::vector<bui> new_arr = bui_data;

        auto start_old = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            shift_right_ip_imp(old_arr[i].data(), BI_N, shifts_bui[i]);
        }
        auto end_old = std::chrono::high_resolution_clock::now();

        auto start_new = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            shift_right_ip_fused_imp(new_arr[i].data(), BI_N, shifts_bui[i]);
        }
        auto end_new = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_TESTS; ++i) {
            if (cmp(old_arr[i], new_arr[i]) != 0) {
                std::cerr << "[!] FAILED: Shift Right (bui) results mismatch at index " << i << "!\n";
                exit(1);
            }
        }

        double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
        double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;
        results.push_back({"Shift Right", "bui", old_ns, new_ns, old_ns / new_ns});
    }

    // ==========================================
    // 4. Shift Right (bul)
    // ==========================================
    {
        std::vector<bul> old_arr = bul_data;
        std::vector<bul> new_arr = bul_data;

        auto start_old = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            shift_right_ip_imp(old_arr[i].data(), BI_N * 2, shifts_bul[i]);
        }
        auto end_old = std::chrono::high_resolution_clock::now();

        auto start_new = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < NUM_TESTS; ++i) {
            shift_right_ip_fused_imp(new_arr[i].data(), BI_N * 2, shifts_bul[i]);
        }
        auto end_new = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < NUM_TESTS; ++i) {
            if (cmp(old_arr[i], new_arr[i]) != 0) {
                std::cerr << "[!] FAILED: Shift Right (bul) results mismatch at index " << i << "!\n";
                exit(1);
            }
        }

        double old_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_old - start_old).count() / NUM_TESTS;
        double new_ns = (double)std::chrono::duration_cast<std::chrono::nanoseconds>(end_new - start_new).count() / NUM_TESTS;
        results.push_back({"Shift Right", "bul", old_ns, new_ns, old_ns / new_ns});
    }

    // ==========================================
    // Print Report
    // ==========================================
    std::cout << "[SUCCESS] Mathematical Verification Passed!\n\n";
    std::cout << "--------------------------------------------------------------------------\n";
    std::cout << std::left << std::setw(15) << "Operation"
              << std::setw(8) << "Size"
              << std::setw(18) << "Old (ns/call)"
              << std::setw(18) << "New (ns/call)"
              << "Speedup\n";
    std::cout << "--------------------------------------------------------------------------\n";

    std::cout << std::fixed << std::setprecision(2);
    for (const auto& r : results) {
        std::cout << std::left << std::setw(15) << r.name
                  << std::setw(8) << r.size_type
                  << std::setw(18) << r.old_ns
                  << std::setw(18) << r.new_ns
                  << r.speedup << "x\n";
    }
    std::cout << "--------------------------------------------------------------------------\n";

    return 0;
}

// ========================================================
//  BIGINT SHIFT BENCHMARK (BI_BIT = 512)
// ========================================================
//
// Pre-generating 200000 random inputs to ensure clean timers...
// Running verification and benchmarks...
//
// [SUCCESS] Mathematical Verification Passed!
//
// --------------------------------------------------------------------------
// Operation      Size    Old (ns/call)     New (ns/call)     Speedup
// --------------------------------------------------------------------------
// Shift Left     bui     15.47             12.38             1.25x
// Shift Left     bul     43.30             21.83             1.98x
// Shift Right    bui     25.35             19.54             1.30x
// Shift Right    bul     41.46             34.97             1.19x
// --------------------------------------------------------------------------
//
// ========================================================
//  BIGINT SHIFT BENCHMARK (BI_BIT = 4096)
// ========================================================
//
// Pre-generating 200000 random inputs to ensure clean timers...
// Running verification and benchmarks...
//
// [SUCCESS] Mathematical Verification Passed!
//
// --------------------------------------------------------------------------
// Operation      Size    Old (ns/call)     New (ns/call)     Speedup
// --------------------------------------------------------------------------
// Shift Left     bui     136.55            78.79             1.73x
// Shift Left     bul     318.36            154.72            2.06x
// Shift Right    bui     163.43            109.71            1.49x
// Shift Right    bul     365.94            324.38            1.13x
// --------------------------------------------------------------------------