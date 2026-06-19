#include "adx_decoder.hpp"
#include <cstring>
#include <cstdio>
#include <cmath>

namespace usm_toolkit {

bool AdxDecoder::is_adx(const uint8_t* data, size_t size) {
    if (size < 2) return false;
    return data[0] == 0x80 && data[1] == 0x00;
}

bool AdxDecoder::parse_header(const uint8_t* data, size_t size, AdxHeader& header) {
    if (size < 4) return false;
    if (data[0] != 0x80 || data[1] != 0x00) return false;

    header.encoding = data[2];

    uint16_t copyright_offset = (static_cast<uint16_t>(data[3]) << 8) | data[4];
    if (copyright_offset & 0x8000) {
        header.version = static_cast<uint8_t>(copyright_offset >> 12);
        header.encoding = static_cast<uint8_t>((copyright_offset >> 8) & 0x0F);
        copyright_offset &= 0x0FFF;
    } else {
        header.version = 3;
    }

    if (size < 16) return false;

    header.channels = data[7];
    header.sample_rate = (static_cast<uint32_t>(data[8]) << 24) |
                         (static_cast<uint32_t>(data[9]) << 16) |
                         (static_cast<uint32_t>(data[10]) << 8) |
                         static_cast<uint32_t>(data[11]);
    header.block_size = (static_cast<uint16_t>(data[12]) << 8) | data[13];
    header.sample_bits = data[14];
    header.sample_count = (static_cast<uint32_t>(data[15]) << 24) |
                          (static_cast<uint32_t>(data[16]) << 16) |
                          (static_cast<uint32_t>(data[17]) << 8) |
                          static_cast<uint32_t>(data[18]);

    if (header.version >= 4) {
        if (size < 20) return false;
        header.highpass_freq = (static_cast<uint16_t>(data[19]) << 8) | data[20];
    } else {
        header.highpass_freq = 0;
    }

    double a = 0.984837603729248046875000;
    if (header.highpass_freq > 0) {
        double omega = 2.0 * 3.14159265358979323846 * header.highpass_freq / header.sample_rate;
        a = std::cos(omega);
        double b = std::sqrt(2.0 * a);
        double c = std::sqrt(1.0 - a * a);
        b = b * c;
        a = a * 2.0;
        header.coeff[0] = static_cast<int16_t>(b * -32768.0);
        header.coeff[1] = static_cast<int16_t>(a * 32768.0);
    } else {
        header.coeff[0] = 0;
        header.coeff[1] = static_cast<int16_t>(0x0000FC00);
    }

    if (copyright_offset >= 4) {
        header.data_offset = copyright_offset + 4;
    } else {
        header.data_offset = static_cast<uint32_t>(copyright_offset);
    }

    if (header.sample_bits == 4) {
        uint32_t total_frames = (header.sample_count + 31) / 32;
        uint32_t samples_per_frame = header.block_size / header.channels * 2;
        if (samples_per_frame == 0) samples_per_frame = 32;
        total_frames = (header.sample_count + samples_per_frame - 1) / samples_per_frame;
        header.sample_count = total_frames * samples_per_frame;
    }

    return true;
}

void AdxDecoder::decode_frame_adpcm(const uint8_t* frame, int frame_size,
                                     int32_t& hist1, int32_t& hist2,
                                     int16_t* output, int samples_per_frame,
                                     const int16_t* coeff) {
    int32_t c0 = coeff[0];
    int32_t c1 = coeff[1];

    int shift = ((frame_size - 2) > 0) ? ((frame_size - 2) * 2) : 1;

    for (int s = 0; s < samples_per_frame; s++) {
        int byte_idx = 2 + s / 2;
        if (byte_idx >= frame_size) break;

        int nibble;
        if (s & 1) {
            nibble = frame[byte_idx] & 0x0F;
        } else {
            nibble = (frame[byte_idx] >> 4) & 0x0F;
        }

        int32_t predict = (c0 * hist1 + c1 * hist2) / 0x7FFF;

        int32_t sample;
        if (nibble >= 8) {
            sample = predict + (static_cast<int32_t>(nibble - 16) << (shift - 1));
        } else {
            sample = predict + (static_cast<int32_t>(nibble) << (shift - 1));
        }

        if (sample < -32768) sample = -32768;
        if (sample > 32767) sample = 32767;

        output[s] = static_cast<int16_t>(sample);
        hist2 = hist1;
        hist1 = sample;
    }
}

bool AdxDecoder::write_wav_header(FILE* f, uint32_t sample_rate, uint16_t channels,
                                   uint32_t data_size) {
    uint32_t file_size = data_size + 36;
    uint32_t byte_rate = sample_rate * channels * 2;
    uint16_t block_align = channels * 2;

    uint8_t header[44];
    memcpy(header, "RIFF", 4);
    header[4] = file_size & 0xFF; header[5] = (file_size >> 8) & 0xFF;
    header[6] = (file_size >> 16) & 0xFF; header[7] = (file_size >> 24) & 0xFF;
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);

