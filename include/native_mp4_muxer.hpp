#ifndef NATIVE_MP4_MUXER_HPP
#define NATIVE_MP4_MUXER_HPP

/**
 * Native MP4 muxer for VP9 video.
 * Creates valid ISO BMFF (MP4) files without ffmpeg.
 */

#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace usm_toolkit {

/**
 * MP4 box structure
 */
struct Mp4Box {
    uint32_t type;        // FourCC
    std::vector<uint8_t> data;
};

/**
 * MP4 muxer configuration
 */
struct Mp4MuxerConfig {
    uint16_t width;
    uint16_t height;
    uint32_t timescale;
    uint32_t framerate_num;
    uint32_t framerate_den;
    
    uint8_t profile;
    uint8_t level;
    uint8_t bit_depth;
    uint8_t chroma_subsampling;
    uint8_t full_range;

    bool has_audio = false;
    uint32_t audio_sample_rate = 0;
    uint16_t audio_channels = 0;
    uint16_t audio_bits_per_sample = 16;
    uint32_t audio_timescale = 0;
};

/**
 * Native MP4 muxer for VP9 video.
 * Creates valid ISO BMFF files without external dependencies.
 */
class NativeMp4Muxer {
public:
    NativeMp4Muxer();
    ~NativeMp4Muxer();
    
    /**
     * Initialize muxer with configuration.
     * Must be called before writing frames.
     */
    bool init(const Mp4MuxerConfig& config);
    
    /**
     * Write a VP9 frame to the MP4 file.
     * frame_data: raw VP9 frame data (without IVF frame header)
     * frame_size: size of frame data
     * timestamp: presentation timestamp in timescale units
     * is_keyframe: true if this is a keyframe (IDR frame)
     */
    bool write_frame(const uint8_t* frame_data, uint32_t frame_size,
                     uint64_t timestamp, bool is_keyframe);
    
    bool write_audio_samples(const uint8_t* pcm_data, uint32_t data_size,
                             uint64_t timestamp);
    
    bool finalize();
    
    /**
     * Get the output file path.
     */
    const std::string& get_output_path() const { return output_path_; }
    
    /**
     * Set output file path.
     */
    void set_output_path(const std::string& path) { output_path_ = path; }

private:
    // Write callback for file I/O
    static int write_callback(int64_t offset, const void* buffer, size_t size, void* token);
    
    // Build ftyp box
    std::vector<uint8_t> build_ftyp();
    
    // Build moov box
    std::vector<uint8_t> build_moov();
    
    // Build mvhd box
    std::vector<uint8_t> build_mvhd();
    
    // Build trak box (video track)
    std::vector<uint8_t> build_trak();
    
    // Build tkhd box
    std::vector<uint8_t> build_tkhd();
    
    // Build mdia box
    std::vector<uint8_t> build_mdia();
    
    // Build mdhd box
    std::vector<uint8_t> build_mdhd();
    
    // Build hdlr box
    std::vector<uint8_t> build_hdlr();
    
    // Build minf box
    std::vector<uint8_t> build_minf();
    
    // Build vmhd box
    std::vector<uint8_t> build_vmhd();
    
    // Build dinf box
    std::vector<uint8_t> build_dinf();
    
    // Build dref box
    std::vector<uint8_t> build_dref();
    
    // Build stbl box
    std::vector<uint8_t> build_stbl();
    
    // Build vpcC box (VP9 codec configuration)
    std::vector<uint8_t> build_vpcC();
    
    // Build stsd box (sample description)
    std::vector<uint8_t> build_stsd();
    
    // Build vp09 sample entry
    std::vector<uint8_t> build_vp09_entry();
    
    // Build stts box (time-to-sample)
    std::vector<uint8_t> build_stts();
    
    // Build stsc box (sample-to-chunk)
    std::vector<uint8_t> build_stsc();
    
    // Build stsz box (sample sizes)
    std::vector<uint8_t> build_stsz();
    
    // Build stco box (chunk offsets)
    std::vector<uint8_t> build_stco();
    
    // Build stss box (sync samples / keyframes)
    std::vector<uint8_t> build_stss();
    
    std::vector<uint8_t> build_audio_trak();
    std::vector<uint8_t> build_audio_stbl();
    std::vector<uint8_t> build_l16_entry();
    
    // Helper: write 4 bytes big endian
    static void write_be32(uint8_t* p, uint32_t v);
    
    // Helper: write 2 bytes big endian
    static void write_be16(uint8_t* p, uint16_t v);
    
    // Helper: write 4 bytes little endian
    static void write_le32(uint8_t* p, uint32_t v);
    
    // Helper: write 8 bytes little endian
    static void write_le64(uint8_t* p, uint64_t v);
    
    // State
    Mp4MuxerConfig config_;
    std::string output_path_;
    FILE* output_file_;
    bool initialized_;
    
    // Frame tracking
    struct FrameInfo {
        uint32_t size;
        uint64_t timestamp;
        bool is_keyframe;
    };
    std::vector<FrameInfo> frames_;
    uint64_t video_bytes_written_;
    uint64_t audio_bytes_written_;
    uint64_t current_offset_;
    
    // Audio tracking
    struct AudioSampleInfo {
        uint32_t size;
    };
    std::vector<AudioSampleInfo> audio_samples_;
};

} // namespace usm_toolkit

#endif // NATIVE_MP4_MUXER_HPP
