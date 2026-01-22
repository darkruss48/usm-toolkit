#ifndef FILE_UTILS_HPP
#define FILE_UTILS_HPP

#include <string>
#include <vector>
#include <fstream>
#include <cstdint>
#include <filesystem>

namespace usm_toolkit {

// Constants
constexpr size_t FILE_READ_CHUNK_SIZE = 71680; // Same as VGMToolbox

/**
 * File utility functions for binary file operations
 */
class FileUtils {
public:
    /**
     * Read bytes from a stream at a specific offset
     */
    static std::vector<uint8_t> read_bytes(std::ifstream& stream, 
                                           int64_t offset, 
                                           size_t length);

    /**
     * Find the next occurrence of a byte pattern in a stream
     * Returns -1 if not found
     */
    static int64_t find_next_offset(std::ifstream& stream,
                                    int64_t starting_offset,
                                    const std::vector<uint8_t>& search_bytes);

    /**
     * Compare a segment of bytes starting at offset with target
     */
    static bool compare_segment(const std::vector<uint8_t>& source,
                                size_t offset,
                                const std::vector<uint8_t>& target);

    /**
     * Extract a chunk from a stream to a file
     */
    static void extract_chunk_to_file(std::ifstream& stream,
                                      int64_t starting_offset,
                                      int64_t length,
                                      const std::string& file_path);

    /**
     * Remove a chunk from a file (creates new file, returns path)
     */
    static std::string remove_chunk_from_file(const std::string& path,
                                              int64_t starting_offset,
                                              int64_t length);

    /**
     * Execute an external process (cross-platform)
     */
    static int execute_process(const std::string& program,
                               const std::string& arguments);

    /**
     * Get file size
     */
    static int64_t get_file_size(std::ifstream& stream);

    /**
     * Check if file exists
     */
    static bool file_exists(const std::string& path);

    /**
     * Create directory if it doesn't exist
     */
    static bool create_directory(const std::string& path);

    /**
     * Get directory name from path
     */
    static std::string get_directory_name(const std::string& path);

    /**
     * Get file name without extension
     */
    static std::string get_filename_without_extension(const std::string& path);

    /**
     * Change file extension
     */
    static std::string change_extension(const std::string& path, 
                                        const std::string& new_ext);

    /**
     * Combine paths
     */
    static std::string combine_path(const std::string& path1, 
                                    const std::string& path2);
};

} // namespace usm_toolkit

#endif // FILE_UTILS_HPP
