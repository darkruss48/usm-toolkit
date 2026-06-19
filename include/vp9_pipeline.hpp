#ifndef VP9_PIPELINE_HPP
#define VP9_PIPELINE_HPP

/**
 * VP9 video pipeline.
 * Converts VP9 video from USM files to native MP4 format.
 * No ffmpeg dependency - direct demux + mux.
 */

#include <string>
#include <vector>
#include <cstdint>
#include <fstream>
#include "ivf_parser.hpp"
#include "native_mp4_muxer.hpp"

namespace usm_toolkit {

/**
 * VP9 pipeline configuration
 */
struct Vp9PipelineConfig {
    std::string input_path;       // Path to USM file
    std::string output_path;      // Path to output MP4 file
    
    // Optional: override video parameters
    uint16_t width = 0;           // 0 = auto-detect from IVF
    uint16_t height = 0;
    uint32_t timescale = 90000;   // Default VP9 timescale
};

/**
 * VP9 pipeline status/result
 */
struct Vp9PipelineResult {
    bool success = false;
    uint32_t frames_written = 0;
    uint64_t duration_ms = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    std::string error_message;
};

/**
 * VP9 video pipeline
 * 
 * Reads VP9 video from USM file:
 * 1. Parses @SFV blocks to find IVF data
 * 2. Extracts VP9 frames from IVF container
 * 3. Muxes directly to MP4 using native muxer
 * 
 * No ffmpeg dependency, no temporary files.
 */
class Vp9Pipeline {
public:
    Vp9Pipeline();
    ~Vp9Pipeline();
    
    /**
     * Execute the pipeline.
     * Reads input USM file and writes output MP4.
     */
    Vp9PipelineResult execute(const Vp9PipelineConfig& config);
    
    /**
     * Check if a USM file contains VP9 video.
     * Returns true if the file has @SFV blocks with IVF/VP9 data.
     */
    static bool is_vp9_usm(const std::string& path);
    
    /**
     * Get video info from a USM file without converting.
     */
    static Vp9PipelineResult probe(const std::string& path);

private:
    static int64_t find_ivf_data(std::ifstream& file, int64_t sfv_offset, int64_t sfv_size);
    
    static bool parse_ivf_header(std::ifstream& file, int64_t offset, 
                          IvfHeader& header, Vp9CodecConfig& codec_config);
    
    static bool parse_ivf_header_stream(std::ifstream& file,
                                         IvfHeader& header, Vp9CodecConfig& codec_config);
    
    static int64_t find_sfv_block(std::ifstream& file, int64_t start_offset);
    
    static uint32_t read_le32(const uint8_t* data);
    
    static uint16_t read_le16(const uint8_t* data);
};

} // namespace usm_toolkit

#endif // VP9_PIPELINE_HPP
