#include "abx2xml.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <cmath>
#include <array>
#include <algorithm>
#include <bit>

namespace abx2xml {


std::string encode_xml_entities(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    
    for (char c : text) {
        switch (c) {
            case '&': result += "&amp;"; break;
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&apos;"; break;
            default: result += c; break;
        }
    }
    
    return result;
}


std::string hex_encode(const uint8_t* data, size_t len) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    
    for (size_t i = 0; i < len; ++i) {
        oss << std::setw(2) << static_cast<int>(data[i]);
    }
    
    return oss.str();
}


std::string base64_encode(const uint8_t* data, size_t len) {
    static constexpr std::string_view b64_table = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    
    std::string result;
    result.reserve(((len + 2) / 3) * 4);
    
    for (size_t i = 0; i < len; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < len) n |= static_cast<uint32_t>(data[i + 1]) << 8;
        if (i + 2 < len) n |= static_cast<uint32_t>(data[i + 2]);
        
        result += b64_table[(n >> 18) & 0x3F];
        result += b64_table[(n >> 12) & 0x3F];
        result += (i + 1 < len) ? b64_table[(n >> 6) & 0x3F] : '=';
        result += (i + 2 < len) ? b64_table[n & 0x3F] : '=';
    }
    
    return result;
}


DataInput::DataInput(std::istream& input) 
    : reader(input), peeked_byte(std::nullopt) {}

uint8_t DataInput::read_byte() {
    if (peeked_byte.has_value()) {
        uint8_t byte = *peeked_byte;
        peeked_byte.reset();
        return byte;
    }
    
    char c;
    if (!reader.get(c)) {
        throw AbxError("Failed to read byte");
    }
    return static_cast<uint8_t>(c);
}

uint8_t DataInput::peek_byte() {
    if (peeked_byte.has_value()) {
        return *peeked_byte;
    }
    
    peeked_byte = read_byte();
    return *peeked_byte;
}

uint16_t DataInput::read_short() {
    std::array<uint8_t, 2> bytes = {read_byte(), read_byte()};
    
    if constexpr (std::endian::native == std::endian::little) {
        std::reverse(bytes.begin(), bytes.end());
    }
    
    uint16_t result;
    std::memcpy(&result, bytes.data(), sizeof(result));
    return result;
}

int32_t DataInput::read_int() {
    std::array<uint8_t, 4> bytes = {
        read_byte(), read_byte(), read_byte(), read_byte()
    };
    
    if constexpr (std::endian::native == std::endian::little) {
        std::reverse(bytes.begin(), bytes.end());
    }
    
    int32_t result;
    std::memcpy(&result, bytes.data(), sizeof(result));
    return result;
}

int64_t DataInput::read_long() {
    std::array<uint8_t, 8> bytes = {
        read_byte(), read_byte(), read_byte(), read_byte(),
        read_byte(), read_byte(), read_byte(), read_byte()
    };
    
    if constexpr (std::endian::native == std::endian::little) {
        std::reverse(bytes.begin(), bytes.end());
    }
    
    int64_t result;
    std::memcpy(&result, bytes.data(), sizeof(result));
    return result;
}

float DataInput::read_float() {
    int32_t int_val = read_int();
    uint32_t uint_val = static_cast<uint32_t>(int_val);
    
    float result;
    std::memcpy(&result, &uint_val, sizeof(float));
    return result;
}

double DataInput::read_double() {
    int64_t long_val = read_long();
    uint64_t ulong_val = static_cast<uint64_t>(long_val);
    
    double result;
    std::memcpy(&result, &ulong_val, sizeof(double));
    return result;
}

std::string DataInput::read_utf() {
    uint16_t length = read_short();
    std::string result(length, '\0');
    
    for (uint16_t i = 0; i < length; ++i) {
        result[i] = static_cast<char>(read_byte());
    }
    
    return result;
}

std::string DataInput::read_interned_utf() {
    uint16_t index = read_short();
    
    if (index == 0xFFFF) {
        std::string str = read_utf();
        interned_strings.push_back(str);
        return str;
    }
    
    if (index >= interned_strings.size()) {
        throw AbxError("Invalid interned string index: " + std::to_string(index));
    }
    
    return interned_strings[index];
}

