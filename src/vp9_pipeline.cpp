#include "vp9_pipeline.hpp"
#include "byte_utils.hpp"
#include "file_utils.hpp"
#include "usm_stream.hpp"
#include "adx_decoder.hpp"
#include "hca_decoder.hpp"
#include <fstream>
#include <cstring>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

namespace usm_toolkit {

Vp9Pipeline::Vp9Pipeline() {}
Vp9Pipeline::~Vp9Pipeline() {}

Vp9PipelineResult Vp9Pipeline::execute(const Vp9PipelineConfig& config) {
    Vp9PipelineResult result;

    std::string pure_name = FileUtils::get_filename_without_extension(config.input_path);
    std::string directory = FileUtils::get_directory_name(config.input_path);

    CriUsmStream usm_stream(config.input_path);

    DemuxOptions options;
    options.extract_video = true;
    options.extract_audio = true;
    options.add_header = false;

    std::cout << "Demuxing VP9 stream..." << std::endl;
    usm_stream.demultiplex_streams(options);

    std::string extracted_video = FileUtils::combine_path(directory, pure_name + ".m2v");

    if (!FileUtils::file_exists(extracted_video)) {
        result.error_message = "No video stream extracted";
        return result;
    }

    std::ifstream ivf_file(extracted_video, std::ios::binary);
    if (!ivf_file.is_open()) {
        result.error_message = "Cannot open extracted video: " + extracted_video;
        return result;
    }

    uint8_t sig[4];
    ivf_file.read(reinterpret_cast<char*>(sig), 4);
    if (!IvfParser::is_ivf(sig, 4)) {
        result.error_message = "Extracted video is not IVF format";
        ivf_file.close();
        return result;
    }

    ivf_file.seekg(0);

    IvfHeader ivf_header;
    Vp9CodecConfig codec_config;
    if (!parse_ivf_header_stream(ivf_file, ivf_header, codec_config)) {
        result.error_message = "Failed to parse IVF header";
        ivf_file.close();
        return result;
    }

    if (!IvfParser::is_vp9(ivf_header)) {
        result.error_message = "Not VP9 (FourCC: " + IvfParser::get_fourcc_string(ivf_header) + ")";
        ivf_file.close();
        return result;
    }

    result.width = config.width > 0 ? config.width : ivf_header.width;
    result.height = config.height > 0 ? config.height : ivf_header.height;

    std::cout << "VP9: " << result.width << "x" << result.height
              << ", " << ivf_header.frame_count << " frames"
              << ", fps=" << IvfParser::get_framerate(ivf_header) << std::endl;

    Mp4MuxerConfig muxer_config;
    memset(&muxer_config, 0, sizeof(muxer_config));
    muxer_config.width = result.width;
    muxer_config.height = result.height;
    muxer_config.timescale = config.timescale;
    muxer_config.framerate_num = ivf_header.framerate_n;
    muxer_config.framerate_den = ivf_header.framerate_d;
    muxer_config.profile = codec_config.profile;
    muxer_config.level = codec_config.level;
    muxer_config.bit_depth = codec_config.bit_depth;
    muxer_config.chroma_subsampling = codec_config.chroma_subsampling;
    muxer_config.full_range = codec_config.full_range;

    bool has_audio = false;
    std::string audio_path;
    std::string audio_ext;
    size_t audio_data_offset = 0;
    if (usm_stream.has_audio()) {
        audio_ext = usm_stream.get_final_audio_extension();
        audio_path = FileUtils::combine_path(directory, pure_name + audio_ext);
        if (FileUtils::file_exists(audio_path)) {
            FILE* af = fopen(audio_path.c_str(), "rb");
            if (af) {
                uint8_t hdr_buf[1024];
                size_t n = fread(hdr_buf, 1, sizeof(hdr_buf), af);
                fclose(af);
                
                size_t adx_offset = 0;
                for (size_t i = 0; i + 4 <= n; i++) {
                    if (hdr_buf[i] == 0x80 && hdr_buf[i+1] == 0x00) {
                        uint16_t copy_off = (static_cast<uint16_t>(hdr_buf[i+3]) << 8) | hdr_buf[i+4];
                        if (i + copy_off + 5 <= n &&
                            hdr_buf[i + copy_off] == '(' && hdr_buf[i + copy_off + 1] == 'c') {
                            adx_offset = i;
                            break;
                        }
                    }
                }
                
                if (audio_ext == ".adx" && adx_offset + 18 < n) {
                    AdxHeader adx_hdr;
                    if (AdxDecoder::parse_header(hdr_buf + adx_offset, n - adx_offset, adx_hdr)) {
                        muxer_config.has_audio = true;
                        muxer_config.audio_sample_rate = adx_hdr.sample_rate;
                        muxer_config.audio_channels = adx_hdr.channels;
                        muxer_config.audio_bits_per_sample = 16;
                        muxer_config.audio_timescale = adx_hdr.sample_rate;
                        has_audio = true;
                        audio_data_offset = adx_offset;
                        std::cout << "ADX audio: " << adx_hdr.sample_rate << "Hz, "
                                  << (int)adx_hdr.channels << "ch (offset=" << adx_offset << ")" << std::endl;
                    }
                } else if (audio_ext == ".hca" && HcaDecoder::is_hca(hdr_buf, n)) {
                    HcaHeader hca_hdr;
                    if (HcaDecoder::parse_header(hdr_buf, n, hca_hdr)) {
                        muxer_config.has_audio = true;
                        muxer_config.audio_sample_rate = hca_hdr.sample_rate;
                        muxer_config.audio_channels = static_cast<uint16_t>(hca_hdr.channel_count);
                        muxer_config.audio_bits_per_sample = 16;
                        muxer_config.audio_timescale = hca_hdr.sample_rate;
                        has_audio = true;
                        std::cout << "HCA audio: " << hca_hdr.sample_rate << "Hz, "
                                  << hca_hdr.channel_count << "ch" << std::endl;
                    }
                }
            }
        }
    }

    NativeMp4Muxer muxer;
    muxer.set_output_path(config.output_path);

    if (!muxer.init(muxer_config)) {
        result.error_message = "Failed to initialize MP4 muxer";
        ivf_file.close();
        return result;
    }

    uint32_t frames_written = 0;
    uint64_t frame_duration_ticks = 0;
    if (muxer_config.framerate_den > 0 && muxer_config.framerate_num > 0) {
        frame_duration_ticks = static_cast<uint64_t>(muxer_config.framerate_den) *
                               muxer_config.timescale / muxer_config.framerate_num;
    } else {
        frame_duration_ticks = muxer_config.timescale / 30;
    }

    uint64_t current_timestamp = 0;

    for (uint32_t i = 0; i < ivf_header.frame_count; i++) {
        uint8_t fh[12];
        ivf_file.read(reinterpret_cast<char*>(fh), 12);
        if (ivf_file.gcount() != 12) {
            std::cerr << "Truncated at frame " << i << "/" << ivf_header.frame_count << std::endl;
            break;
        }

        IvfFrameHeader frame_header;
        IvfParser::parse_frame_header(fh, 12, frame_header);

        std::vector<uint8_t> frame_data(frame_header.frame_size);
        ivf_file.read(reinterpret_cast<char*>(frame_data.data()), frame_header.frame_size);
        if (ivf_file.gcount() != static_cast<std::streamsize>(frame_header.frame_size)) {
            std::cerr << "Truncated frame data at frame " << i << std::endl;
            break;
        }

        bool is_keyframe = (i == 0);
        if (frame_data.size() >= 3) {
            if (frame_data[0] == 0x49 && frame_data[1] == 0x83 && frame_data[2] == 0x42) {
                is_keyframe = true;
            }
        }

        muxer.write_frame(frame_data.data(), static_cast<uint32_t>(frame_data.size()),
                          current_timestamp, is_keyframe);
        frames_written++;
        current_timestamp += frame_duration_ticks;
    }

    ivf_file.close();

    if (has_audio && !audio_path.empty()) {
        std::string temp_wav = FileUtils::combine_path(directory, pure_name + "_tmp_audio.wav");
        bool decoded = false;

        if (audio_ext == ".adx") {
            std::cout << "Decoding ADX \xe2\x86\x92 WAV..." << std::endl;
            AdxDecoder decoder;
            decoded = decoder.decode_file(audio_path, temp_wav, audio_data_offset);
        } else if (audio_ext == ".hca") {
            std::cout << "Decoding HCA → WAV..." << std::endl;
            HcaDecoder decoder;
            decoded = decoder.decode_file(audio_path, temp_wav);
        }

        if (decoded && FileUtils::file_exists(temp_wav)) {
            FILE* wf = fopen(temp_wav.c_str(), "rb");
            if (wf) {
                fseek(wf, 44, SEEK_SET);

                fseek(wf, 0, SEEK_END);
                long pcm_size = ftell(wf) - 44;
                fseek(wf, 44, SEEK_SET);

                if (pcm_size > 0) {
                    std::vector<uint8_t> pcm_data(pcm_size);
                    size_t read = fread(pcm_data.data(), 1, pcm_size, wf);
                    if (read > 0) {
                        muxer.write_audio_samples(pcm_data.data(),
                            static_cast<uint32_t>(read), 0);
                        std::cout << "Audio: " << read << " bytes PCM written to MP4" << std::endl;
                    }
                }
                fclose(wf);
            }
            fs::remove(temp_wav);
        } else {
            std::cerr << "Audio decode failed, continuing without audio" << std::endl;
        }
        fs::remove(audio_path);
    }

    if (!muxer.finalize()) {
        result.error_message = "Failed to finalize MP4";
        return result;
    }

    result.frames_written = frames_written;
    double framerate = IvfParser::get_framerate(ivf_header);
    if (framerate > 0 && frames_written > 0) {
        result.duration_ms = static_cast<uint64_t>((frames_written / framerate) * 1000.0);
    }

    fs::remove(extracted_video);

    result.success = true;
    std::cout << "Done: " << config.output_path << " (" << frames_written << " frames)" << std::endl;
    return result;
}

bool Vp9Pipeline::is_vp9_usm(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    int64_t sfv_offset = find_sfv_block(file, 0);
    if (sfv_offset == -1) return false;

    uint8_t size_bytes[4];
    file.seekg(sfv_offset + 4);
    file.read(reinterpret_cast<char*>(size_bytes), 4);
    uint32_t sfv_block_size = read_le32(size_bytes);

    int64_t ivf_offset = find_ivf_data(file, sfv_offset, sfv_block_size);
    if (ivf_offset == -1) return false;

    IvfHeader header;
    Vp9CodecConfig cfg;
    if (!parse_ivf_header(file, ivf_offset, header, cfg)) return false;

    return IvfParser::is_vp9(header);
}

Vp9PipelineResult Vp9Pipeline::probe(const std::string& path) {
    Vp9PipelineResult result;

    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        result.error_message = "Cannot open file: " + path;
        return result;
    }

