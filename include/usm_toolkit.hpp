#ifndef USM_TOOLKIT_HPP
#define USM_TOOLKIT_HPP

/**
 * UsmToolkit - USM Video File Converter
 * 
 * Converts CRI Middleware USM video files to standard formats.
 * Cross-platform support for Windows and Linux.
 * 
 * Based on the original C# implementation by the VGMToolbox project.
 */

#include <string>
#include <cstdint>

namespace usm_toolkit {

// Version info
constexpr const char* VERSION = "1.0.0";
constexpr const char* PROGRAM_NAME = "usm_toolkit";

// Default file extensions
constexpr const char* DEFAULT_AUDIO_EXT = ".adx";
constexpr const char* DEFAULT_VIDEO_EXT = ".m2v";
constexpr const char* HCA_AUDIO_EXT = ".hca";
constexpr const char* AIX_AUDIO_EXT = ".aix";
constexpr const char* WAV_AUDIO_EXT = ".wav";

// Configuration structure
struct JoinConfig {
    std::string video_parameter;
    std::string audio_parameter;
    std::string output_format;
};

// Demux options
struct DemuxOptions {
    bool extract_video = true;
    bool extract_audio = true;
    bool add_header = false;
    bool split_audio_streams = false;
    bool add_playback_hacks = false;
};

} // namespace usm_toolkit

#endif // USM_TOOLKIT_HPP
