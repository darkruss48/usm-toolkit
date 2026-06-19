#include "native_mp4_muxer.hpp"
#include <cstdio>
#include <cstring>
#include <cmath>

namespace usm_toolkit {

NativeMp4Muxer::NativeMp4Muxer()
    : output_file_(nullptr)
    , initialized_(false)
    , video_bytes_written_(0)
    , audio_bytes_written_(0)
    , current_offset_(0)
{
}

NativeMp4Muxer::~NativeMp4Muxer() {
    if (output_file_) {
        fclose(output_file_);
    }
}

bool NativeMp4Muxer::init(const Mp4MuxerConfig& config) {
    config_ = config;
    frames_.clear();
    audio_samples_.clear();
    video_bytes_written_ = 0;
    audio_bytes_written_ = 0;
    current_offset_ = 0;
    
    output_file_ = fopen(output_path_.c_str(), "wb");
    if (!output_file_) return false;
    
    auto ftyp = build_ftyp();
    fwrite(ftyp.data(), 1, ftyp.size(), output_file_);
    current_offset_ += ftyp.size();
    
    uint8_t mdat_header[8] = {0, 0, 0, 0, 'm', 'd', 'a', 't'};
    fwrite(mdat_header, 1, 8, output_file_);
    current_offset_ += 8;
    
    initialized_ = true;
    return true;
}

bool NativeMp4Muxer::write_frame(const uint8_t* frame_data, uint32_t frame_size,
                                  uint64_t timestamp, bool is_keyframe) {
    if (!initialized_) return false;
    
    FrameInfo info;
    info.size = frame_size;
    info.timestamp = timestamp;
    info.is_keyframe = is_keyframe;
    frames_.push_back(info);
    
    fwrite(frame_data, 1, frame_size, output_file_);
    video_bytes_written_ += frame_size;
    
    return true;
}

bool NativeMp4Muxer::write_audio_samples(const uint8_t* pcm_data, uint32_t data_size,
                                          uint64_t timestamp) {
    if (!initialized_) return false;
    
    AudioSampleInfo info;
    info.size = data_size;
    audio_samples_.push_back(info);
    
    fwrite(pcm_data, 1, data_size, output_file_);
    audio_bytes_written_ += data_size;
    
    return true;
}

bool NativeMp4Muxer::finalize() {
    if (!initialized_ || !output_file_) return false;
    
    uint32_t mdat_size = 8 + static_cast<uint32_t>(video_bytes_written_) 
                           + static_cast<uint32_t>(audio_bytes_written_);
    fseek(output_file_, 20, SEEK_SET);
    uint8_t mdat_size_buf[4];
    write_be32(mdat_size_buf, mdat_size);
    fwrite(mdat_size_buf, 4, 1, output_file_);
    
    fseek(output_file_, 0, SEEK_END);
    
    auto moov = build_moov();
    fwrite(moov.data(), 1, moov.size(), output_file_);
    
    fclose(output_file_);
    output_file_ = nullptr;
    initialized_ = false;
    
    return true;
}

std::vector<uint8_t> NativeMp4Muxer::build_ftyp() {
    std::vector<uint8_t> box;
    box.resize(20);
    
    // Box size (20 bytes)
    write_be32(box.data(), 20);
    
    // Box type: 'ftyp'
    box[4] = 'f'; box[5] = 't'; box[6] = 'y'; box[7] = 'p';
    
    // Major brand: 'isom'
    box[8] = 'i'; box[9] = 's'; box[10] = 'o'; box[11] = 'm';
    
    // Minor version: 0
    write_be32(box.data() + 12, 0);
    
    // Compatible brands: 'isom', 'mp41'
    box[16] = 'i'; box[17] = 's'; box[18] = 'o'; box[19] = 'm';
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_moov() {
    auto mvhd = build_mvhd();
    auto trak = build_trak();
    
    std::vector<uint8_t> audio_trak;
    if (config_.has_audio && !audio_samples_.empty()) {
        audio_trak = build_audio_trak();
    }
    
    uint32_t moov_size = 8 + static_cast<uint32_t>(mvhd.size() + trak.size() + audio_trak.size());
    
    std::vector<uint8_t> box(moov_size);
    
    write_be32(box.data(), moov_size);
    box[4] = 'm'; box[5] = 'o'; box[6] = 'o'; box[7] = 'v';
    
    size_t offset = 8;
    memcpy(box.data() + offset, mvhd.data(), mvhd.size());
    offset += mvhd.size();
    
    memcpy(box.data() + offset, trak.data(), trak.size());
    offset += trak.size();
    
    if (!audio_trak.empty()) {
        memcpy(box.data() + offset, audio_trak.data(), audio_trak.size());
    }
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_mvhd() {
    std::vector<uint8_t> box;
    box.resize(108);  // mvhd version 0
    
    // Box size (108 bytes)
    write_be32(box.data(), 108);
    
    // Box type: 'mvhd'
    box[4] = 'm'; box[5] = 'v'; box[6] = 'h'; box[7] = 'd';
    
    // Version 0, flags 0
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    
    // Creation time (0)
    write_be32(box.data() + 12, 0);
    
    // Modification time (0)
    write_be32(box.data() + 16, 0);
    
    // Timescale
    write_be32(box.data() + 20, config_.timescale);
    
    // Duration (calculate from frames)
    uint64_t duration = 0;
    if (!frames_.empty()) {
        duration = frames_.back().timestamp + 
                   (config_.timescale * config_.framerate_den / config_.framerate_num);
    }
    write_be32(box.data() + 24, static_cast<uint32_t>(duration));
    
    // Rate (1.0 = 0x00010000)
    write_be32(box.data() + 76, 0x00010000);
    
    // Volume (1.0 = 0x0100)
    write_be16(box.data() + 80, 0x0100);
    
    // Matrix (identity)
    box[88] = 0; box[89] = 0x01; box[90] = 0; box[91] = 0;
    box[92] = 0; box[93] = 0; box[94] = 0; box[95] = 0;
    box[96] = 0; box[97] = 0; box[98] = 0; box[99] = 0;
    box[100] = 0; box[101] = 0; box[102] = 0; box[103] = 0;
    box[104] = 0; box[105] = 0; box[106] = 0; box[107] = 0x40;
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_trak() {
    auto tkhd = build_tkhd();
    auto mdia = build_mdia();
    
    uint32_t trak_size = 8 + tkhd.size() + mdia.size();
    
    std::vector<uint8_t> box;
    box.resize(trak_size);
    
    write_be32(box.data(), trak_size);
    box[4] = 't'; box[5] = 'r'; box[6] = 'a'; box[7] = 'k';
    
    size_t offset = 8;
    memcpy(box.data() + offset, tkhd.data(), tkhd.size());
    offset += tkhd.size();
    memcpy(box.data() + offset, mdia.data(), mdia.size());
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_tkhd() {
    std::vector<uint8_t> box;
    box.resize(92);  // tkhd version 0
    
    write_be32(box.data(), 92);
    box[4] = 't'; box[5] = 'k'; box[6] = 'h'; box[7] = 'd';
    
    // Version 0, flags = 0x000003 (track_enabled | track_in_movie)
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 3;
    
    // Creation/modification time
    write_be32(box.data() + 12, 0);
    write_be32(box.data() + 16, 0);
    
    // Track ID
    write_be32(box.data() + 20, 1);
    
    // Reserved
    write_be32(box.data() + 24, 0);
    
    // Duration
    uint64_t duration = 0;
    if (!frames_.empty()) {
        duration = frames_.back().timestamp +
                   (config_.timescale * config_.framerate_den / config_.framerate_num);
    }
    write_be32(box.data() + 28, static_cast<uint32_t>(duration));
    
    // Reserved
    write_be32(box.data() + 32, 0);
    write_be32(box.data() + 36, 0);
    
    // Layer
    write_be16(box.data() + 40, 0);
    
    // Alternate group
    write_be16(box.data() + 42, 0);
    
    // Volume (0 for video)
    write_be16(box.data() + 44, 0);
    
    // Reserved
    write_be16(box.data() + 46, 0);
    
    // Matrix (identity)
    box[48] = 0; box[49] = 0x01; box[50] = 0; box[51] = 0;
    box[52] = 0; box[53] = 0; box[54] = 0; box[55] = 0;
    box[56] = 0; box[57] = 0; box[58] = 0; box[59] = 0;
    box[60] = 0; box[61] = 0; box[62] = 0; box[63] = 0;
    box[64] = 0; box[65] = 0; box[66] = 0; box[67] = 0;
    
    // Width (16.16 fixed point)
    write_be32(box.data() + 68, static_cast<uint32_t>(config_.width) << 16);
    
    // Height (16.16 fixed point)
    write_be32(box.data() + 72, static_cast<uint32_t>(config_.height) << 16);
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_mdia() {
    auto mdhd = build_mdhd();
    auto hdlr = build_hdlr();
    auto minf = build_minf();
    
    uint32_t mdia_size = 8 + mdhd.size() + hdlr.size() + minf.size();
    
    std::vector<uint8_t> box;
    box.resize(mdia_size);
    
    write_be32(box.data(), mdia_size);
    box[4] = 'm'; box[5] = 'd'; box[6] = 'i'; box[7] = 'a';
    
    size_t offset = 8;
    memcpy(box.data() + offset, mdhd.data(), mdhd.size());
    offset += mdhd.size();
    memcpy(box.data() + offset, hdlr.data(), hdlr.size());
    offset += hdlr.size();
    memcpy(box.data() + offset, minf.data(), minf.size());
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_mdhd() {
    std::vector<uint8_t> box;
    box.resize(32);  // mdhd version 0
    
    write_be32(box.data(), 32);
    box[4] = 'm'; box[5] = 'd'; box[6] = 'h'; box[7] = 'd';
    
    // Version 0, flags 0
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    
    // Creation/modification time
    write_be32(box.data() + 12, 0);
    write_be32(box.data() + 16, 0);
    
    // Timescale
    write_be32(box.data() + 20, config_.timescale);
    
    // Duration
    uint64_t duration = 0;
    if (!frames_.empty()) {
        duration = frames_.back().timestamp +
                   (config_.timescale * config_.framerate_den / config_.framerate_num);
    }
    write_be32(box.data() + 24, static_cast<uint32_t>(duration));
    
    // Language (und = 0x55C4)
    write_be16(box.data() + 28, 0x55C4);
    
    // Quality (0)
    write_be16(box.data() + 30, 0);
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_hdlr() {
    std::vector<uint8_t> box;
    box.resize(45);
    
    write_be32(box.data(), 45);
    box[4] = 'h'; box[5] = 'd'; box[6] = 'l'; box[7] = 'r';
    
    // Version 0, flags 0
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    
    // Pre-defined (0)
    write_be32(box.data() + 12, 0);
    
    // Handler type: 'vide'
    box[16] = 'v'; box[17] = 'i'; box[18] = 'd'; box[19] = 'e';
    
    // Reserved
    write_be32(box.data() + 20, 0);
    write_be32(box.data() + 24, 0);
    write_be32(box.data() + 28, 0);
    
    // Name: "VideoHandler\0"
    const char* name = "VideoHandler\0";
    memcpy(box.data() + 32, name, 13);
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_minf() {
    auto vmhd = build_vmhd();
    auto dinf = build_dinf();
    auto stbl = build_stbl();
    
    uint32_t minf_size = 8 + vmhd.size() + dinf.size() + stbl.size();
    
    std::vector<uint8_t> box;
    box.resize(minf_size);
    
    write_be32(box.data(), minf_size);
    box[4] = 'm'; box[5] = 'i'; box[6] = 'n'; box[7] = 'f';
    
    size_t offset = 8;
    memcpy(box.data() + offset, vmhd.data(), vmhd.size());
    offset += vmhd.size();
    memcpy(box.data() + offset, dinf.data(), dinf.size());
    offset += dinf.size();
    memcpy(box.data() + offset, stbl.data(), stbl.size());
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_vmhd() {
    std::vector<uint8_t> box;
    box.resize(20);
    
    write_be32(box.data(), 20);
    box[4] = 'v'; box[5] = 'm'; box[6] = 'h'; box[7] = 'd';
    
    // Version 0, flags 1
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 1;
    
    // Graphicsmode (0)
    write_be16(box.data() + 12, 0);
    
    // Opcolor (0,0,0)
    write_be16(box.data() + 14, 0);
    write_be16(box.data() + 16, 0);
    write_be16(box.data() + 18, 0);
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_dinf() {
    auto dref = build_dref();
    
    uint32_t dinf_size = 8 + dref.size();
    
    std::vector<uint8_t> box;
    box.resize(dinf_size);
    
    write_be32(box.data(), dinf_size);
    box[4] = 'd'; box[5] = 'i'; box[6] = 'n'; box[7] = 'f';
    
    memcpy(box.data() + 8, dref.data(), dref.size());
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_dref() {
    std::vector<uint8_t> box;
    box.resize(28);
    
    write_be32(box.data(), 28);
    box[4] = 'd'; box[5] = 'r'; box[6] = 'e'; box[7] = 'f';
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    write_be32(box.data() + 12, 1);
    write_be32(box.data() + 16, 12);
    box[20] = 'u'; box[21] = 'r'; box[22] = 'l'; box[23] = ' ';
    write_be32(box.data() + 24, 1);
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_stbl() {
    auto stsd = build_stsd();
    auto stts = build_stts();
    auto stsc = build_stsc();
    auto stsz = build_stsz();
    auto stco = build_stco();
    auto stss = build_stss();
    
    uint32_t stbl_size = 8 + stsd.size() + stts.size() + stsc.size() + 
                         stsz.size() + stco.size() + stss.size();
    
    std::vector<uint8_t> box;
    box.resize(stbl_size);
    
    write_be32(box.data(), stbl_size);
    box[4] = 's'; box[5] = 't'; box[6] = 'b'; box[7] = 'l';
    
    size_t offset = 8;
    memcpy(box.data() + offset, stsd.data(), stsd.size());
    offset += stsd.size();
    memcpy(box.data() + offset, stts.data(), stts.size());
    offset += stts.size();
    memcpy(box.data() + offset, stsc.data(), stsc.size());
    offset += stsc.size();
    memcpy(box.data() + offset, stsz.data(), stsz.size());
    offset += stsz.size();
    memcpy(box.data() + offset, stco.data(), stco.size());
    offset += stco.size();
    memcpy(box.data() + offset, stss.data(), stss.size());
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_vpcC() {
    std::vector<uint8_t> box;
    box.resize(24);
    
    write_be32(box.data(), 24);
    box[4] = 'v'; box[5] = 'p'; box[6] = 'c'; box[7] = 'C';
    
    // Version 0, flags 0
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    
    // configVersion (1)
    box[12] = 1;
    
    // profile (1 byte)
    box[13] = config_.profile;
    
    // level (1 byte)
    box[14] = config_.level;
    
    // bit_depth (4 bits) + chroma_subsample (2 bits) + full_range (1 bit) + reserved (1 bit)
    box[15] = (config_.bit_depth << 4) | 
              (config_.chroma_subsampling << 2) |
              (config_.full_range << 1);
    
    // codec_init_size (0)
    write_be16(box.data() + 16, 0);
    
    // Reserved (4 bytes)
    write_be32(box.data() + 18, 0);
    write_be16(box.data() + 22, 0);
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_stsd() {
    auto vp09 = build_vp09_entry();
    
    uint32_t stsd_size = 16 + vp09.size();
    
    std::vector<uint8_t> box;
    box.resize(stsd_size);
    
    write_be32(box.data(), stsd_size);
    box[4] = 's'; box[5] = 't'; box[6] = 's'; box[7] = 'd';
    
    // Version 0, flags 0
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    
    // Entry count
    write_be32(box.data() + 12, 1);
    
    // Copy vp09 entry
    memcpy(box.data() + 16, vp09.data(), vp09.size());
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_vp09_entry() {
    auto vpcC = build_vpcC();
    
    uint32_t entry_size = 8 + 8 + 70 + vpcC.size();  // VisualSampleEntry base + vpcC
    
    std::vector<uint8_t> box;
    box.resize(entry_size);
    
    // Size
    write_be32(box.data(), entry_size);
    
    // Type: 'vp09'
    box[4] = 'v'; box[5] = 'p'; box[6] = '0'; box[7] = '9';
    
    // Reserved (6 bytes)
    memset(box.data() + 8, 0, 6);
    
    // Data reference index
    write_be16(box.data() + 14, 1);
    
    // Pre-defined (2 bytes)
    write_be16(box.data() + 16, 0);
    
    // Reserved (2 bytes)
    write_be16(box.data() + 18, 0);
    
    // Pre-defined (12 bytes)
    memset(box.data() + 20, 0, 12);
    
    // Width
    write_be16(box.data() + 32, config_.width);
    
    // Height
    write_be16(box.data() + 34, config_.height);
    
    // HorizResolution (72 dpi = 0x00480000)
    write_be32(box.data() + 36, 0x00480000);
    
    // VertResolution (72 dpi = 0x00480000)
    write_be32(box.data() + 40, 0x00480000);
    
    // Reserved (4 bytes)
    write_be32(box.data() + 44, 0);
    
    // Frame count (1)
    write_be16(box.data() + 48, 1);
    
    // Compressorname (32 bytes, all zeros)
    memset(box.data() + 50, 0, 32);
    
    // Depth (0x0018 = 24 bits)
    write_be16(box.data() + 82, 0x0018);
    
    // Pre-defined (-1)
    write_be16(box.data() + 84, 0xFFFF);
    
    // Copy vpcC box
    memcpy(box.data() + 86, vpcC.data(), vpcC.size());
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_stts() {
    // For simplicity, assume constant frame duration
    uint32_t frame_duration = config_.timescale * config_.framerate_den / config_.framerate_num;
    
    std::vector<uint8_t> box;
    box.resize(16);
    
    write_be32(box.data(), 16);
    box[4] = 's'; box[5] = 't'; box[6] = 't'; box[7] = 's';
    
    // Version 0, flags 0
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    
    // Entry count (1 for constant duration)
    write_be32(box.data() + 12, 1);
    
    // Actually we need to expand for variable frames, but for now constant
    box.resize(24);
    write_be32(box.data(), 24);
    box[4] = 's'; box[5] = 't'; box[6] = 't'; box[7] = 's';
    write_be32(box.data() + 12, 1);
    write_be32(box.data() + 16, static_cast<uint32_t>(frames_.size()));
    write_be32(box.data() + 20, frame_duration);
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_stsc() {
    std::vector<uint8_t> box;
    box.resize(16);
    
    write_be32(box.data(), 16);
    box[4] = 's'; box[5] = 't'; box[6] = 's'; box[7] = 'c';
    
    // Version 0, flags 0
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    
    // Entry count (1)
    write_be32(box.data() + 12, 1);
    
    // Actually we need entries, let's expand
    box.resize(28);
    write_be32(box.data(), 28);
    box[4] = 's'; box[5] = 't'; box[6] = 's'; box[7] = 'c';
    write_be32(box.data() + 12, 1);
    
    // First chunk (1)
    write_be32(box.data() + 16, 1);
    // Samples per chunk (1 frame per chunk for sequential mode)
    write_be32(box.data() + 20, 1);
    // Sample description index (1)
    write_be32(box.data() + 24, 1);
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_stsz() {
    // Sample sizes: each frame is one sample
    uint32_t entry_count = static_cast<uint32_t>(frames_.size());
    uint32_t box_size = 20 + entry_count * 4;
    
    std::vector<uint8_t> box;
    box.resize(box_size);
    
    write_be32(box.data(), box_size);
    box[4] = 's'; box[5] = 't'; box[6] = 's'; box[7] = 'z';
    
    // Version 0, flags 0
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    
    // Sample size (0 = variable)
    write_be32(box.data() + 12, 0);
    
    // Sample count
    write_be32(box.data() + 16, entry_count);
    
    // Sample sizes
    for (uint32_t i = 0; i < entry_count; i++) {
        write_be32(box.data() + 20 + i * 4, frames_[i].size);
    }
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_stco() {
    std::vector<uint8_t> box;
    box.resize(20);
    
    write_be32(box.data(), 20);
    box[4] = 's'; box[5] = 't'; box[6] = 'c'; box[7] = 'o';
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    write_be32(box.data() + 12, 1);
    write_be32(box.data() + 16, 28);
    
    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_l16_entry() {
    std::vector<uint8_t> box;
    box.resize(36);

    write_be32(box.data(), 36);
    box[4] = 'L'; box[5] = '1'; box[6] = '6'; box[7] = ' ';

    memset(box.data() + 8, 0, 6);
    write_be16(box.data() + 14, 1);

    write_be32(box.data() + 16, 0);
    memset(box.data() + 20, 0, 8);

    write_be16(box.data() + 28, config_.audio_channels);
    write_be16(box.data() + 30, 16);

    write_be16(box.data() + 32, 0);
    write_be16(box.data() + 34, 0);

    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_audio_stbl() {
    auto l16 = build_l16_entry();
    uint32_t stsd_size = 16 + l16.size();
    std::vector<uint8_t> stsd(stsd_size);
    write_be32(stsd.data(), stsd_size);
    stsd[4] = 's'; stsd[5] = 't'; stsd[6] = 's'; stsd[7] = 'd';
    write_be32(stsd.data() + 8, 0);
    write_be32(stsd.data() + 12, 1);
    memcpy(stsd.data() + 16, l16.data(), l16.size());

    uint32_t audio_sample_count = static_cast<uint32_t>(audio_samples_.size());
    uint32_t sample_duration = 1;

    std::vector<uint8_t> stts(24);
    write_be32(stts.data(), 24);
    stts[4] = 's'; stts[5] = 't'; stts[6] = 't'; stts[7] = 's';
    write_be32(stts.data() + 8, 0);
    write_be32(stts.data() + 12, 1);
    write_be32(stts.data() + 16, audio_sample_count);
    write_be32(stts.data() + 20, sample_duration);

    std::vector<uint8_t> stsc(28);
    write_be32(stsc.data(), 28);
    stsc[4] = 's'; stsc[5] = 't'; stsc[6] = 's'; stsc[7] = 'c';
    write_be32(stsc.data() + 8, 0);
    write_be32(stsc.data() + 12, 1);
    write_be32(stsc.data() + 16, 1);
    write_be32(stsc.data() + 20, 1);
    write_be32(stsc.data() + 24, 1);

    uint32_t stsz_size = 20 + audio_sample_count * 4;
    std::vector<uint8_t> stsz(stsz_size);
    write_be32(stsz.data(), stsz_size);
    stsz[4] = 's'; stsz[5] = 't'; stsz[6] = 's'; stsz[7] = 'z';
    write_be32(stsz.data() + 8, 0);
    write_be32(stsz.data() + 12, 0);
    write_be32(stsz.data() + 16, audio_sample_count);
    for (uint32_t i = 0; i < audio_sample_count; i++) {
        write_be32(stsz.data() + 20 + i * 4, audio_samples_[i].size);
    }

    uint32_t audio_offset = static_cast<uint32_t>(28 + video_bytes_written_);
    std::vector<uint8_t> stco(20);
    write_be32(stco.data(), 20);
    stco[4] = 's'; stco[5] = 't'; stco[6] = 'c'; stco[7] = 'o';
    write_be32(stco.data() + 8, 0);
    write_be32(stco.data() + 12, 1);
    write_be32(stco.data() + 16, audio_offset);

    uint32_t total = 8 + stsd.size() + stts.size() + stsc.size() + stsz.size() + stco.size();
    std::vector<uint8_t> box(total);
    write_be32(box.data(), total);
    box[4] = 's'; box[5] = 't'; box[6] = 'b'; box[7] = 'l';

    size_t off = 8;
    memcpy(box.data() + off, stsd.data(), stsd.size()); off += stsd.size();
    memcpy(box.data() + off, stts.data(), stts.size()); off += stts.size();
    memcpy(box.data() + off, stsc.data(), stsc.size()); off += stsc.size();
    memcpy(box.data() + off, stsz.data(), stsz.size()); off += stsz.size();
    memcpy(box.data() + off, stco.data(), stco.size());

    return box;
}

std::vector<uint8_t> NativeMp4Muxer::build_audio_trak() {
    std::vector<uint8_t> tkhd(92);
    write_be32(tkhd.data(), 92);
    tkhd[4] = 't'; tkhd[5] = 'k'; tkhd[6] = 'h'; tkhd[7] = 'd';
    tkhd[8] = 0; tkhd[9] = 0; tkhd[10] = 0; tkhd[11] = 3;
    write_be32(tkhd.data() + 12, 0);
    write_be32(tkhd.data() + 16, 0);
    write_be32(tkhd.data() + 20, 2);
    write_be32(tkhd.data() + 24, 0);

    uint64_t audio_duration = 0;
    if (!audio_samples_.empty()) {
        audio_duration = audio_samples_.size();
    }
    write_be32(tkhd.data() + 28, static_cast<uint32_t>(audio_duration));
    write_be32(tkhd.data() + 32, 0);
    write_be32(tkhd.data() + 36, 0);
    write_be16(tkhd.data() + 40, 0);
    write_be16(tkhd.data() + 42, 0);
    write_be16(tkhd.data() + 44, 0x0100);
    write_be16(tkhd.data() + 46, 0);
    tkhd[48] = 0; tkhd[49] = 0x01;
    memset(tkhd.data() + 50, 0, 18);
    write_be32(tkhd.data() + 68, 0);
    write_be32(tkhd.data() + 72, 0);

    uint32_t audio_ts = config_.audio_timescale > 0 ? config_.audio_timescale : config_.audio_sample_rate;
    if (audio_ts == 0) audio_ts = 44100;

    std::vector<uint8_t> mdhd(32);
    write_be32(mdhd.data(), 32);
    mdhd[4] = 'm'; mdhd[5] = 'd'; mdhd[6] = 'h'; mdhd[7] = 'd';
    write_be32(mdhd.data() + 8, 0);
    write_be32(mdhd.data() + 12, 0);
    write_be32(mdhd.data() + 16, 0);
    write_be32(mdhd.data() + 20, audio_ts);
    write_be32(mdhd.data() + 24, static_cast<uint32_t>(audio_duration));
    write_be16(mdhd.data() + 28, 0x55C4);
    write_be16(mdhd.data() + 30, 0);

    std::vector<uint8_t> hdlr(45);
    write_be32(hdlr.data(), 45);
    hdlr[4] = 'h'; hdlr[5] = 'd'; hdlr[6] = 'l'; hdlr[7] = 'r';
    write_be32(hdlr.data() + 8, 0);
    write_be32(hdlr.data() + 12, 0);
    hdlr[16] = 's'; hdlr[17] = 'o'; hdlr[18] = 'u'; hdlr[19] = 'n';
    write_be32(hdlr.data() + 20, 0);
    write_be32(hdlr.data() + 24, 0);
    write_be32(hdlr.data() + 28, 0);
    const char* sname = "SoundHandler\0";
    memcpy(hdlr.data() + 32, sname, 13);

    std::vector<uint8_t> smhd(16);
    write_be32(smhd.data(), 16);
    smhd[4] = 's'; smhd[5] = 'm'; smhd[6] = 'h'; smhd[7] = 'd';
    write_be32(smhd.data() + 8, 0);
    write_be16(smhd.data() + 12, 0);

    auto dinf = build_dinf();
    auto audio_stbl = build_audio_stbl();

    uint32_t minf_size = 8 + smhd.size() + dinf.size() + audio_stbl.size();
    std::vector<uint8_t> minf(minf_size);
    write_be32(minf.data(), minf_size);
    minf[4] = 'm'; minf[5] = 'i'; minf[6] = 'n'; minf[7] = 'f';
    size_t mo = 8;
    memcpy(minf.data() + mo, smhd.data(), smhd.size()); mo += smhd.size();
    memcpy(minf.data() + mo, dinf.data(), dinf.size()); mo += dinf.size();
    memcpy(minf.data() + mo, audio_stbl.data(), audio_stbl.size());

    uint32_t mdia_size = 8 + mdhd.size() + hdlr.size() + minf.size();
    std::vector<uint8_t> mdia(mdia_size);
    write_be32(mdia.data(), mdia_size);
    mdia[4] = 'm'; mdia[5] = 'd'; mdia[6] = 'i'; mdia[7] = 'a';
    mo = 8;
    memcpy(mdia.data() + mo, mdhd.data(), mdhd.size()); mo += mdhd.size();
    memcpy(mdia.data() + mo, hdlr.data(), hdlr.size()); mo += hdlr.size();
    memcpy(mdia.data() + mo, minf.data(), minf.size());

    uint32_t trak_size = 8 + tkhd.size() + mdia.size();
    std::vector<uint8_t> trak(trak_size);
    write_be32(trak.data(), trak_size);
    trak[4] = 't'; trak[5] = 'r'; trak[6] = 'a'; trak[7] = 'k';
    mo = 8;
    memcpy(trak.data() + mo, tkhd.data(), tkhd.size()); mo += tkhd.size();
    memcpy(trak.data() + mo, mdia.data(), mdia.size());

    return trak;
}

std::vector<uint8_t> NativeMp4Muxer::build_stss() {
    // Sync samples (keyframes)
    std::vector<uint32_t> keyframe_indices;
    for (uint32_t i = 0; i < frames_.size(); i++) {
        if (frames_[i].is_keyframe) {
            keyframe_indices.push_back(i + 1);  // 1-indexed
        }
    }
    
    uint32_t entry_count = static_cast<uint32_t>(keyframe_indices.size());
    uint32_t box_size = 16 + entry_count * 4;
    
    std::vector<uint8_t> box;
    box.resize(box_size);
    
    write_be32(box.data(), box_size);
    box[4] = 's'; box[5] = 's'; box[6] = 's'; box[7] = 's';
    
    // Version 0, flags 0
    box[8] = 0; box[9] = 0; box[10] = 0; box[11] = 0;
    
    // Entry count
    write_be32(box.data() + 12, entry_count);
    
    // Keyframe indices
    for (uint32_t i = 0; i < entry_count; i++) {
        write_be32(box.data() + 16 + i * 4, keyframe_indices[i]);
    }
    
    return box;
}

void NativeMp4Muxer::write_be32(uint8_t* p, uint32_t v) {
    p[0] = (v >> 24) & 0xFF;
    p[1] = (v >> 16) & 0xFF;
    p[2] = (v >> 8) & 0xFF;
    p[3] = v & 0xFF;
}

void NativeMp4Muxer::write_be16(uint8_t* p, uint16_t v) {
    p[0] = (v >> 8) & 0xFF;
    p[1] = v & 0xFF;
}

void NativeMp4Muxer::write_le32(uint8_t* p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

void NativeMp4Muxer::write_le64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; i++) {
        p[i] = (v >> (i * 8)) & 0xFF;
    }
}

} // namespace usm_toolkit