    int64_t sfv_offset = find_sfv_block(file, 0);
    if (sfv_offset == -1) {
        result.error_message = "No @SFV video block found";
        return result;
    }

    uint8_t size_bytes[4];
    file.seekg(sfv_offset + 4);
    file.read(reinterpret_cast<char*>(size_bytes), 4);
    uint32_t sfv_block_size = read_le32(size_bytes);

    int64_t ivf_offset = find_ivf_data(file, sfv_offset, sfv_block_size);
    if (ivf_offset == -1) {
        result.error_message = "No IVF data found in @SFV block";
        return result;
    }

    IvfHeader header;
    Vp9CodecConfig codec_config;
    if (!parse_ivf_header(file, ivf_offset, header, codec_config)) {
        result.error_message = "Failed to parse IVF header";
        return result;
    }

    result.width = header.width;
    result.height = header.height;
    result.frames_written = header.frame_count;

    double framerate = IvfParser::get_framerate(header);
    if (framerate > 0 && header.frame_count > 0) {
        result.duration_ms = static_cast<uint64_t>((header.frame_count / framerate) * 1000.0);
    }

    result.success = true;
    return result;
}

int64_t Vp9Pipeline::find_sfv_block(std::ifstream& file, int64_t start_offset) {
    file.seekg(start_offset);
    const int64_t file_size = file.seekg(0, std::ios::end).tellg();
    file.seekg(start_offset);

    uint8_t sig[4] = {0x40, 0x53, 0x46, 0x56};

    const int64_t chunk_size = 4096;
    std::vector<char> buffer(chunk_size);

    while (file.tellg() < file_size) {
        int64_t pos = file.tellg();
        int64_t to_read = std::min(chunk_size, file_size - pos);

        file.read(buffer.data(), to_read);
        auto bytes_read = file.gcount();

        if (bytes_read == 0) break;

        for (int64_t i = 0; i <= bytes_read - 4; i++) {
            if (memcmp(buffer.data() + i, sig, 4) == 0) {
                return pos + i;
            }
        }

        if (pos + bytes_read < file_size) {
            file.seekg(pos + bytes_read - 3);
        }
    }

    return -1;
}

