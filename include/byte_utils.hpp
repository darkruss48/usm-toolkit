#ifndef BYTE_UTILS_HPP
#define BYTE_UTILS_HPP

#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

namespace usm_toolkit {

/**
 * Byte manipulation utilities for reading binary data
 * with proper endian handling.
 */
class ByteUtils {
public:
    // Read unsigned 16-bit big endian
    static uint16_t read_uint16_be(const uint8_t* data) {
        return static_cast<uint16_t>((data[0] << 8) | data[1]);
    }

    // Read unsigned 32-bit big endian
    static uint32_t read_uint32_be(const uint8_t* data) {
        return static_cast<uint32_t>(
            (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3]
        );
    }

    // Read unsigned 16-bit little endian
    static uint16_t read_uint16_le(const uint8_t* data) {
        return static_cast<uint16_t>((data[1] << 8) | data[0]);
    }

    // Read unsigned 32-bit little endian
    static uint32_t read_uint32_le(const uint8_t* data) {
        return static_cast<uint32_t>(
            (data[3] << 24) | (data[2] << 16) | (data[1] << 8) | data[0]
        );
    }

    // Get high nibble of a byte
    static uint8_t get_high_nibble(uint8_t value) {
        return (value >> 4) & 0x0F;
    }

    // Get low nibble of a byte
    static uint8_t get_low_nibble(uint8_t value) {
        return value & 0x0F;
    }

    // Compare byte arrays
    static bool compare_bytes(const uint8_t* arr1, const uint8_t* arr2, size_t length) {
        return std::memcmp(arr1, arr2, length) == 0;
    }

    // Compare byte vectors
    static bool compare_bytes(const std::vector<uint8_t>& arr1, 
                              const std::vector<uint8_t>& arr2) {
        if (arr1.size() != arr2.size()) return false;
        return std::memcmp(arr1.data(), arr2.data(), arr1.size()) == 0;
    }

    // Compare vector with raw array
    static bool compare_bytes(const std::vector<uint8_t>& arr1,
                              const uint8_t* arr2, size_t length) {
        if (arr1.size() < length) return false;
        return std::memcmp(arr1.data(), arr2, length) == 0;
    }

    // Convert 4 bytes to uint32 (for signature matching)
    static uint32_t bytes_to_uint32(const uint8_t* bytes) {
        // Store as little endian for map key lookup (same as original C#)
        return *reinterpret_cast<const uint32_t*>(bytes);
    }

    // Reverse byte array (for endian conversion)
    static void reverse_bytes(uint8_t* data, size_t length) {
        std::reverse(data, data + length);
    }

    static std::vector<uint8_t> reverse_bytes(const std::vector<uint8_t>& data) {
        std::vector<uint8_t> result(data.rbegin(), data.rend());
        return result;
    }
};

} // namespace usm_toolkit

#endif // BYTE_UTILS_HPP
