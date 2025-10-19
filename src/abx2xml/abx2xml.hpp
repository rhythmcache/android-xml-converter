#ifndef ABX2XML_HPP
#define ABX2XML_HPP

#include <iostream>
#include <vector>
#include <string>
#include <string_view>
#include <optional>
#include <cstdint>
#include <stdexcept>
#include <filesystem>

namespace abx2xml {


inline constexpr uint8_t PROTOCOL_MAGIC_VERSION_0[4] = {0x41, 0x42, 0x58, 0x00};


inline constexpr uint8_t START_DOCUMENT = 0;
inline constexpr uint8_t END_DOCUMENT = 1;
inline constexpr uint8_t START_TAG = 2;
inline constexpr uint8_t END_TAG = 3;
inline constexpr uint8_t TEXT = 4;
inline constexpr uint8_t CDSECT = 5;
inline constexpr uint8_t ENTITY_REF = 6;
inline constexpr uint8_t IGNORABLE_WHITESPACE = 7;
inline constexpr uint8_t PROCESSING_INSTRUCTION = 8;
inline constexpr uint8_t COMMENT = 9;
inline constexpr uint8_t DOCDECL = 10;
inline constexpr uint8_t ATTRIBUTE = 15;


inline constexpr uint8_t TYPE_STRING = 2 << 4;
inline constexpr uint8_t TYPE_STRING_INTERNED = 3 << 4;
inline constexpr uint8_t TYPE_BYTES_HEX = 4 << 4;
inline constexpr uint8_t TYPE_BYTES_BASE64 = 5 << 4;
inline constexpr uint8_t TYPE_INT = 6 << 4;
inline constexpr uint8_t TYPE_INT_HEX = 7 << 4;
inline constexpr uint8_t TYPE_LONG = 8 << 4;
inline constexpr uint8_t TYPE_LONG_HEX = 9 << 4;
inline constexpr uint8_t TYPE_FLOAT = 10 << 4;
inline constexpr uint8_t TYPE_DOUBLE = 11 << 4;
inline constexpr uint8_t TYPE_BOOLEAN_TRUE = 12 << 4;
inline constexpr uint8_t TYPE_BOOLEAN_FALSE = 13 << 4;


class AbxError : public std::runtime_error {
public:
    explicit AbxError(const std::string& msg) : std::runtime_error(msg) {}
};


[[nodiscard]] std::string encode_xml_entities(std::string_view text);
[[nodiscard]] std::string hex_encode(const uint8_t* data, size_t len);
[[nodiscard]] std::string base64_encode(const uint8_t* data, size_t len);


class DataInput {
private:
    std::istream& reader;
    std::vector<std::string> interned_strings;
    std::optional<uint8_t> peeked_byte;
    
public:
    explicit DataInput(std::istream& input);
    
    [[nodiscard]] uint8_t read_byte();
    [[nodiscard]] uint8_t peek_byte();
    [[nodiscard]] uint16_t read_short();
    [[nodiscard]] int32_t read_int();
    [[nodiscard]] int64_t read_long();
    [[nodiscard]] float read_float();
    [[nodiscard]] double read_double();
    [[nodiscard]] std::string read_utf();
    [[nodiscard]] std::string read_interned_utf();
    [[nodiscard]] std::vector<uint8_t> read_bytes(uint16_t length);
    
    [[nodiscard]] const std::vector<std::string>& get_interned_strings() const noexcept {
        return interned_strings;
    }
};


class BinaryXmlDeserializer {
private:
    DataInput input;
    std::ostream& output;
    
    [[nodiscard]] bool process_token();
    void process_attribute(uint8_t token);
    
public:
    BinaryXmlDeserializer(std::istream& reader, std::ostream& writer);
    void deserialize();
};


class AbxToXmlConverter {
public:
    static void convert(std::istream& input, std::ostream& output);
    static void convert_file(const std::filesystem::path& input_path, 
                            const std::filesystem::path& output_path);
    static void convert_stdin_stdout();
    static void convert_stdin_to_file(const std::filesystem::path& output_path);
    static void convert_file_to_stdout(const std::filesystem::path& input_path);
    [[nodiscard]] static std::string convert_bytes(const uint8_t* data, size_t length);
    [[nodiscard]] static std::string convert_string(std::string_view abx_data);
};

} 

#endif 