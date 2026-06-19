#include "ivf_parser.hpp"
#include "byte_utils.hpp"
#include <cstring>

namespace usm_toolkit {

bool IvfParser::parse_header(const uint8_t* data, size_t size, IvfHeader& header) {
    if (size < IVF_HEADER_SIZE) return false;
    
    // Read signature (little endian uint32)
    header.signature = ByteUtils::read_uint32_le(data);
    if (header.signature != IVF_SIGNATURE) return false;
    
    header.version = ByteUtils::read_uint16_le(data + 4);
    header.header_length = ByteUtils::read_uint16_le(data + 6);
    
    if (header.header_length < IVF_HEADER_SIZE) return false;
    
    // FourCC (4 bytes, ASCII)
    std::memcpy(header.fourcc, data + 8, 4);
    
    header.width = ByteUtils::read_uint16_le(data + 12);
    header.height = ByteUtils::read_uint16_le(data + 14);
    header.framerate_n = ByteUtils::read_uint32_le(data + 16);
    header.framerate_d = ByteUtils::read_uint32_le(data + 20);
    header.frame_count = ByteUtils::read_uint32_le(data + 24);
    header.unused = ByteUtils::read_uint32_le(data + 28);
    
    return true;
}

bool IvfParser::parse_frame_header(const uint8_t* data, size_t size, IvfFrameHeader& frame) {
    if (size < IVF_FRAME_HEADER_SIZE) return false;
    
    frame.frame_size = ByteUtils::read_uint32_le(data);
    
    // Timestamp is 8 bytes little endian
    frame.timestamp = 0;
    for (int i = 0; i < 8; i++) {
        frame.timestamp |= static_cast<uint64_t>(data[4 + i]) << (i * 8);
    }
    
    return true;
}

Vp9CodecConfig IvfParser::get_codec_config(const IvfHeader& header) {
    // VP9 codec config is stored in the last 4 bytes of IVF header
    // Profile is bits 0-1, level is bits 2-5, etc.
    uint32_t config = header.unused;
    
    Vp9CodecConfig cfg;
    cfg.profile = config & 0x03;
    cfg.level = (config >> 2) & 0x0F;
    cfg.bit_depth = ((config >> 6) & 0x01) ? 12 : 10;
    cfg.chroma_subsampling = (config >> 7) & 0x03;
    cfg.full_range = (config >> 9) & 0x01;
    
    return cfg;
}

double IvfParser::get_framerate(const IvfHeader& header) {
    if (header.framerate_d == 0) return 0.0;
    return static_cast<double>(header.framerate_n) / static_cast<double>(header.framerate_d);
}

std::string IvfParser::get_fourcc_string(const IvfHeader& header) {
    return std::string(header.fourcc, 4);
}

bool IvfParser::is_ivf(const uint8_t* data, size_t size) {
    if (size < 4) return false;
    return ByteUtils::read_uint32_le(data) == IVF_SIGNATURE;
}

bool IvfParser::is_vp9(const IvfHeader& header) {
    return std::memcmp(header.fourcc, "VP90", 4) == 0;
}

} // namespace usm_toolkit
