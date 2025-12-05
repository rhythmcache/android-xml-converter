#ifndef XML2ABX_HPP
#define XML2ABX_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <ostream>
#include <stdexcept>
#include <optional>

namespace xml2abx {


class ConversionError : public std::runtime_error {
public:
    explicit ConversionError(const std::string& msg) : std::runtime_error(msg) {}
};

class StringTooLongError : public ConversionError {
public:
    StringTooLongError(size_t actual, size_t max)
        : ConversionError("String too long: " + std::to_string(actual) + 
                         " bytes (max: " + std::to_string(max) + ")") {}
};

class BinaryDataTooLongError : public ConversionError {
public:
    BinaryDataTooLongError(size_t actual, size_t max)
        : ConversionError("Binary data too long: " + std::to_string(actual) + 
                         " bytes (max: " + std::to_string(max) + ")") {}
};


void show_warning(const std::string& feature, const std::optional<std::string>& details = std::nullopt);


class FastDataOutput {
public:
    static constexpr uint16_t MAX_UNSIGNED_SHORT = 65535;

    explicit FastDataOutput(std::ostream& output);

    void write_byte(uint8_t value);
    void write_short(uint16_t value);
    void write_int(int32_t value);
    void write_long(int64_t value);
    void write_float(float value);
    void write_double(double value);
    void write_utf(const std::string& s);
    void write_interned_utf(const std::string& s);
    void write_bytes(const uint8_t* data, size_t length);
    void flush();

private:
    std::ostream& writer_;
    std::unordered_map<std::string, uint16_t> string_pool_;
    std::vector<std::string> interned_strings_;
};


class BinaryXmlSerializer {
public:
    
    static constexpr uint8_t PROTOCOL_MAGIC_VERSION_0[4] = {0x41, 0x42, 0x58, 0x00};
    
    
    static constexpr uint8_t START_DOCUMENT = 0;
    static constexpr uint8_t END_DOCUMENT = 1;
    static constexpr uint8_t START_TAG = 2;
    static constexpr uint8_t END_TAG = 3;
    static constexpr uint8_t TEXT = 4;
    static constexpr uint8_t CDSECT = 5;
    static constexpr uint8_t ENTITY_REF = 6;
    static constexpr uint8_t IGNORABLE_WHITESPACE = 7;
    static constexpr uint8_t PROCESSING_INSTRUCTION = 8;
    static constexpr uint8_t COMMENT = 9;
    static constexpr uint8_t DOCDECL = 10;
    static constexpr uint8_t ATTRIBUTE = 15;

    
    static constexpr uint8_t TYPE_NULL = 1 << 4;
    static constexpr uint8_t TYPE_STRING = 2 << 4;
    static constexpr uint8_t TYPE_STRING_INTERNED = 3 << 4;
    static constexpr uint8_t TYPE_BYTES_HEX = 4 << 4;
    static constexpr uint8_t TYPE_BYTES_BASE64 = 5 << 4;
    static constexpr uint8_t TYPE_INT = 6 << 4;
    static constexpr uint8_t TYPE_INT_HEX = 7 << 4;
    static constexpr uint8_t TYPE_LONG = 8 << 4;
    static constexpr uint8_t TYPE_LONG_HEX = 9 << 4;
    static constexpr uint8_t TYPE_FLOAT = 10 << 4;
    static constexpr uint8_t TYPE_DOUBLE = 11 << 4;
    static constexpr uint8_t TYPE_BOOLEAN_TRUE = 12 << 4;
    static constexpr uint8_t TYPE_BOOLEAN_FALSE = 13 << 4;

    explicit BinaryXmlSerializer(std::ostream& output, bool preserve_whitespace = true);

    void start_document();
    void end_document();
    void start_tag(const std::string& name);
    void end_tag(const std::string& name);
    
    void attribute(const std::string& name, const std::string& value);
    void attribute_interned(const std::string& name, const std::string& value);
    void attribute_boolean(const std::string& name, bool value);
    void attribute_int(const std::string& name, int32_t value);
    void attribute_long(const std::string& name, int64_t value);
    void attribute_float(const std::string& name, float value);
    void attribute_double(const std::string& name, double value);
    void attribute_bytes_hex(const std::string& name, const uint8_t* data, size_t length);
    void attribute_bytes_base64(const std::string& name, const uint8_t* data, size_t length);

    void text(const std::string& text);
    void cdsect(const std::string& text);
    void comment(const std::string& text);
    void processing_instruction(const std::string& target, const std::optional<std::string>& data = std::nullopt);
    void docdecl(const std::string& text);
    void ignorable_whitespace(const std::string& text);
    void entity_ref(const std::string& text);

    bool preserve_whitespace() const { return preserve_whitespace_; }

private:
    void write_token(uint8_t token, const std::optional<std::string>& text = std::nullopt);

    FastDataOutput output_;
    size_t tag_count_;
    std::vector<std::string> tag_names_;
    bool preserve_whitespace_;
};


namespace type_detection {
    bool is_boolean(const std::string& s);
    bool is_whitespace_only(const std::string& s);
}


class XmlToAbxConverter {
public:
    static void convert_from_string(const std::string& xml, std::ostream& output, 
                                   bool preserve_whitespace = true);
    static void convert_from_file(const std::string& input_path, std::ostream& output,
                                 bool preserve_whitespace = true);

private:
    static void write_attribute(BinaryXmlSerializer& serializer, 
                              const std::string& name, 
                              const std::string& value);
};

} 

#endif 