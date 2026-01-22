/**
 * UsmToolkit - USM Video File Converter
 * 
 * Command-line tool to convert CRI Middleware USM video files
 * to standard formats like MP4.
 * 
 * Cross-platform: Windows and Linux
 * 
 * Based on the original C# implementation by VGMToolbox.
 */

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "usm_toolkit.hpp"
#include "usm_stream.hpp"
#include "file_utils.hpp"
#include "nlohmann/json.hpp"

namespace fs = std::filesystem;

void print_usage() {
    std::cout << "UsmToolkit v" << usm_toolkit::VERSION << " - USM Video File Converter\n\n";
    std::cout << "Usage:\n";
    std::cout << "  usm_toolkit extract <file/folder>           Extract audio and video streams\n";
    std::cout << "  usm_toolkit convert <file/folder> [options] Convert USM to output format\n";
    std::cout << "\n";
    std::cout << "Options for convert:\n";
    std::cout << "  -o, --output-dir <dir>  Specify output directory\n";
    std::cout << "  -c, --clean             Remove temporary .m2v and audio files after converting\n";
    std::cout << "\n";
    std::cout << "Examples:\n";
    std::cout << "  usm_toolkit extract movie.usm\n";
    std::cout << "  usm_toolkit convert movie.usm -o output/ -c\n";
    std::cout << "  usm_toolkit convert ./usm_folder/\n";
}

void print_version() {
    std::cout << "UsmToolkit v" << usm_toolkit::VERSION << std::endl;
}

