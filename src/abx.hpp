#ifndef LIBABX_H
#define LIBABX_H

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <pugixml.hpp>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace libabx {

// callback type for warnings during XML parsing
using WarningCallback =
    std::function<void(const std::string& category, const std::string& message)>;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::string base64_encode(const unsigned char* data, size_t len);
std::vector<uint8_t> base64_decode(const std::string& data);
std::vector<uint8_t> hex_decode(const std::string& hex);
std::string encode_xml_entities(const std::string& text);

// ============================================================================
// STREAM WRAPPERS - Low-level binary I/O
// ============================================================================

class FastDataInput {
   private:
    std::istream& input_stream;
    std::vector<std::string> interned_strings;

   public:
    explicit FastDataInput(std::istream& is);

    uint8_t readByte();
    uint16_t readShort();
    int32_t readInt();
    int64_t readLong();
    float readFloat();
    double readDouble();
    std::string readUTF();
    std::string readInternedUTF();
    std::vector<uint8_t> readBytes(uint16_t length);
    bool eof() const;
    std::streampos tellg();
    void seekg(std::streampos pos);
};

class FastDataOutput {
   private:
    std::ostream& output_stream;
    std::unordered_map<std::string, uint16_t> string_pool;
    std::vector<std::string> interned_strings;

   public:
    static const uint16_t MAX_UNSIGNED_SHORT = 65535;

    explicit FastDataOutput(std::ostream& os);

    void writeByte(uint8_t value);
    void writeShort(uint16_t value);
    void writeInt(int32_t value);
    void writeLong(int64_t value);
    void writeFloat(float value);
    void writeDouble(double value);
    void writeUTF(const std::string& str);
    void writeInternedUTF(const std::string& str);
    void write(const std::vector<uint8_t>& data);
    void flush();
};

// ============================================================================
// CORE SERIALIZER/DESERIALIZER - Pure stream-based conversion
// ============================================================================

class BinaryXmlSerializer {
   public:
    static const uint8_t PROTOCOL_MAGIC_VERSION_0[4];
    static const uint8_t START_DOCUMENT = 0;
    static const uint8_t END_DOCUMENT = 1;
    static const uint8_t START_TAG = 2;
    static const uint8_t END_TAG = 3;
    static const uint8_t TEXT = 4;
    static const uint8_t CDSECT = 5;
    static const uint8_t ENTITY_REF = 6;
    static const uint8_t IGNORABLE_WHITESPACE = 7;
    static const uint8_t PROCESSING_INSTRUCTION = 8;
    static const uint8_t COMMENT = 9;
    static const uint8_t DOCDECL = 10;
    static const uint8_t ATTRIBUTE = 15;
    static const uint8_t TYPE_NULL = 1 << 4;
    static const uint8_t TYPE_STRING = 2 << 4;
    static const uint8_t TYPE_STRING_INTERNED = 3 << 4;
    static const uint8_t TYPE_BYTES_HEX = 4 << 4;
    static const uint8_t TYPE_BYTES_BASE64 = 5 << 4;
    static const uint8_t TYPE_INT = 6 << 4;
    static const uint8_t TYPE_INT_HEX = 7 << 4;
    static const uint8_t TYPE_LONG = 8 << 4;
    static const uint8_t TYPE_LONG_HEX = 9 << 4;
    static const uint8_t TYPE_FLOAT = 10 << 4;
    static const uint8_t TYPE_DOUBLE = 11 << 4;
    static const uint8_t TYPE_BOOLEAN_TRUE = 12 << 4;
    static const uint8_t TYPE_BOOLEAN_FALSE = 13 << 4;

   private:
    std::unique_ptr<FastDataOutput> mOut;
    int mTagCount = 0;
    std::vector<std::string> mTagNames;

    void writeToken(uint8_t token, const std::string* text = nullptr);

   public:
    explicit BinaryXmlSerializer(std::ostream& os);

    void startDocument();
    void endDocument();
    void startTag(const std::string& name);
    void endTag(const std::string& name);
    void attribute(const std::string& name, const std::string& value);
    void attributeInterned(const std::string& name, const std::string& value);
    void attributeBytesHex(const std::string& name, const std::vector<uint8_t>& value);
    void attributeBytesBase64(const std::string& name, const std::vector<uint8_t>& value);
    void attributeInt(const std::string& name, int32_t value);
    void attributeIntHex(const std::string& name, int32_t value);
    void attributeLong(const std::string& name, int64_t value);
    void attributeLongHex(const std::string& name, int64_t value);
    void attributeFloat(const std::string& name, float value);
    void attributeDouble(const std::string& name, double value);
    void attributeBoolean(const std::string& name, bool value);
    void text(const std::string& text);
    void cdsect(const std::string& text);
    void comment(const std::string& text);
    void processingInstruction(const std::string& target, const std::string& data = "");
    void docdecl(const std::string& text);
    void ignorableWhitespace(const std::string& text);
    void entityRef(const std::string& text);
};

class BinaryXmlDeserializer {
   public:
    static const uint8_t PROTOCOL_MAGIC_VERSION_0[4];
    static const uint8_t START_DOCUMENT = 0;
    static const uint8_t END_DOCUMENT = 1;
    static const uint8_t START_TAG = 2;
    static const uint8_t END_TAG = 3;
    static const uint8_t TEXT = 4;
    static const uint8_t CDSECT = 5;
    static const uint8_t ENTITY_REF = 6;
    static const uint8_t IGNORABLE_WHITESPACE = 7;
    static const uint8_t PROCESSING_INSTRUCTION = 8;
    static const uint8_t COMMENT = 9;
    static const uint8_t DOCDECL = 10;
    static const uint8_t ATTRIBUTE = 15;
    static const uint8_t TYPE_NULL = 1 << 4;
    static const uint8_t TYPE_STRING = 2 << 4;
    static const uint8_t TYPE_STRING_INTERNED = 3 << 4;
    static const uint8_t TYPE_BYTES_HEX = 4 << 4;
    static const uint8_t TYPE_BYTES_BASE64 = 5 << 4;
    static const uint8_t TYPE_INT = 6 << 4;
    static const uint8_t TYPE_INT_HEX = 7 << 4;
    static const uint8_t TYPE_LONG = 8 << 4;
    static const uint8_t TYPE_LONG_HEX = 9 << 4;
    static const uint8_t TYPE_FLOAT = 10 << 4;
    static const uint8_t TYPE_DOUBLE = 11 << 4;
    static const uint8_t TYPE_BOOLEAN_TRUE = 12 << 4;
    static const uint8_t TYPE_BOOLEAN_FALSE = 13 << 4;

   private:
    std::unique_ptr<FastDataInput> mIn;
    std::ostream& mOut;

    std::string bytesToHex(const std::vector<uint8_t>& bytes);
    void processAttribute(uint8_t token);

   public:
    BinaryXmlDeserializer(std::istream& is, std::ostream& os);

    void deserialize();
};

// ============================================================================
// XML PARSING LAYER
// ============================================================================

// options for XML to ABX conversion
struct XmlToAbxOptions {
    bool collapse_whitespaces = false;
    WarningCallback warning_callback = nullptr;
};

// ============================================================================
// LOW-LEVEL API
// ============================================================================

// convert a parsed XML node to ABX binary format
// Use this when you need to manipulate the XML before conversion
void serializeXmlToAbx(BinaryXmlSerializer& serializer, const pugi::xml_node& xml_node,
                       const XmlToAbxOptions& options);

// ============================================================================
// HIGH-LEVEL API
// ============================================================================

// convert XML file directly to ABX format
// this is the simplest way to convert XML to ABX
void convertXmlFileToAbx(const std::string& xml_path, std::ostream& abx_output,
                         const XmlToAbxOptions& options = {});

// convert XML string directly to ABX format
void convertXmlStringToAbx(const std::string& xml_string, std::ostream& abx_output,
                           const XmlToAbxOptions& options = {});

// convert ABX file directly to XML format
void convertAbxToXmlFile(std::istream& abx_input, const std::string& xml_path);

// Convert ABX stream to XML string
std::string convertAbxToXmlString(std::istream& abx_input);

}  // namespace libabx

#endif  // LIBABX_H