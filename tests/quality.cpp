#include <iskra64/iskra64.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <random>
#include <vector>
using u64 = std::uint64_t;
int main() {
    constexpr std::size_t samples = 32768;
    std::array<std::array<unsigned,64>,64> flips{};
    std::mt19937_64 rng(987654321);
    u64 flipped_total = 0;
    for (std::size_t n = 0; n < samples; ++n) {
        const u64 x = rng(), h = iskra64::hash_u64(x);
        for (unsigned bit = 0; bit < 64; ++bit) {
            const u64 d = h ^ iskra64::hash_u64(x ^ (UINT64_C(1) << bit));
            for (unsigned j = 0; j < 64; ++j) {
                const auto v = static_cast<unsigned>((d >> j) & 1);
                flips[bit][j] += v;
                flipped_total += v;
            }
        }
    }
    double worst = 0;
    for (auto& row: flips) for (auto c: row) worst = std::max(worst, std::abs(double(c)/samples-0.5));
    constexpr std::size_t keys = 1 << 20, bins = 1 << 16;
    std::vector<unsigned> buckets(bins);
    std::vector<u64> hashes(keys);
    for (std::size_t i = 0; i < keys; ++i) {
        const auto h = iskra64::hash_u64(u64(i) << 20,42);
        hashes[i] = h;
        ++buckets[h & (bins-1)];
    }
    std::sort(hashes.begin(),hashes.end());
    std::size_t collisions = 0;
    for (std::size_t i = 1; i < keys; ++i) collisions += hashes[i] == hashes[i-1];
    double chi2 = 0;
    const double expected = double(keys)/bins;
    for (auto count: buckets) chi2 += (count-expected)*(count-expected)/expected;
    // Diagnostic thresholds only, not a substitute for SMHasher3 or an audit.
    const bool ok = collisions == 0 && worst < 0.025 && chi2/(bins-1)>0.9 && chi2/(bins-1)<1.1;
    std::cout << std::setprecision(12)
              << "{\n  \"status\": \"" << (ok?"PASS":"FAIL") << "\",\n"
              << "  \"avalanche_samples\": " << samples << ",\n"
              << "  \"avalanche_cells\": 4096,\n"
              << "  \"avalanche_seed\": 0,\n"
              << "  \"mean_flipped_bits\": " << double(flipped_total)/(samples*64) << ",\n"
              << "  \"worst_probability_deviation_from_half\": " << worst << ",\n"
              << "  \"structured_keys\": " << keys << ",\n"
              << "  \"structured_seed\": 42,\n"
              << "  \"structured_stride\": 1048576,\n"
              << "  \"full64_collisions\": " << collisions << ",\n"
              << "  \"low16_bins\": " << bins << ",\n"
              << "  \"low16_reduced_chi_square\": " << chi2/(bins-1) << ",\n"
              << "  \"max_bucket_load\": " << *std::max_element(buckets.begin(),buckets.end()) << "\n}\n";
    return ok ? 0 : 1;
}