bool extract_file(const std::string& file_path) {
    std::cout << "File: " << file_path << std::endl;
    
    try {
        usm_toolkit::CriUsmStream usm_stream(file_path);
        
        std::cout << "Demuxing..." << std::endl;
        usm_toolkit::DemuxOptions options;
        options.extract_video = true;
        options.extract_audio = true;
        options.add_header = false;
        
        usm_stream.demultiplex_streams(options);
        std::cout << "Done." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

usm_toolkit::JoinConfig load_config(const std::string& config_path) {
    usm_toolkit::JoinConfig config;
    config.video_parameter = "-c:v copy";
    config.audio_parameter = "-c:a ac3 -b:a 640k";
    config.output_format = "mp4";
    
    std::ifstream file(config_path);
    if (!file.is_open()) {
        std::cerr << "Warning: config.json not found, using defaults." << std::endl;
        return config;
    }
    
    try {
        nlohmann::json j = nlohmann::json::parse(file);
        
        if (j.contains("VideoParameter")) {
            config.video_parameter = j["VideoParameter"].get_string();
        }
        if (j.contains("AudioParameter")) {
            config.audio_parameter = j["AudioParameter"].get_string();
        }
        if (j.contains("OutputFormat")) {
            config.output_format = j["OutputFormat"].get_string();
        }
    } catch (const std::exception& e) {
        std::cerr << "Warning: Error parsing config.json: " << e.what() << std::endl;
    }
    
    file.close();
    return config;
}

std::string create_ffmpeg_parameters(usm_toolkit::CriUsmStream& usm_stream,
                                     const std::string& pure_file_name,
                                     const std::string& output_dir,
                                     const usm_toolkit::JoinConfig& config) {
    std::stringstream ss;
    
    // Input video
    std::string video_file = usm_toolkit::FileUtils::change_extension(
        usm_stream.get_file_path(), usm_stream.get_file_extension_video());
    ss << "-i \"" << video_file << "\" ";
    
    // Input audio (if present)
    if (usm_stream.has_audio()) {
        std::string audio_file = usm_toolkit::FileUtils::change_extension(
            usm_stream.get_file_path(), usm_stream.get_final_audio_extension());
        ss << "-i \"" << audio_file << "\" ";
    }
    
    // Video parameters
    ss << config.video_parameter << " ";
    
    // Audio parameters (if audio present)
    if (usm_stream.has_audio()) {
        ss << config.audio_parameter << " ";
    }
    
    // Output file
    std::string output_file_name = pure_file_name + "." + config.output_format;
    std::string output_path;
    if (!output_dir.empty()) {
        output_path = usm_toolkit::FileUtils::combine_path(output_dir, output_file_name);
    } else {
        output_path = usm_toolkit::FileUtils::combine_path(
            usm_toolkit::FileUtils::get_directory_name(usm_stream.get_file_path()),
            output_file_name);
    }
    
    ss << "-y \"" << output_path << "\"";
    
    return ss.str();
}

bool convert_file(const std::string& file_path, 
                  const std::string& output_dir,
                  bool clean_temp_files) {
    std::cout << "File: " << file_path << std::endl;
    
    try {
        usm_toolkit::CriUsmStream usm_stream(file_path);
        
        std::cout << "Demuxing..." << std::endl;
        usm_toolkit::DemuxOptions options;
        options.extract_video = true;
        options.extract_audio = true;
        options.add_header = false;
        
        usm_stream.demultiplex_streams(options);
        
        // Create output directory if specified
        if (!output_dir.empty() && !fs::exists(output_dir)) {
            fs::create_directories(output_dir);
        }
        
        // Find config.json next to executable or in current directory
        std::string config_path = "config.json";
        usm_toolkit::JoinConfig config = load_config(config_path);
        
        std::string pure_file_name = usm_toolkit::FileUtils::get_filename_without_extension(file_path);
        
        // Check for ADX audio - needs conversion to WAV first
        if (usm_stream.get_final_audio_extension() == ".adx") {
            std::cout << "ADX audio detected. FFmpeg should handle it directly." << std::endl;
            // Note: Modern FFmpeg can handle ADX directly. If not, vgmstream would be needed.
        }
        
        // Build FFmpeg command
        std::string ffmpeg_params = create_ffmpeg_parameters(
            usm_stream, pure_file_name, output_dir, config);
        
        std::cout << "Converting with FFmpeg..." << std::endl;
        int result = usm_toolkit::FileUtils::execute_process("ffmpeg", ffmpeg_params);
        
        if (result != 0) {
            std::cerr << "Warning: FFmpeg returned error code " << result << std::endl;
        }
        
        // Clean temporary files if requested
        if (clean_temp_files) {
            std::cout << "Cleaning up temporary files..." << std::endl;
            
            std::vector<std::string> extensions = {".wav", ".adx", ".hca", ".m2v", ".aix"};
            for (const auto& ext : extensions) {
                std::string temp_file = usm_toolkit::FileUtils::change_extension(file_path, ext);
                if (fs::exists(temp_file)) {
                    fs::remove(temp_file);
                }
            }
        }
        
        std::cout << "Done." << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return false;
    }
}

void process_path(const std::string& input_path, 
                  bool convert_mode,
                  const std::string& output_dir,
                  bool clean_temp_files) {
    if (!fs::exists(input_path)) {
        std::cerr << "Error: Path does not exist: " << input_path << std::endl;
        return;
    }
    
    if (fs::is_directory(input_path)) {
        // Process all .usm files in directory
        for (const auto& entry : fs::directory_iterator(input_path)) {
            if (entry.is_regular_file() && 
                entry.path().extension() == ".usm") {
                if (convert_mode) {
                    convert_file(entry.path().string(), output_dir, clean_temp_files);
                } else {
                    extract_file(entry.path().string());
                }
            }
        }
    } else {
        // Process single file
        if (convert_mode) {
            convert_file(input_path, output_dir, clean_temp_files);
        } else {
            extract_file(input_path);
        }
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }
    
    std::string command = argv[1];
    
    if (command == "--help" || command == "-h") {
        print_usage();
        return 0;
    }
    
    if (command == "--version" || command == "-v") {
        print_version();
        return 0;
    }
    
    if (command == "extract") {
        if (argc < 3) {
            std::cerr << "Error: Missing input file/folder argument" << std::endl;
            print_usage();
            return 1;
        }
        
        std::string input_path = argv[2];
        process_path(input_path, false, "", false);
        return 0;
    }
    
    if (command == "convert") {
        if (argc < 3) {
            std::cerr << "Error: Missing input file/folder argument" << std::endl;
            print_usage();
            return 1;
        }
        
        std::string input_path = argv[2];
        std::string output_dir;
        bool clean_temp_files = false;
        
        // Parse options
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            
            if ((arg == "-o" || arg == "--output-dir") && i + 1 < argc) {
                output_dir = argv[++i];
            } else if (arg == "-c" || arg == "--clean") {
                clean_temp_files = true;
            }
        }
        
        process_path(input_path, true, output_dir, clean_temp_files);
        return 0;
    }
    
    std::cerr << "Error: Unknown command: " << command << std::endl;
    print_usage();
    return 1;
}
