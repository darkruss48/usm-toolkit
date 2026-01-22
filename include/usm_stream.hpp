#ifndef USM_STREAM_HPP
#define USM_STREAM_HPP

#include "mpeg_stream.hpp"
#include <string>
#include <vector>
#include <fstream>

namespace usm_toolkit {

/**
 * CRI USM stream parser
 * Ported from VGMToolbox CriUsmStream.cs
 */
class CriUsmStream : public MpegStream {
public:
    // USM-specific signatures
    static constexpr uint8_t ALP_BYTES[4]  = {0x40, 0x41, 0x4C, 0x50};  // @ALP
    static constexpr uint8_t CRID_BYTES[4] = {0x43, 0x52, 0x49, 0x44};  // CRID
    static constexpr uint8_t SFV_BYTES[4]  = {0x40, 0x53, 0x46, 0x56};  // @SFV (video)
    static constexpr uint8_t SFA_BYTES[4]  = {0x40, 0x53, 0x46, 0x41};  // @SFA (audio)
    static constexpr uint8_t SBT_BYTES[4]  = {0x40, 0x53, 0x42, 0x54};  // @SBT (subtitle)
    static constexpr uint8_t CUE_BYTES[4]  = {0x40, 0x43, 0x55, 0x45};  // @CUE
    static constexpr uint8_t UTF_BYTES[4]  = {0x40, 0x55, 0x54, 0x46};  // @UTF

    // HCA audio signature
    static constexpr uint8_t HCA_SIG_BYTES[4] = {0x48, 0x43, 0x41, 0x00};

    // AIX audio signature
    static constexpr uint8_t AIX_SIG_BYTES[4] = {0x41, 0x49, 0x58, 0x46};

    // Section end markers
    static constexpr uint8_t HEADER_END_BYTES[32] = {
        0x23, 0x48, 0x45, 0x41, 0x44, 0x45, 0x52, 0x20,
        0x45, 0x4E, 0x44, 0x20, 0x20, 0x20, 0x20, 0x20,
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D,
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x00
    };

    static constexpr uint8_t METADATA_END_BYTES[32] = {
        0x23, 0x4D, 0x45, 0x54, 0x41, 0x44, 0x41, 0x54,
        0x41, 0x20, 0x45, 0x4E, 0x44, 0x20, 0x20, 0x20,
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D,
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x00
    };

    static constexpr uint8_t CONTENTS_END_BYTES[32] = {
        0x23, 0x43, 0x4F, 0x4E, 0x54, 0x45, 0x4E, 0x54,
        0x53, 0x20, 0x45, 0x4E, 0x44, 0x20, 0x20, 0x20,
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D,
        0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x3D, 0x00
    };

    CriUsmStream(const std::string& path);
    virtual ~CriUsmStream() = default;

protected:
    // Override packet start bytes (CRID for USM)
    std::vector<uint8_t> get_packet_start_bytes() const override;

    // Packet header/footer size methods
    int get_audio_packet_header_size(std::ifstream& stream, int64_t current_offset) override;
    int get_video_packet_header_size(std::ifstream& stream, int64_t current_offset) override;
    int get_audio_packet_footer_size(std::ifstream& stream, int64_t current_offset) override;
    int get_video_packet_footer_size(std::ifstream& stream, int64_t current_offset) override;

    // Block type identification
    bool is_audio_block(const std::vector<uint8_t>& block_to_check) const override;
    bool is_video_block(const std::vector<uint8_t>& block_to_check) const override;

    // Stream ID extraction
    uint8_t get_stream_id(std::ifstream& stream, int64_t current_offset) override;

    // Final processing (strip headers/footers, detect audio format)
    void do_final_tasks(std::ifstream& source_stream,
                        std::unordered_map<uint32_t, std::shared_ptr<std::ofstream>>& output_files,
                        bool add_header) override;

private:
    // Initialize USM-specific block dictionary
    void init_usm_block_dictionary();

    // Helper to read 2-byte big endian value at relative offset
    uint16_t read_packet_size_at_offset(std::ifstream& stream, int64_t current_offset, int relative_offset);
};

} // namespace usm_toolkit

#endif // USM_STREAM_HPP
