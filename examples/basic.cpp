#include <iskra64/iskra64.hpp>
#include <array>
#include <iostream>
#include <unordered_map>
int main() {
    constexpr std::uint64_t seed = 42; // A seed is NOT a cryptographic key.
    std::cout << "single: " << iskra64::hash_u64(123, seed) << '\n';
    std::array<std::uint64_t, 5> keys{10, 20, 30, 40, 50}, hashes{};
    if (!iskra64::hash_many(keys.data(), hashes.data(), keys.size(), seed)) return 1;
    std::unordered_map<std::uint64_t, int, iskra64::u64_hasher> table(0, iskra64::u64_hasher{seed});
    table.emplace(123, 7);
    std::cout << "backend: " << iskra64::backend_name(iskra64::selected_backend()) << '\n';
    return table.at(123) == 7 ? 0 : 1;
}
