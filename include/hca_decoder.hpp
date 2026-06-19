#ifndef HCA_DECODER_HPP
#define HCA_DECODER_HPP

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace usm_toolkit {

struct HcaHeader {
    uint32_t channel_count;
    uint32_t sample_rate;
    uint32_t frame_count;
    uint32_t encoder_delay;
    uint32_t encoder_padding;
};

class HcaDecoder {
public:
    static bool decode_file(const std::string& input_path, const std::string& output_path);
    static bool parse_header(const uint8_t* data, size_t size, HcaHeader& header);
    static bool is_hca(const uint8_t* data, size_t size);

private:
    struct HcaChannel {
        float   scalefac[8];
        int     intensity[8];
        int     hfr_count;
        int     hfr_offset;
        int     channels_to_reuse;
        int     reuse_index;
        int     swap_count;
        float   ms_spectrum[8][8];
        float   temp[128];
        float   output[2048];
        int     count;
    };

    static void decode_frame(const uint8_t* frame_data, int frame_size,
                              HcaChannel* channels, int channel_count,
                              float* output_buffers);

    static bool write_wav_header(FILE* f, uint32_t sample_rate, uint16_t channels,
                                  uint32_t data_size);

    static constexpr float HCA_SF_HIPASS_FREQ_COEFF = 0.0f;
};

} // namespace usm_toolkit

#endif // HCA_DECODER_HPP
