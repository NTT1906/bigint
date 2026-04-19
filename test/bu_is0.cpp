#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <iomanip>

// --- Definitions ---
typedef uint32_t u32;
constexpr u32 BI_N = 100; // 512-bit math

// --- The Old Version ---
inline bool bu_is0_old(const u32 *x, u32 n) {
    while (n-- > 0)
        if (x[n] != 0) return false;
    return true;
}

// --- The New Optimized Version ---
inline bool bu_is0_new(const u32 *x, u32 n) {
    for (; n >= 4; n -= 4)
        if (x[n-4] | x[n-3] | x[n-2] | x[n-1]) return false;

    while (n-- > 0)
        if (x[n]) return false;

    return true;
}

// --- Benchmark Runner ---
void run_benchmark(const std::string& name, const std::vector<u32>& data) {
    const int iterations = 100'000'000;

    // We use a volatile accumulator so the compiler doesn't optimize the loop away
    volatile int dummy = 0;

    // Bench Old
    auto start_old = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        if (bu_is0_old(data.data(), BI_N)) dummy++;
    }
    auto end_old = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> time_old = end_old - start_old;

    // Bench New
    auto start_new = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        if (bu_is0_new(data.data(), BI_N)) dummy++;
    }
    auto end_new = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double, std::milli> time_new = end_new - start_new;

    // Print Results
    std::cout << "--- " << name << " ---" << "\n";
    std::cout << "Old Version: " << std::fixed << std::setprecision(2) << time_old.count() << " ms\n";
    std::cout << "New Version: " << std::fixed << std::setprecision(2) << time_new.count() << " ms\n";

    double speedup = (time_old.count() - time_new.count()) / time_old.count() * 100.0;
    std::cout << "Speedup:     " << (speedup > 0 ? "+" : "") << speedup << "%\n\n";
}

int main() {
    std::cout << "Running 100 Million Iterations per test...\n\n";

    // Scenario 1: Worst Case (All Zeros)
    // Both algorithms must scan the entire 512 bits.
    std::vector<u32> worst_case(BI_N, 0);
    run_benchmark("Worst Case (All Zeros)", worst_case);

    // Scenario 2: Best Case (Small Number)
    // Data is located at the Least Significant Word (x[15]).
    std::vector<u32> best_case(BI_N, 0);
    best_case[BI_N - 1] = 1;
    run_benchmark("Best Case (Data at end)", best_case);

    // Scenario 3: Average Case (Large Number)
    // Data is located at the Most Significant Word (x[0]).
    std::vector<u32> large_case(BI_N, 0);
    large_case[0] = 0xFFFFFFFF;
    run_benchmark("Large Case (Data at start)", large_case);

    return 0;
}