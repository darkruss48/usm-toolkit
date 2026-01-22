#include "usm_stream.hpp"
#include "file_utils.hpp"
#include "byte_utils.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cstring>

namespace usm_toolkit {

CriUsmStream::CriUsmStream(const std::string& path)
    : MpegStream(path)
{
    uses_same_id_for_multiple_audio_tracks_ = true;
    file_extension_audio_ = DEFAULT_AUDIO_EXT;
    file_extension_video_ = DEFAULT_VIDEO_EXT;
    
    // Clear base dictionary and add USM-specific blocks
    block_id_dictionary_.clear();
    init_usm_block_dictionary();
}

void CriUsmStream::init_usm_block_dictionary() {
    BlockSizeStruct size_block(PacketSizeType::SizeBytes, 4);
    
    block_id_dictionary_[bytes_to_key(ALP_BYTES)] = size_block;   // @ALP
    block_id_dictionary_[bytes_to_key(CRID_BYTES)] = size_block;  // CRID
    block_id_dictionary_[bytes_to_key(SFV_BYTES)] = size_block;   // @SFV
    block_id_dictionary_[bytes_to_key(SFA_BYTES)] = size_block;   // @SFA
    block_id_dictionary_[bytes_to_key(SBT_BYTES)] = size_block;   // @SBT
    block_id_dictionary_[bytes_to_key(CUE_BYTES)] = size_block;   // @CUE
}

std::vector<uint8_t> CriUsmStream::get_packet_start_bytes() const {
    return std::vector<uint8_t>(CRID_BYTES, CRID_BYTES + 4);
}

uint16_t CriUsmStream::read_packet_size_at_offset(std::ifstream& stream, 
                                                   int64_t current_offset, 
                                                   int relative_offset) {
    auto bytes = FileUtils::read_bytes(stream, current_offset + relative_offset, 2);
    return ByteUtils::read_uint16_be(bytes.data());
}

int CriUsmStream::get_audio_packet_header_size(std::ifstream& stream, int64_t current_offset) {
    return read_packet_size_at_offset(stream, current_offset, 0x8);
}

int CriUsmStream::get_video_packet_header_size(std::ifstream& stream, int64_t current_offset) {
    return read_packet_size_at_offset(stream, current_offset, 0x8);
}

int CriUsmStream::get_audio_packet_footer_size(std::ifstream& stream, int64_t current_offset) {
    return read_packet_size_at_offset(stream, current_offset, 0xA);
}

int CriUsmStream::get_video_packet_footer_size(std::ifstream& stream, int64_t current_offset) {
    return read_packet_size_at_offset(stream, current_offset, 0xA);
}

bool CriUsmStream::is_audio_block(const std::vector<uint8_t>& block_to_check) const {
    if (block_to_check.size() < 4) return false;
    return ByteUtils::compare_bytes(block_to_check.data(), SFA_BYTES, 4);
}

bool CriUsmStream::is_video_block(const std::vector<uint8_t>& block_to_check) const {
    if (block_to_check.size() < 4) return false;
    return ByteUtils::compare_bytes(block_to_check.data(), SFV_BYTES, 4);
}

uint8_t CriUsmStream::get_stream_id(std::ifstream& stream, int64_t current_offset) {
    auto bytes = FileUtils::read_bytes(stream, current_offset + 0xC, 1);
    return bytes[0];
}

void CriUsmStream::do_final_tasks(std::ifstream& source_stream,
                                   std::unordered_map<uint32_t, std::shared_ptr<std::ofstream>>& output_files,
                                   bool add_header) {
    (void)source_stream;
    (void)add_header;
    
    std::vector<uint8_t> header_end_vec(HEADER_END_BYTES, HEADER_END_BYTES + 32);
    std::vector<uint8_t> metadata_end_vec(METADATA_END_BYTES, METADATA_END_BYTES + 32);
    std::vector<uint8_t> contents_end_vec(CONTENTS_END_BYTES, CONTENTS_END_BYTES + 32);
    
    std::string pure_filename = FileUtils::get_filename_without_extension(file_path_);
    std::string directory = FileUtils::get_directory_name(file_path_);
    
    for (auto& pair : output_files) {
        uint32_t stream_id = pair.first;
        auto& output_stream = pair.second;
        
        // Close the output stream first to flush all data
        if (output_stream && output_stream->is_open()) {
            output_stream->close();
        }
        
        // Reconstruct the file name that was used during demuxing
        std::stringstream name_ss;
        name_ss << pure_filename << "_" 
                << std::hex << std::uppercase << std::setw(8) 
                << std::setfill('0') << stream_id;
        std::string base_name = name_ss.str();
        
        // Check if this is an audio or video stream
        bool is_audio = false;
        uint32_t masked_id = stream_id & 0xFFFFFFF0;
        std::vector<uint8_t> masked_bytes(4);
        std::memcpy(masked_bytes.data(), &masked_id, 4);
        
        if (ByteUtils::compare_bytes(masked_bytes.data(), SFA_BYTES, 4)) {
            is_audio = true;
        }
        
        std::string current_ext = is_audio ? file_extension_audio_ : file_extension_video_;
        std::string source_file = FileUtils::combine_path(directory, base_name + current_ext);
        
        if (!FileUtils::file_exists(source_file)) {
            std::cerr << "Warning: Cannot find output file: " << source_file << std::endl;
            continue;
        }
        
        // Open the output file for reading to find markers
        std::ifstream temp_stream(source_file, std::ios::binary);
        if (!temp_stream.is_open()) continue;
        
        int64_t file_size = FileUtils::get_file_size(temp_stream);
        
        // Find header end offset
        int64_t header_end_offset = FileUtils::find_next_offset(temp_stream, 0, header_end_vec);
        int64_t metadata_end_offset = FileUtils::find_next_offset(temp_stream, 0, metadata_end_vec);
        
        int64_t header_size = 0;
        if (metadata_end_offset != -1 && metadata_end_offset > header_end_offset) {
            header_size = metadata_end_offset + 32;
        } else if (header_end_offset != -1) {
            header_size = header_end_offset + 32;
        }
        
        // Find footer/contents end offset  
        int64_t contents_end_offset = FileUtils::find_next_offset(temp_stream, 0, contents_end_vec);
        
        // Calculate footer offset - it's the position of CONTENTS_END minus header_size
        // because after we strip the header, the file positions shift
        int64_t footer_offset;
        int64_t footer_size;
        
        if (contents_end_offset != -1) {
            footer_offset = contents_end_offset - header_size;
            footer_size = file_size - contents_end_offset;
        } else {
            // No footer marker found - no footer to remove
            footer_offset = file_size - header_size;
            footer_size = 0;
        }
        
        temp_stream.close();
        
        // Calculate the actual data size to copy
        int64_t data_size = footer_offset;  // This is already relative to after header removal
        
        // Sanity check
        if (data_size <= 0 || header_size >= file_size) {
            // No processing needed or something is wrong, just rename the file
            std::string dest_file = FileUtils::combine_path(directory, pure_filename + current_ext);
            if (source_file != dest_file) {
                std::filesystem::rename(source_file, dest_file);
            }
            continue;
        }
        
        // Detect audio format if this is an audio block
        std::string file_extension = current_ext;
        
        if (is_audio) {
            std::ifstream check_stream(source_file, std::ios::binary);
            if (check_stream.is_open()) {
                auto check_bytes = FileUtils::read_bytes(check_stream, header_size, 4);
                check_stream.close();
                
                if (check_bytes.size() >= 4) {
                    if (ByteUtils::compare_bytes(check_bytes, 
                            std::vector<uint8_t>(AIX_SIG_BYTES, AIX_SIG_BYTES + 4))) {
                        file_extension = AIX_AUDIO_EXT;
                    } else if (check_bytes[0] == 0x80) {
                        file_extension = DEFAULT_AUDIO_EXT;  // ADX
                    } else if (ByteUtils::compare_bytes(check_bytes,
                            std::vector<uint8_t>(HCA_SIG_BYTES, HCA_SIG_BYTES + 4))) {
                        file_extension = HCA_AUDIO_EXT;
                    } else {
                        file_extension = ".bin";
                    }
                }
                
                final_audio_extension_ = file_extension;
                has_audio_ = true;
            }
        }
        
        // Final destination file name (without stream ID)
        std::string dest_file = FileUtils::combine_path(directory, pure_filename + file_extension);
        
        // Process file: remove header and footer
        std::ifstream in_file(source_file, std::ios::binary);
        std::ofstream out_file(dest_file, std::ios::binary);
        
        if (in_file.is_open() && out_file.is_open()) {
            // Skip header
            in_file.seekg(header_size, std::ios::beg);
            
            // Copy data (data_size = footer_offset which is size after header removed, before footer)
            std::vector<char> buffer(8192);
            int64_t bytes_copied = 0;
            
            while (bytes_copied < data_size) {
                int64_t to_read = std::min(static_cast<int64_t>(buffer.size()), 
                                           data_size - bytes_copied);
                in_file.read(buffer.data(), to_read);
                auto bytes_read = in_file.gcount();
                if (bytes_read <= 0) break;
                out_file.write(buffer.data(), bytes_read);
                bytes_copied += bytes_read;
            }
            
            in_file.close();
            out_file.close();
            
            // Remove the intermediate file with stream ID
            if (source_file != dest_file) {
                std::filesystem::remove(source_file);
            }
        } else {
            if (in_file.is_open()) in_file.close();
            if (out_file.is_open()) out_file.close();
            
            // Fallback: just rename
            if (source_file != dest_file) {
                std::filesystem::rename(source_file, dest_file);
            }
        }
    }
}

} // namespace usm_toolkit
