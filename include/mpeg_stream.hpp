#ifndef MPEG_STREAM_HPP
#define MPEG_STREAM_HPP

#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <cstdint>
#include <memory>
#include "usm_toolkit.hpp"

namespace usm_toolkit {

/**
 * Packet size types for block parsing
 */
enum class PacketSizeType {
    Static,     // Fixed size block
    SizeBytes,  // Size is read from following bytes
    Eof         // End of file marker
};

/**
 * Block size structure for packet parsing
 */
struct BlockSizeStruct {
    PacketSizeType size_type;
    int size;

    BlockSizeStruct() : size_type(PacketSizeType::Static), size(0) {}
    BlockSizeStruct(PacketSizeType type, int sz) : size_type(type), size(sz) {}
};

/**
 * Base MPEG stream parser class
 * Ported from VGMToolbox MpegStream.cs
 */
class MpegStream {
public:
    // Standard MPEG markers
    static constexpr uint8_t PACKET_START_BYTES[4] = {0x00, 0x00, 0x01, 0xBA};
    static constexpr uint8_t PACKET_END_BYTES[4] = {0x00, 0x00, 0x01, 0xB9};

    MpegStream(const std::string& path);
    virtual ~MpegStream() = default;

    // Main demultiplexing function
    virtual void demultiplex_streams(const DemuxOptions& options);

    // Accessors
    const std::string& get_file_path() const { return file_path_; }
    const std::string& get_file_extension_audio() const { return file_extension_audio_; }
    const std::string& get_file_extension_video() const { return file_extension_video_; }
    bool has_audio() const { return has_audio_; }
    const std::string& get_final_audio_extension() const { return final_audio_extension_; }

    void set_final_audio_extension(const std::string& ext) { final_audio_extension_ = ext; }

protected:
    // Block ID dictionary for parsing
    std::unordered_map<uint32_t, BlockSizeStruct> block_id_dictionary_;

    std::string file_path_;
    std::string file_extension_audio_;
    std::string file_extension_video_;
    bool has_audio_ = false;
    std::string final_audio_extension_;

    bool uses_same_id_for_multiple_audio_tracks_ = false;
    bool subtitle_extraction_supported_ = false;
    bool block_size_is_little_endian_ = false;

    // Virtual methods for customization by derived classes
    virtual std::vector<uint8_t> get_packet_start_bytes() const;
    virtual std::vector<uint8_t> get_packet_end_bytes() const;

    virtual int get_audio_packet_header_size(std::ifstream& stream, int64_t current_offset) = 0;
    virtual int get_video_packet_header_size(std::ifstream& stream, int64_t current_offset) = 0;
    virtual int get_audio_packet_subheader_size(std::ifstream& stream, int64_t current_offset, uint8_t stream_id);
    virtual int get_audio_packet_footer_size(std::ifstream& stream, int64_t current_offset);
    virtual int get_video_packet_footer_size(std::ifstream& stream, int64_t current_offset);

    virtual bool is_audio_block(const std::vector<uint8_t>& block_to_check) const;
    virtual bool is_video_block(const std::vector<uint8_t>& block_to_check) const;
    virtual bool is_subpicture_block(const std::vector<uint8_t>& block_to_check) const;

    virtual std::string get_audio_file_extension(std::ifstream& stream, int64_t current_offset);
    virtual std::string get_video_file_extension(std::ifstream& stream, int64_t current_offset);

    virtual uint8_t get_stream_id(std::ifstream& stream, int64_t current_offset);
    virtual int64_t get_start_offset(std::ifstream& stream, int64_t current_offset);

    virtual void do_final_tasks(std::ifstream& source_stream,
                                std::unordered_map<uint32_t, std::shared_ptr<std::ofstream>>& output_files,
                                bool add_header);

    // Helper to initialize block dictionary
    void init_block_dictionary();

    // Convert bytes to uint32 for dictionary key
    static uint32_t bytes_to_key(const uint8_t* bytes);
};

} // namespace usm_toolkit

#endif // MPEG_STREAM_HPP
