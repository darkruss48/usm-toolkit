#ifndef ADX_DECODER_HPP
#define ADX_DECODER_HPP

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace usm_toolkit {

struct AdxHeader {
    uint8_t  encoding;       // 3 = ADPCM
    uint32_t sample_count;
    uint16_t highpass_freq;
    uint8_t  version;
    uint8_t  channels;
    uint32_t sample_rate;
    uint16_t block_size;
    uint8_t  sample_bits;
    int16_t  coeff[2];
    uint32_t data_offset;
};

class AdxDecoder {
public:
    static bool decode_file(const std::string& input_path, const std::string& output_path,
                            size_t start_offset = 0);
    static bool parse_header(const uint8_t* data, size_t size, AdxHeader& header);
    static bool is_adx(const uint8_t* data, size_t size);

private:
    static void decode_frame_adpcm(const uint8_t* frame, int frame_size,
                                    int32_t& hist1, int32_t& hist2,
                                    int16_t* output, int samples_per_frame,
                                    const int16_t* coeff);

    static bool write_wav_header(FILE* f, uint32_t sample_rate, uint16_t channels,
                                  uint32_t data_size);
};

} // namespace usm_toolkit

#endif // ADX_DECODER_HPP
