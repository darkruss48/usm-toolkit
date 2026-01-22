#include "mpeg_stream.hpp"
#include "file_utils.hpp"
#include "byte_utils.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace usm_toolkit {

MpegStream::MpegStream(const std::string& path)
    : file_path_(path)
    , file_extension_audio_(DEFAULT_AUDIO_EXT)
    , file_extension_video_(DEFAULT_VIDEO_EXT)
{
    init_block_dictionary();
}

void MpegStream::init_block_dictionary() {
    // Add slice packets (0x00 to 0xAF)
    BlockSizeStruct slice_block(PacketSizeType::Static, 0x0E);
    for (uint8_t i = 0; i <= 0xAF; ++i) {
        uint8_t slice_bytes[4] = {0x00, 0x00, 0x01, i};
        block_id_dictionary_[bytes_to_key(slice_bytes)] = slice_block;
    }
    
    // System packets
    block_id_dictionary_[bytes_to_key(PACKET_END_BYTES)] = BlockSizeStruct(PacketSizeType::Eof, -1);
    block_id_dictionary_[bytes_to_key(PACKET_START_BYTES)] = BlockSizeStruct(PacketSizeType::Static, 0x0E);
    
    uint8_t system_header[4] = {0x00, 0x00, 0x01, 0xBB};
    block_id_dictionary_[bytes_to_key(system_header)] = BlockSizeStruct(PacketSizeType::SizeBytes, 2);
    
    uint8_t private_stream[4] = {0x00, 0x00, 0x01, 0xBD};
    block_id_dictionary_[bytes_to_key(private_stream)] = BlockSizeStruct(PacketSizeType::SizeBytes, 2);
    
    uint8_t padding_stream[4] = {0x00, 0x00, 0x01, 0xBE};
    block_id_dictionary_[bytes_to_key(padding_stream)] = BlockSizeStruct(PacketSizeType::SizeBytes, 2);
    
    uint8_t private_stream2[4] = {0x00, 0x00, 0x01, 0xBF};
    block_id_dictionary_[bytes_to_key(private_stream2)] = BlockSizeStruct(PacketSizeType::SizeBytes, 2);
    
    // Audio streams (0xC0 to 0xDF)
    for (uint8_t i = 0xC0; i <= 0xDF; ++i) {
        uint8_t audio_bytes[4] = {0x00, 0x00, 0x01, i};
        block_id_dictionary_[bytes_to_key(audio_bytes)] = BlockSizeStruct(PacketSizeType::SizeBytes, 2);
    }
    
    // Video streams (0xE0 to 0xEF)
    for (uint8_t i = 0xE0; i <= 0xEF; ++i) {
        uint8_t video_bytes[4] = {0x00, 0x00, 0x01, i};
        block_id_dictionary_[bytes_to_key(video_bytes)] = BlockSizeStruct(PacketSizeType::SizeBytes, 2);
    }
}

uint32_t MpegStream::bytes_to_key(const uint8_t* bytes) {
    return *reinterpret_cast<const uint32_t*>(bytes);
}

std::vector<uint8_t> MpegStream::get_packet_start_bytes() const {
    return std::vector<uint8_t>(PACKET_START_BYTES, PACKET_START_BYTES + 4);
}

std::vector<uint8_t> MpegStream::get_packet_end_bytes() const {
    return std::vector<uint8_t>(PACKET_END_BYTES, PACKET_END_BYTES + 4);
}

int MpegStream::get_audio_packet_subheader_size(std::ifstream&, int64_t, uint8_t) {
    return 0;
}

int MpegStream::get_audio_packet_footer_size(std::ifstream&, int64_t) {
    return 0;
}

int MpegStream::get_video_packet_footer_size(std::ifstream&, int64_t) {
    return 0;
}

bool MpegStream::is_audio_block(const std::vector<uint8_t>& block_to_check) const {
    if (block_to_check.size() < 4) return false;
    return (block_to_check[3] >= 0xC0 && block_to_check[3] <= 0xDF);
}

bool MpegStream::is_video_block(const std::vector<uint8_t>& block_to_check) const {
    if (block_to_check.size() < 4) return false;
    return (block_to_check[3] >= 0xE0 && block_to_check[3] <= 0xEF);
}

bool MpegStream::is_subpicture_block(const std::vector<uint8_t>& block_to_check) const {
    if (block_to_check.size() < 4) return false;
    return (block_to_check[3] >= 0xE0 && block_to_check[3] <= 0xEF);
}

std::string MpegStream::get_audio_file_extension(std::ifstream&, int64_t) {
    return file_extension_audio_;
}

std::string MpegStream::get_video_file_extension(std::ifstream&, int64_t) {
    return file_extension_video_;
}

uint8_t MpegStream::get_stream_id(std::ifstream&, int64_t) {
    return 0;
}

int64_t MpegStream::get_start_offset(std::ifstream&, int64_t) {
    return 0;
}

void MpegStream::do_final_tasks(std::ifstream&, 
                                std::unordered_map<uint32_t, std::shared_ptr<std::ofstream>>&,
                                bool) {
    // Default: nothing to do
}

