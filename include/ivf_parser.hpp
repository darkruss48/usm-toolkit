#ifndef IVF_PARSER_HPP
#define IVF_PARSER_HPP

/**
 * IVF (Indeo Video Format) parser for VP9 frames.
 * Extracts VP9 frames from IVF containers embedded in USM @SFV blocks.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace usm_toolkit {

/**
 * IVF file header structure (32 bytes)
 */
struct IvfHeader {
    uint32_t signature;       // "DKIF" = 0x46494B44
    uint16_t version;         // Should be 0
    uint16_t header_length;   // Should be 32
    char     fourcc[4];       // "VP90" for VP9
    uint16_t width;
    uint16_t height;
    uint32_t framerate_n;     // Framerate numerator
    uint32_t framerate_d;     // Framerate denominator
    uint32_t frame_count;
    uint32_t unused;
};

/**
 * IVF frame header structure (12 bytes)
 */
struct IvfFrameHeader {
    uint32_t frame_size;      // Little endian
    uint64_t timestamp;       // Presentation timestamp
};

/**
 * VP9 codec configuration record (from IVF header)
 */
struct Vp9CodecConfig {
    uint8_t profile;
    uint8_t level;
    uint8_t bit_depth;
    uint8_t chroma_subsampling;  // 0=4:2:0, 1=4:2:2, 2=4:4:4
    uint8_t full_range;
};

/**
 * IVF Parser class
 * Reads IVF containers and extracts VP9 frames.
 */
class IvfParser {
public:
    /**
     * Parse IVF header from a byte buffer.
     * Returns true on success.
     */
    static bool parse_header(const uint8_t* data, size_t size, IvfHeader& header);

    /**
     * Parse IVF frame header from a byte buffer.
     * Returns true on success.
     */
    static bool parse_frame_header(const uint8_t* data, size_t size, IvfFrameHeader& frame);

    /**
     * Extract VP9 codec configuration from IVF header.
     * The codec config is stored in the last 4 bytes of the IVF header (offset 28).
     */
    static Vp9CodecConfig get_codec_config(const IvfHeader& header);

    /**
     * Calculate framerate from numerator and denominator.
     */
    static double get_framerate(const IvfHeader& header);

    /**
     * Get FourCC as a string.
     */
    static std::string get_fourcc_string(const IvfHeader& header);

    /**
     * Check if the data starts with a valid IVF signature (DKIF).
     */
    static bool is_ivf(const uint8_t* data, size_t size);

    /**
     * Check if the FourCC indicates VP9.
     */
    static bool is_vp9(const IvfHeader& header);

    // Signature constants
    static constexpr uint32_t IVF_SIGNATURE = 0x46494B44;  // "DKIF" in little endian
    static constexpr uint16_t IVF_HEADER_SIZE = 32;
    static constexpr uint16_t IVF_FRAME_HEADER_SIZE = 12;
};

} // namespace usm_toolkit

#endif // IVF_PARSER_HPP