std::vector<uint8_t> DataInput::read_bytes(uint16_t length) {
    std::vector<uint8_t> data(length);
    
    for (uint16_t i = 0; i < length; ++i) {
        data[i] = read_byte();
    }
    
    return data;
}


BinaryXmlDeserializer::BinaryXmlDeserializer(std::istream& reader, std::ostream& writer)
    : input(reader), output(writer) {
    
    std::array<uint8_t, 4> magic;
    for (auto& byte : magic) {
        char c;
        if (!reader.get(c)) {
            throw AbxError("Failed to read magic header");
        }
        byte = static_cast<uint8_t>(c);
    }
    
    if (!std::equal(magic.begin(), magic.end(), std::begin(PROTOCOL_MAGIC_VERSION_0))) {
        throw AbxError("Invalid ABX file format - magic header mismatch");
    }
}

void BinaryXmlDeserializer::deserialize() {
    output << R"(<?xml version="1.0" encoding="UTF-8"?>)";
    
    while (true) {
        try {
            if (!process_token()) {
                break;
            }
        } catch (const AbxError&) {
            break;
        }
    }
}

bool BinaryXmlDeserializer::process_token() {
    uint8_t token = input.read_byte();
    uint8_t command = token & 0x0F;
    uint8_t type_info = token & 0xF0;
    
    switch (command) {
        case START_DOCUMENT:
            return true;
            
        case END_DOCUMENT:
            return false;
            
        case START_TAG: {
            auto tag_name = input.read_interned_utf();
            output << '<' << tag_name;
            
            while (true) {
                try {
                    uint8_t next_token = input.peek_byte();
                    if ((next_token & 0x0F) == ATTRIBUTE) {
                        
                        (void)input.read_byte();  
                        process_attribute(next_token);
                    } else {
                        break;
                    }
                } catch (const AbxError&) {
                    break;
                }
            }
            
            output << '>';
            return true;
        }
        
        case END_TAG: {
            auto tag_name = input.read_interned_utf();
            output << "</" << tag_name << '>';
            return true;
        }
        
        case TEXT:
            if (type_info == TYPE_STRING) {
                auto text = input.read_utf();
                if (!text.empty()) {
                    output << encode_xml_entities(text);
                }
            }
            return true;
            
        case CDSECT:
            if (type_info == TYPE_STRING) {
                auto text = input.read_utf();
                output << "<![CDATA[" << text << "]]>";
            }
            return true;
            
        case COMMENT:
            if (type_info == TYPE_STRING) {
                auto text = input.read_utf();
                output << "<!--" << text << "-->";
            }
            return true;
            
        case PROCESSING_INSTRUCTION:
            if (type_info == TYPE_STRING) {
                auto text = input.read_utf();
                output << "<?" << text << "?>";
            }
            return true;
            
        case DOCDECL:
            if (type_info == TYPE_STRING) {
                auto text = input.read_utf();
                output << "<!DOCTYPE " << text << '>';
            }
            return true;
            
        case ENTITY_REF:
            if (type_info == TYPE_STRING) {
                auto text = input.read_utf();
                output << '&' << text << ';';
            }
            return true;
            
        case IGNORABLE_WHITESPACE:
            if (type_info == TYPE_STRING) {
                auto text = input.read_utf();
                output << text;
            }
            return true;
            
        default:
            return true;
    }
}

