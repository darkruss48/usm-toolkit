#include "file_utils.hpp"
#include "byte_utils.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#include <cstdlib>
#endif

namespace usm_toolkit {

std::vector<uint8_t> FileUtils::read_bytes(std::ifstream& stream, 
                                           int64_t offset, 
                                           size_t length) {
    std::vector<uint8_t> result(length);
    
    // Save current position
    auto current_pos = stream.tellg();
    
    // Seek to offset and read
    stream.seekg(offset, std::ios::beg);
    stream.read(reinterpret_cast<char*>(result.data()), length);
    
    // Restore position
    stream.seekg(current_pos);
    
    return result;
}

int64_t FileUtils::find_next_offset(std::ifstream& stream,
                                    int64_t starting_offset,
                                    const std::vector<uint8_t>& search_bytes) {
    if (search_bytes.empty()) return -1;
    
    // Save current position
    auto initial_pos = stream.tellg();
    
    std::vector<uint8_t> check_bytes(FILE_READ_CHUNK_SIZE);
    int64_t absolute_offset = starting_offset;
    
    // Get file size
    stream.seekg(0, std::ios::end);
    int64_t file_size = stream.tellg();
    
    while (absolute_offset < file_size) {
        stream.seekg(absolute_offset, std::ios::beg);
        stream.read(reinterpret_cast<char*>(check_bytes.data()), FILE_READ_CHUNK_SIZE);
        auto bytes_read = stream.gcount();
        
        for (int64_t relative_offset = 0; 
             relative_offset < bytes_read - static_cast<int64_t>(search_bytes.size()); 
             ++relative_offset) {
            
            bool found = true;
            for (size_t i = 0; i < search_bytes.size(); ++i) {
                if (check_bytes[relative_offset + i] != search_bytes[i]) {
                    found = false;
                    break;
                }
            }
            
            if (found) {
                stream.seekg(initial_pos);
                return absolute_offset + relative_offset;
            }
        }
        
        // Move forward, overlapping to catch patterns at chunk boundaries
        absolute_offset += FILE_READ_CHUNK_SIZE - search_bytes.size();
    }
    
    // Restore position
    stream.seekg(initial_pos);
    return -1;
}

bool FileUtils::compare_segment(const std::vector<uint8_t>& source,
                                size_t offset,
                                const std::vector<uint8_t>& target) {
    if (source.size() < offset + target.size()) return false;
    
    for (size_t i = 0; i < target.size(); ++i) {
        if (source[offset + i] != target[i]) {
            return false;
        }
    }
    return true;
}

void FileUtils::extract_chunk_to_file(std::ifstream& stream,
                                      int64_t starting_offset,
                                      int64_t length,
                                      const std::string& file_path) {
    // Create output directory if needed
    std::filesystem::path out_path(file_path);
    if (out_path.has_parent_path()) {
        std::filesystem::create_directories(out_path.parent_path());
    }
    
    std::ofstream out_file(file_path, std::ios::binary);
    if (!out_file.is_open()) {
        std::cerr << "Error: Cannot create output file: " << file_path << std::endl;
        return;
    }
    
    stream.seekg(starting_offset, std::ios::beg);
    
    std::vector<char> buffer(FILE_READ_CHUNK_SIZE);
    int64_t total_written = 0;
    
    while (total_written < length) {
        int64_t to_read = std::min(static_cast<int64_t>(buffer.size()), length - total_written);
        stream.read(buffer.data(), to_read);
        auto bytes_read = stream.gcount();
        
        if (bytes_read <= 0) break;
        
        out_file.write(buffer.data(), bytes_read);
        total_written += bytes_read;
    }
    
    out_file.close();
}

std::string FileUtils::remove_chunk_from_file(const std::string& path,
                                              int64_t starting_offset,
                                              int64_t length) {
    std::filesystem::path full_path = std::filesystem::absolute(path);
    
    if (!std::filesystem::exists(full_path)) {
        return "";
    }
    
    // Create destination path with .cut extension
    std::filesystem::path dest_path = full_path;
    dest_path.replace_extension(".cut");
    
    std::ifstream source(full_path, std::ios::binary);
    std::ofstream dest(dest_path, std::ios::binary);
    
    if (!source.is_open() || !dest.is_open()) {
        return "";
    }
    
    std::vector<char> buffer(1024);
    
    // Copy from start to starting_offset
    int64_t bytes_to_copy = starting_offset;
    int64_t total_copied = 0;
    
    while (total_copied < bytes_to_copy) {
        int64_t to_read = std::min(static_cast<int64_t>(buffer.size()), bytes_to_copy - total_copied);
        source.read(buffer.data(), to_read);
        auto bytes_read = source.gcount();
        if (bytes_read <= 0) break;
        dest.write(buffer.data(), bytes_read);
        total_copied += bytes_read;
    }
    
    // Skip the chunk to remove
    source.seekg(starting_offset + length, std::ios::beg);
    
    // Copy remainder
    while (true) {
        source.read(buffer.data(), buffer.size());
        auto bytes_read = source.gcount();
        if (bytes_read <= 0) break;
        dest.write(buffer.data(), bytes_read);
    }
    
    source.close();
    dest.close();
    
    return dest_path.string();
}

int FileUtils::execute_process(const std::string& program,
                               const std::string& arguments) {
#ifdef PLATFORM_WINDOWS
    std::string command = program + " " + arguments;
    
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    ZeroMemory(&pi, sizeof(pi));
    
    std::vector<char> cmd(command.begin(), command.end());
    cmd.push_back('\0');
    
    if (!CreateProcessA(
            nullptr,
            cmd.data(),
            nullptr,
            nullptr,
            FALSE,
            CREATE_NO_WINDOW,
            nullptr,
            nullptr,
            &si,
            &pi)) {
        return -1;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exit_code = 0;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return static_cast<int>(exit_code);
#else
    std::string command = program + " " + arguments;
    return system(command.c_str());
#endif
}

int64_t FileUtils::get_file_size(std::ifstream& stream) {
    auto current_pos = stream.tellg();
    stream.seekg(0, std::ios::end);
    int64_t size = stream.tellg();
    stream.seekg(current_pos);
    return size;
}

bool FileUtils::file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

bool FileUtils::create_directory(const std::string& path) {
    return std::filesystem::create_directories(path);
}

std::string FileUtils::get_directory_name(const std::string& path) {
    std::filesystem::path p(path);
    return p.parent_path().string();
}

std::string FileUtils::get_filename_without_extension(const std::string& path) {
    std::filesystem::path p(path);
    return p.stem().string();
}

std::string FileUtils::change_extension(const std::string& path, 
                                        const std::string& new_ext) {
    std::filesystem::path p(path);
    p.replace_extension(new_ext);
    return p.string();
}

std::string FileUtils::combine_path(const std::string& path1, 
                                    const std::string& path2) {
    std::filesystem::path p1(path1);
    std::filesystem::path p2(path2);
    return (p1 / p2).string();
}

} // namespace usm_toolkit