void MpegStream::demultiplex_streams(const DemuxOptions& options) {
    std::ifstream fs(file_path_, std::ios::binary);
    if (!fs.is_open()) {
        throw std::runtime_error("Cannot open file: " + file_path_);
    }
    
    int64_t file_size = FileUtils::get_file_size(fs);
    int64_t current_offset = 0;
    
    std::unordered_map<uint32_t, std::shared_ptr<std::ofstream>> stream_output_writers;
    std::unordered_map<uint8_t, std::string> stream_id_file_type;
    
    bool eof_found = false;
    
    // Find first packet
    current_offset = get_start_offset(fs, current_offset);
    current_offset = FileUtils::find_next_offset(fs, current_offset, get_packet_start_bytes());
    
    if (current_offset == -1) {
        throw std::runtime_error("Cannot find Pack Header for file: " + file_path_);
    }
    
    while (current_offset < file_size && !eof_found) {
        // Read current block ID (4 bytes)
        auto current_block_id = FileUtils::read_bytes(fs, current_offset, 4);
        uint32_t current_block_id_val = bytes_to_key(current_block_id.data());
        
        auto it = block_id_dictionary_.find(current_block_id_val);
        if (it == block_id_dictionary_.end()) {
            // Unknown block type - throw error
            std::stringstream ss;
            ss << "Block ID at 0x" << std::hex << std::uppercase 
               << current_offset << " not found in table: 0x" 
               << current_block_id_val;
            throw std::runtime_error(ss.str());
        }
        
        BlockSizeStruct block_struct = it->second;
        
        switch (block_struct.size_type) {
            case PacketSizeType::Static:
                current_offset += block_struct.size;
                break;
                
            case PacketSizeType::Eof:
                eof_found = true;
                break;
                
            case PacketSizeType::SizeBytes: {
                // Read block size
                auto block_size_array = FileUtils::read_bytes(fs, 
                    current_offset + 4, block_struct.size);
                
                uint32_t block_size = 0;
                if (!block_size_is_little_endian_) {
                    // Big endian
                    if (block_struct.size == 4) {
                        block_size = ByteUtils::read_uint32_be(block_size_array.data());
                    } else if (block_struct.size == 2) {
                        block_size = ByteUtils::read_uint16_be(block_size_array.data());
                    } else if (block_struct.size == 1) {
                        block_size = block_size_array[0];
                    }
                } else {
                    // Little endian
                    if (block_struct.size == 4) {
                        block_size = ByteUtils::read_uint32_le(block_size_array.data());
                    } else if (block_struct.size == 2) {
                        block_size = ByteUtils::read_uint16_le(block_size_array.data());
                    } else if (block_struct.size == 1) {
                        block_size = block_size_array[0];
                    }
                }
                
                bool is_audio = is_audio_block(current_block_id);
                bool is_video = is_video_block(current_block_id);
                
                if ((options.extract_audio && is_audio) || 
                    (options.extract_video && is_video)) {
                    
                    uint8_t stream_id = 0;
                    uint32_t current_stream_key;
                    
                    if (is_audio && uses_same_id_for_multiple_audio_tracks_) {
                        stream_id = get_stream_id(fs, current_offset);
                        current_stream_key = stream_id | current_block_id_val;
                    } else {
                        current_stream_key = current_block_id_val;
                    }
                    
                    // Create output file if not exists
                    if (stream_output_writers.find(current_stream_key) == stream_output_writers.end()) {
                        // Build output file name
                        std::stringstream name_ss;
                        name_ss << FileUtils::get_filename_without_extension(file_path_)
                                << "_" << std::hex << std::uppercase << std::setw(8) 
                                << std::setfill('0') << current_stream_key;
                        
                        std::string output_file_name;
                        std::string extension;
                        
                        if (is_audio) {
                            extension = get_audio_file_extension(fs, current_offset);
                            stream_id_file_type[stream_id] = extension;
                        } else {
                            extension = get_video_file_extension(fs, current_offset);
                            file_extension_video_ = extension;
                        }
                        
                        output_file_name = name_ss.str() + extension;
                        output_file_name = FileUtils::combine_path(
                            FileUtils::get_directory_name(file_path_), 
                            output_file_name);
                        
                        auto writer = std::make_shared<std::ofstream>(
                            output_file_name, std::ios::binary);
                        stream_output_writers[current_stream_key] = writer;
                    }
                    
                    // Write the block data
                    if (is_audio) {
                        int header_size = get_audio_packet_header_size(fs, current_offset) +
                                         get_audio_packet_subheader_size(fs, current_offset, stream_id);
                        int footer_size = get_audio_packet_footer_size(fs, current_offset);
                        int cut_size = block_size - header_size - footer_size;
                        
                        if (cut_size > 0) {
                            auto data = FileUtils::read_bytes(fs, 
                                current_offset + 4 + block_struct.size + header_size,
                                cut_size);
                            stream_output_writers[current_stream_key]->write(
                                reinterpret_cast<char*>(data.data()), cut_size);
                        }
                    } else {
                        int header_size = get_video_packet_header_size(fs, current_offset);
                        int footer_size = get_video_packet_footer_size(fs, current_offset);
                        int cut_size = block_size - header_size - footer_size;
                        
                        if (cut_size > 0) {
                            auto data = FileUtils::read_bytes(fs,
                                current_offset + 4 + block_struct.size + header_size,
                                cut_size);
                            stream_output_writers[current_stream_key]->write(
                                reinterpret_cast<char*>(data.data()), cut_size);
                        }
                    }
                }
                
                // Move to next block
                current_offset += 4 + block_struct.size + block_size;
                break;
            }
        }
    }
    
    // Perform final tasks
    do_final_tasks(fs, stream_output_writers, options.add_header);
    
    // Close all writers
    for (auto& pair : stream_output_writers) {
        if (pair.second && pair.second->is_open()) {
            pair.second->close();
        }
    }
    
    fs.close();
}

} // namespace usm_toolkit