void BinaryXmlDeserializer::process_attribute(uint8_t token) {
    uint8_t type_info = token & 0xF0;
    auto name = input.read_interned_utf();
    
    output << ' ' << name << "=\"";
    
    switch (type_info) {
        case TYPE_STRING: {
            auto value = input.read_utf();
            output << encode_xml_entities(value);
            break;
        }
        
        case TYPE_STRING_INTERNED: {
            auto value = input.read_interned_utf();
            output << encode_xml_entities(value);
            break;
        }
        
        case TYPE_INT: {
            int32_t value = input.read_int();
            output << value;
            break;
        }
        
        case TYPE_INT_HEX: {
            int32_t value = input.read_int();
            if (value == -1) {
                output << value;
            } else {
                output << std::hex << static_cast<uint32_t>(value) << std::dec;
            }
            break;
        }
        
        case TYPE_LONG: {
            int64_t value = input.read_long();
            output << value;
            break;
        }
        
        case TYPE_LONG_HEX: {
            int64_t value = input.read_long();
            if (value == -1) {
                output << value;
            } else {
                output << std::hex << static_cast<uint64_t>(value) << std::dec;
            }
            break;
        }
        
        case TYPE_FLOAT: {
            float value = input.read_float();
            if (std::floor(value) == value && std::isfinite(value)) {
                output << std::fixed << std::setprecision(1) << value;
            } else {
                output << value;
            }
            output << std::defaultfloat;
            break;
        }
        
        case TYPE_DOUBLE: {
            double value = input.read_double();
            if (std::floor(value) == value && std::isfinite(value)) {
                output << std::fixed << std::setprecision(1) << value;
            } else {
                output << value;
            }
            output << std::defaultfloat;
            break;
        }
        
        case TYPE_BOOLEAN_TRUE:
            output << "true";
            break;
            
        case TYPE_BOOLEAN_FALSE:
            output << "false";
            break;
            
        case TYPE_BYTES_HEX: {
            uint16_t length = input.read_short();
            auto bytes = input.read_bytes(length);
            output << hex_encode(bytes.data(), bytes.size());
            break;
        }
        
        case TYPE_BYTES_BASE64: {
            uint16_t length = input.read_short();
            auto bytes = input.read_bytes(length);
            output << base64_encode(bytes.data(), bytes.size());
            break;
        }
        
        default:
            throw AbxError("Unknown attribute type: " + std::to_string(type_info));
    }
    
    output << '"';
}


void AbxToXmlConverter::convert(std::istream& input, std::ostream& output) {
    BinaryXmlDeserializer deserializer(input, output);
    deserializer.deserialize();
}

void AbxToXmlConverter::convert_file(const std::filesystem::path& input_path, 
                                     const std::filesystem::path& output_path) {
    if (input_path == output_path) {
        
        std::ifstream infile(input_path, std::ios::binary);
        if (!infile) {
            throw AbxError("Failed to open input file: " + input_path.string());
        }
        
        std::ostringstream buffer;
        convert(infile, buffer);
        infile.close();
        
        std::ofstream outfile(output_path, std::ios::binary);
        if (!outfile) {
            throw AbxError("Failed to open output file: " + output_path.string());
        }
        outfile << buffer.str();
    } else {
        std::ifstream infile(input_path, std::ios::binary);
        if (!infile) {
            throw AbxError("Failed to open input file: " + input_path.string());
        }
        
        std::ofstream outfile(output_path, std::ios::binary);
        if (!outfile) {
            throw AbxError("Failed to open output file: " + output_path.string());
        }
        
        convert(infile, outfile);
    }
}

void AbxToXmlConverter::convert_stdin_stdout() {
    convert(std::cin, std::cout);
}

void AbxToXmlConverter::convert_stdin_to_file(const std::filesystem::path& output_path) {
    std::ofstream outfile(output_path, std::ios::binary);
    if (!outfile) {
        throw AbxError("Failed to open output file: " + output_path.string());
    }
    convert(std::cin, outfile);
}

void AbxToXmlConverter::convert_file_to_stdout(const std::filesystem::path& input_path) {
    std::ifstream infile(input_path, std::ios::binary);
    if (!infile) {
        throw AbxError("Failed to open input file: " + input_path.string());
    }
    convert(infile, std::cout);
}

std::string AbxToXmlConverter::convert_bytes(const uint8_t* data, size_t length) {
    std::string input_str(reinterpret_cast<const char*>(data), length);
    std::istringstream input(input_str);
    std::ostringstream output;
    convert(input, output);
    return output.str();
}

std::string AbxToXmlConverter::convert_string(std::string_view abx_data) {
    return convert_bytes(reinterpret_cast<const uint8_t*>(abx_data.data()), abx_data.size());
}

} 