int64_t Vp9Pipeline::find_ivf_data(std::ifstream& file, int64_t sfv_offset, int64_t sfv_size) {
    uint8_t ivf_sig[4] = {0x44, 0x4B, 0x49, 0x46};

    int64_t search_end = sfv_offset + sfv_size;
    int64_t search_start = sfv_offset + 8;

    const int64_t chunk_size = 4096;
    std::vector<char> buffer(chunk_size);

    file.seekg(search_start);

    while (file.tellg() < search_end) {
        int64_t pos = file.tellg();
        int64_t to_read = std::min(chunk_size, search_end - pos);

        file.read(buffer.data(), to_read);
        auto bytes_read = file.gcount();

        if (bytes_read == 0) break;

        for (int64_t i = 0; i <= bytes_read - 4; i++) {
            if (memcmp(buffer.data() + i, ivf_sig, 4) == 0) {
                return pos + i;
            }
        }

        if (pos + bytes_read < search_end) {
            file.seekg(pos + bytes_read - 3);
        }
    }

    return -1;
}

bool Vp9Pipeline::parse_ivf_header(std::ifstream& file, int64_t offset,
                                    IvfHeader& header, Vp9CodecConfig& codec_config) {
    file.seekg(offset);

    uint8_t header_data[IvfParser::IVF_HEADER_SIZE];
    file.read(reinterpret_cast<char*>(header_data), IvfParser::IVF_HEADER_SIZE);

    if (file.gcount() != IvfParser::IVF_HEADER_SIZE) return false;

    if (!IvfParser::parse_header(header_data, IvfParser::IVF_HEADER_SIZE, header)) {
        return false;
    }

    codec_config = IvfParser::get_codec_config(header);
    return true;
}

bool Vp9Pipeline::parse_ivf_header_stream(std::ifstream& file,
                                           IvfHeader& header, Vp9CodecConfig& codec_config) {
    uint8_t header_data[IvfParser::IVF_HEADER_SIZE];
    file.read(reinterpret_cast<char*>(header_data), IvfParser::IVF_HEADER_SIZE);

    if (file.gcount() != IvfParser::IVF_HEADER_SIZE) return false;

    if (!IvfParser::parse_header(header_data, IvfParser::IVF_HEADER_SIZE, header)) {
        return false;
    }

    codec_config = IvfParser::get_codec_config(header);
    return true;
}

uint32_t Vp9Pipeline::read_le32(const uint8_t* data) {
    return static_cast<uint32_t>(data[0]) |
           (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) |
           (static_cast<uint32_t>(data[3]) << 24);
}

uint16_t Vp9Pipeline::read_le16(const uint8_t* data) {
    return static_cast<uint16_t>(data[0]) |
           (static_cast<uint16_t>(data[1]) << 8);
}

} // namespace usm_toolkit