    uint32_t fmt_size = 16;
    header[16] = fmt_size & 0xFF; header[17] = (fmt_size >> 8) & 0xFF;
    header[18] = (fmt_size >> 16) & 0xFF; header[19] = (fmt_size >> 24) & 0xFF;

    header[20] = 1; header[21] = 0;
    header[22] = channels & 0xFF; header[23] = (channels >> 8) & 0xFF;
    header[24] = sample_rate & 0xFF; header[25] = (sample_rate >> 8) & 0xFF;
    header[26] = (sample_rate >> 16) & 0xFF; header[27] = (sample_rate >> 24) & 0xFF;
    header[28] = byte_rate & 0xFF; header[29] = (byte_rate >> 8) & 0xFF;
    header[30] = (byte_rate >> 16) & 0xFF; header[31] = (byte_rate >> 24) & 0xFF;
    header[32] = block_align & 0xFF; header[33] = (block_align >> 8) & 0xFF;
    header[34] = 16; header[35] = 0;

    memcpy(header + 36, "data", 4);
    header[40] = data_size & 0xFF; header[41] = (data_size >> 8) & 0xFF;
    header[42] = (data_size >> 16) & 0xFF; header[43] = (data_size >> 24) & 0xFF;

    return fwrite(header, 1, 44, f) == 44;
}

bool AdxDecoder::decode_file(const std::string& input_path, const std::string& output_path,
                             size_t start_offset) {
    FILE* in = fopen(input_path.c_str(), "rb");
    if (!in) return false;

    fseek(in, 0, SEEK_END);
    long file_size = ftell(in);
    fseek(in, static_cast<long>(start_offset), SEEK_SET);

    if (file_size < static_cast<long>(start_offset) + 4) { fclose(in); return false; }

    std::vector<uint8_t> header_buf(64);
    size_t hdr_read = fread(header_buf.data(), 1, 64, in);
    if (hdr_read < 4) { fclose(in); return false; }

    AdxHeader hdr;
    if (!parse_header(header_buf.data(), hdr_read, hdr)) {
        fclose(in);
        return false;
    }

    int32_t hist1 = 0, hist2 = 0;
    int block_size = hdr.block_size * hdr.channels;
    if (block_size <= 0) block_size = 18 * hdr.channels;

    int samples_per_frame = block_size / hdr.channels;
    if (samples_per_frame <= 0) samples_per_frame = 32;

    uint32_t pcm_data_size = hdr.sample_count * hdr.channels * 2;

    FILE* out = fopen(output_path.c_str(), "wb");
    if (!out) { fclose(in); return false; }

    if (!write_wav_header(out, hdr.sample_rate, hdr.channels, pcm_data_size)) {
        fclose(in);
        fclose(out);
        return false;
    }

    fseek(in, hdr.data_offset, SEEK_SET);

    std::vector<uint8_t> frame_buf(block_size);
    std::vector<int16_t> pcm_buf(samples_per_frame * hdr.channels);
    uint32_t total_samples_written = 0;

    while (total_samples_written < hdr.sample_count) {
        size_t bytes_read = fread(frame_buf.data(), 1, block_size, in);
        if (static_cast<int>(bytes_read) < block_size) break;

        for (uint8_t ch = 0; ch < hdr.channels; ch++) {
            decode_frame_adpcm(frame_buf.data() + ch * hdr.block_size,
                               hdr.block_size,
                               hist1, hist2,
                               pcm_buf.data() + ch,
                               samples_per_frame,
                               hdr.coeff);

            for (int s = 0; s < samples_per_frame; s++) {
                int16_t sample = pcm_buf[s * hdr.channels + ch];
                fwrite(&sample, 2, 1, out);
            }
        }

        total_samples_written += samples_per_frame;
    }

    fclose(in);
    fclose(out);
    return true;
}

} // namespace usm_toolkit
