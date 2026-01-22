/*
    nlohmann/json - JSON for Modern C++ (single header)
    https://github.com/nlohmann/json
    
    MIT License
    Copyright (c) 2013-2022 Niels Lohmann
    
    This is a minimal subset for basic JSON parsing.
    For full version, download from: https://github.com/nlohmann/json/releases
*/

#ifndef NLOHMANN_JSON_HPP
#define NLOHMANN_JSON_HPP

#include <string>
#include <map>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cctype>

namespace nlohmann {

// Minimal JSON class for config parsing
class json {
public:
    enum class value_t { null, object, string };
    
    json() : type_(value_t::null) {}
    json(const std::string& s) : type_(value_t::string), string_value_(s) {}
    
    // Access string value
    std::string get_string() const {
        if (type_ != value_t::string) {
            throw std::runtime_error("Not a string");
        }
        return string_value_;
    }
    
    template<typename T>
    T get() const {
        return static_cast<T>(string_value_);
    }
    
    // Object access
    json& operator[](const std::string& key) {
        type_ = value_t::object;
        return object_[key];
    }
    
    const json& operator[](const std::string& key) const {
        static json null_json;
        auto it = object_.find(key);
        if (it == object_.end()) return null_json;
        return it->second;
    }
    
    bool contains(const std::string& key) const {
        return object_.find(key) != object_.end();
    }
    
    // Parse from file
    static json parse(std::ifstream& file) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        return parse(buffer.str());
    }
    
    // Simple JSON parser
    static json parse(const std::string& str) {
        json result;
        result.type_ = value_t::object;
        
        size_t pos = 0;
        skip_whitespace(str, pos);
        
        if (pos >= str.size() || str[pos] != '{') {
            throw std::runtime_error("Expected '{'");
        }
        pos++;
        
        while (pos < str.size()) {
            skip_whitespace(str, pos);
            
            if (str[pos] == '}') break;
            if (str[pos] == ',') { pos++; continue; }
            
            // Parse key
            std::string key = parse_string(str, pos);
            skip_whitespace(str, pos);
            
            if (str[pos] != ':') {
                throw std::runtime_error("Expected ':'");
            }
            pos++;
            skip_whitespace(str, pos);
            
            // Parse value (only strings supported for now)
            std::string value = parse_string(str, pos);
            
            result.object_[key] = json(value);
            skip_whitespace(str, pos);
        }
        
        return result;
    }
    
    // Conversion operator
    operator std::string() const {
        return string_value_;
    }
    
private:
    value_t type_;
    std::string string_value_;
    std::map<std::string, json> object_;
    
    static void skip_whitespace(const std::string& str, size_t& pos) {
        while (pos < str.size() && std::isspace(str[pos])) {
            pos++;
        }
    }
    
    static std::string parse_string(const std::string& str, size_t& pos) {
        if (str[pos] != '"') {
            throw std::runtime_error("Expected '\"'");
        }
        pos++;
        
        std::string result;
        while (pos < str.size() && str[pos] != '"') {
            if (str[pos] == '\\' && pos + 1 < str.size()) {
                pos++;
                switch (str[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    default: result += str[pos]; break;
                }
            } else {
                result += str[pos];
            }
            pos++;
        }
        pos++; // Skip closing quote
        return result;
    }
};

} // namespace nlohmann

#endif // NLOHMANN_JSON_HPP
