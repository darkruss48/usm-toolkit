#include "hca_decoder.hpp"
#include <cstring>
#include <cmath>
#include <cstdio>

namespace usm_toolkit {

bool HcaDecoder::is_hca(const uint8_t* data, size_t size) {
    if (size < 8) return false;
    return data[0] == 'H' && data[1] == 'C' && data[2] == 'A' && data[3] == 0;
}

bool HcaDecoder::parse_header(const uint8_t* data, size_t size, HcaHeader& header) {
    if (size < 64) return false;
    if (!is_hca(data, size)) return false;

    uint32_t bitreader = 0;
    int bitpos = 0;

    auto read_bits = [&](int n) -> uint32_t {
        while (bitpos < n) {
            int byte_idx = bitpos / 8;
            if (byte_idx >= static_cast<int>(size)) return 0;
            bitreader = (bitreader << 8) | data[byte_idx];
            bitpos += 8;
        }
        bitpos -= n;
        return (bitreader >> bitpos) & ((1u << n) - 1);
    };

    read_bits(32);
    read_bits(16);
    header.channel_count = read_bits(8);
    read_bits(5);
    header.sample_rate = read_bits(27);
    header.frame_count = read_bits(32);
    header.encoder_delay = read_bits(16);
    header.encoder_padding = read_bits(16);

    return header.channel_count > 0 && header.sample_rate > 0;
}

bool HcaDecoder::write_wav_header(FILE* f, uint32_t sample_rate, uint16_t channels,
                                   uint32_t data_size) {
    uint32_t file_size = data_size + 36;
    uint32_t byte_rate = sample_rate * channels * 2;
    uint16_t block_align = channels * 2;

    uint8_t h[44];
    memcpy(h, "RIFF", 4);
    h[4] = file_size; h[5] = file_size >> 8; h[6] = file_size >> 16; h[7] = file_size >> 24;
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    h[16] = 16; h[17] = 0; h[18] = 0; h[19] = 0;
    h[20] = 1; h[21] = 0;
    h[22] = channels; h[23] = 0;
    h[24] = sample_rate; h[25] = sample_rate >> 8; h[26] = sample_rate >> 16; h[27] = sample_rate >> 24;
    h[28] = byte_rate; h[29] = byte_rate >> 8; h[30] = byte_rate >> 16; h[31] = byte_rate >> 24;
    h[32] = block_align; h[33] = 0;
    h[34] = 16; h[35] = 0;
    memcpy(h + 36, "data", 4);
    h[40] = data_size; h[41] = data_size >> 8; h[42] = data_size >> 16; h[43] = data_size >> 24;

    return fwrite(h, 1, 44, f) == 44;
}

void HcaDecoder::decode_frame(const uint8_t* frame_data, int frame_size,
                                HcaChannel* channels, int channel_count,
                                float* output_buffers) {
    if (frame_size < 4) return;

    int sync = (frame_data[0] << 8) | frame_data[1];
    if (sync != 0xFFFF) return;

    int bitpos = 32;
    auto read_bits = [&](int n) -> uint32_t {
        uint32_t val = 0;
        for (int i = 0; i < n; i++) {
            int byte_idx = bitpos / 8;
            int bit_idx = 7 - (bitpos % 8);
            if (byte_idx < frame_size) {
                val = (val << 1) | ((frame_data[byte_idx] >> bit_idx) & 1);
            }
            bitpos++;
        }
        return val;
    };

    auto read_int = [&](int n) -> int {
        uint32_t v = read_bits(n);
        if (n > 0 && (v >> (n - 1))) {
            v |= ~((1u << n) - 1);
        }
        return static_cast<int>(v);
    };

    for (int ch = 0; ch < channel_count; ch++) {
        uint32_t delta = read_bits(6);
        uint32_t hfr_count = read_bits(3);
        uint32_t intensity_count = read_bits(4);
        uint32_t code_index = read_bits(5);

        int scale_count = delta + intensity_count + hfr_count;
        for (int i = 0; i < scale_count && i < 8; i++) {
            uint32_t scale = read_bits(6);
            channels[ch].scalefac[i] = scale;
            channels[ch].intensity[i] = 0;
        }
        for (int i = scale_count; i < 8; i++) {
            channels[ch].scalefac[i] = 0;
        }

        channels[ch].hfr_count = hfr_count;
        channels[ch].hfr_offset = scale_count - 1;

        for (int i = 0; i < 8; i++) {
            for (int s = 0; s < 128; s++) {
                channels[ch].temp[s] = 0.0f;
            }
        }

        int total_subbands = 8;
        for (int sb = 0; sb < total_subbands; sb++) {
            int64_t scale = 1;
            if (sb < 8) {
                scale = static_cast<int64_t>(channels[ch].scalefac[sb]);
            }
            float factor = 1.0f;
            for (int i = 0; i < scale; i++) {
                factor *= 2.0f;
            }

            for (int sp = 0; sp < 128; sp += 128) {
                int val = read_int(8);
                float fval = val / 128.0f;
                channels[ch].temp[sb + sp] = fval * factor;
            }
        }
    }

    for (int ch = 0; ch < channel_count; ch++) {
        for (int s = 0; s < 128; s++) {
            float sample = channels[ch].temp[s];
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;
            int16_t pcm = static_cast<int16_t>(sample * 32767.0f);
            output_buffers[ch * 128 + s] = sample;
            (void)pcm;
        }
    }
}

bool HcaDecoder::decode_file(const std::string& input_path, const std::string& output_path) {
    FILE* in = fopen(input_path.c_str(), "rb");
    if (!in) return false;

    fseek(in, 0, SEEK_END);
    long file_size = ftell(in);
    fseek(in, 0, SEEK_SET);

    std::vector<uint8_t> header_buf(256);
    size_t hdr_read = fread(header_buf.data(), 1, 256, in);

    HcaHeader hdr;
    if (!parse_header(header_buf.data(), hdr_read, hdr)) {
        fclose(in);
        return false;
    }

    int channels = static_cast<int>(hdr.channel_count);
    if (channels <= 0 || channels > 16) { fclose(in); return false; }

    std::vector<HcaChannel> channel_data(channels);
    for (auto& ch : channel_data) {
        memset(&ch, 0, sizeof(ch));
    }

    int frame_size = 2048;
    std::vector<uint8_t> frame_buf(frame_size);
    std::vector<float> pcm_float(channels * 128);
    std::vector<int16_t> pcm_int16(channels * 128);

    uint32_t pcm_data_size = (hdr.frame_count - hdr.encoder_delay / 128) * channels * 2 * 128;
    if (pcm_data_size > 200000000) pcm_data_size = 200000000;

    FILE* out = fopen(output_path.c_str(), "wb");
    if (!out) { fclose(in); return false; }

    if (!write_wav_header(out, hdr.sample_rate, channels, pcm_data_size)) {
        fclose(in); fclose(out);
        return false;
    }

    uint32_t frames_written = 0;
    uint32_t skip_frames = (hdr.encoder_delay + 127) / 128;

    while (frames_written < hdr.frame_count) {
        size_t bytes = fread(frame_buf.data(), 1, frame_size, in);
        if (static_cast<int>(bytes) < frame_size) break;

        uint16_t sync = (static_cast<uint16_t>(frame_buf[0]) << 8) | frame_buf[1];
        if (sync != 0xFFFF) break;

        if (frames_written >= skip_frames) {
            decode_frame(frame_buf.data(), frame_size, channel_data.data(), channels,
                        pcm_float.data());

            for (int ch = 0; ch < channels; ch++) {
                for (int s = 0; s < 128; s++) {
                    float sample = pcm_float[ch * 128 + s];
                    if (sample > 1.0f) sample = 1.0f;
                    if (sample < -1.0f) sample = -1.0f;
                    pcm_int16[s * channels + ch] = static_cast<int16_t>(sample * 32767.0f);
                }
            }
            fwrite(pcm_int16.data(), 2, 128 * channels, out);
        }

        frames_written++;
    }

    fclose(in);
    fclose(out);
    return true;
}

} // namespace usm_toolkit
