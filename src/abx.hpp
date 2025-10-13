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

/**
 * @namespace libabx
 * @brief Binary XML serialization library providing conversion between
 *        XML and a ABX.
 *
 * simple experimental library for converting XML documents to Android
 * binary format and back. It provides both high-level convenience functions and
 * low-level APIs for fine-grained control over the serialization process.
 *
 *
 * @section compat COMPATIBILITY NOTICE
 * 
 * - **ABX → XML (Deserialization)**: Compatible with Android system files.
 * 
 * - **XML → ABX (Serialization)**: May produce different output than Android's official
 *   BinaryXmlSerializer. This library uses automatic type inference while Android uses
 *   explicit type methods chosen by the programmer.
 *   
 *   Recommendation: For generating ABX files that Android will consume, use the
 *   BinaryXmlSerializer class directly with explicit type methods.
 *
 * ### XML File to ABX File
 * @code
 * #include <fstream>
 * #include "abx.hpp"
 *
 * std::ofstream abx_file("output.abx", std::ios::binary);
 * libabx::convertXmlFileToAbx("input.xml", abx_file);
 * abx_file.close();
 * @endcode
 *
 * ### ABX File to XML File
 * @code
 * std::ifstream abx_file("input.abx", std::ios::binary);
 * libabx::convertAbxToXmlFile(abx_file, "output.xml");
 * abx_file.close();
 * @endcode
 *
 * ### XML String to ABX
 * @code
 * std::ostringstream abx_stream;
 * libabx::convertXmlStringToAbx("<root><item>value</item></root>", abx_stream);
 * @endcode
 *
 * ### Advanced: Custom Serialization with Warning Callback
 * @code
 * auto warning_handler = [](const std::string& category, const std::string& msg) {
 *     std::cerr << "[" << category << "] " << msg << std::endl;
 * };
 *
 * libabx::XmlToAbxOptions opts;
 * opts.warning_callback = warning_handler;
 * opts.collapse_whitespaces = true;
 *
 * std::ofstream abx_file("output.abx", std::ios::binary);
 * libabx::convertXmlFileToAbx("input.xml", abx_file, opts);
 * abx_file.close();
 * @endcode
 */
namespace libabx {

/**
 * @typedef WarningCallback
 * @brief Callback function for handling warnings during XML parsing.
 *
 * The callback receives two parameters:
 * - @param category A string describing the category of warning (e.g. "Namespaces and prefixes")
 * - @param message A descriptive message about the warning
 *
 * This is useful for logging namespace usage, unsupported features, or other
 * non-fatal issues encountered during conversion.
 *
 * @example
 * @code
 * auto handler = [](const std::string& cat, const std::string& msg) {
 *     std::cout << "Warning [" << cat << "]: " << msg << std::endl;
 * };
 * @endcode
 */
using WarningCallback =
    std::function<void(const std::string& category, const std::string& message)>;

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

/**
 * @defgroup Utilities Utility Functions
 * @brief Low-level encoding and decoding utilities.
 * @{
 */

/**
 * @brief Encode binary data as base64.
 *
 * Converts raw binary data into a base64-encoded string suitable for
 * embedding in text formats.
 *
 * @param data Pointer to the binary data to encode
 * @param len Length of the binary data in bytes
 * @return Base64-encoded string representation of the data
 */
std::string base64_encode(const unsigned char* data, size_t len);

/**
 * @brief Decode a base64-encoded string into binary data.
 *
 * Converts a base64 string back into the original binary form. Padding
 * characters ('=') are handled automatically.
 *
 * @param data Base64-encoded string
 * @return Vector of bytes containing decoded data
 * @throws std::runtime_error if decoding fails
 */
std::vector<uint8_t> base64_decode(const std::string& data);

/**
 * @brief Decode a hexadecimal string into binary data.
 *
 * Converts a hex string (e.g. "DEADBEEF") into its binary representation.
 * The input must have even length (two hex characters per byte).
 *
 * @param hex Hexadecimal string (case-insensitive)
 * @return Vector of bytes containing decoded data
 * @throws std::runtime_error if hex string has odd length
 */
std::vector<uint8_t> hex_decode(const std::string& hex);

/**
 * @brief Encode XML entities in a string.
 *
 * Escapes special XML characters: &, <, >, ", and '. This is used internally
 * to ensure text content is properly escaped when serialized to XML.
 *
 * Replacements:
 * - & → &amp;
 * - < → &lt;
 * - > → &gt;
 * - " → &quot;
 * - ' → &apos;
 *
 * @param text String to encode
 * @return String with XML entities properly escaped
 */
std::string encode_xml_entities(const std::string& text);

/** @} */

// ============================================================================
// STREAM WRAPPERS - Low-level binary I/O
// ============================================================================

/**
 * @defgroup StreamIO Stream I/O Wrappers
 * @brief Low-level binary data input/output with automatic endianness handling.
 * @{
 */

/**
 * @class FastDataInput
 * @brief High-performance binary stream input with support for various data types.
 *
 * Provides type-safe reading of binary data with automatic big-endian to native
 * conversion. Also handles string interning for efficient deduplication of
 * repeated strings.
 *
 * @note This class is primarily for internal use. Consider using the high-level
 *       API functions for most use cases.
 */
class FastDataInput {
   private:
    std::istream& input_stream;
    std::vector<std::string> interned_strings;

   public:
    /**
     * @brief Construct a FastDataInput wrapper around an input stream.
     *
     * @param is Reference to the input stream to wrap
     */
    explicit FastDataInput(std::istream& is);

    /**
     * @brief Read a single unsigned byte.
     *
     * @return Value in range [0, 255]
     * @throws std::runtime_error if read fails
     */
    uint8_t readByte();

    /**
     * @brief Read a 16-bit unsigned integer in big-endian format.
     *
     * @return Value in range [0, 65535]
     * @throws std::runtime_error if read fails
     */
    uint16_t readShort();

    /**
     * @brief Read a 32-bit signed integer in big-endian format.
     *
     * @return Value in range [-2^31, 2^31-1]
     * @throws std::runtime_error if read fails
     */
    int32_t readInt();

    /**
     * @brief Read a 64-bit signed integer in big-endian format.
     *
     * @return Value in range [-2^63, 2^63-1]
     * @throws std::runtime_error if read fails
     */
    int64_t readLong();

    /**
     * @brief Read a 32-bit IEEE 754 floating point value.
     *
     * @return Float value
     * @throws std::runtime_error if read fails
     */
    float readFloat();

    /**
     * @brief Read a 64-bit IEEE 754 double precision value.
     *
     * @return Double value
     * @throws std::runtime_error if read fails
     */
    double readDouble();

    /**
     * @brief Read a UTF-8 string with length prefix.
     *
     * The string is prefixed with a 16-bit length field containing the byte count.
     *
     * @return The decoded UTF-8 string
     * @throws std::runtime_error if read fails
     */
    std::string readUTF();

    /**
     * @brief Read a string reference from the interning pool or a new string.
     *
     * If the value 0xFFFF is encountered, a new string is read and added to the
     * interning pool. Otherwise, a reference to a previously interned string is
     * returned.
     *
     * @return String value (either from pool or newly read)
     * @throws std::runtime_error if index is invalid
     */
    std::string readInternedUTF();

    /**
     * @brief Read a fixed number of bytes.
     *
     * @param length Number of bytes to read
     * @return Vector containing the read bytes
     * @throws std::runtime_error if read fails
     */
    std::vector<uint8_t> readBytes(uint16_t length);

    /**
     * @brief Check if the stream has reached end-of-file.
     *
     * @return true if at EOF, false otherwise
     */
    bool eof() const;

    /**
     * @brief Get the current read position in the stream.
     *
     * @return Current stream position
     */
    std::streampos tellg();

    /**
     * @brief Seek to a specific position in the stream.
     *
     * @param pos Target position
     */
    void seekg(std::streampos pos);
};

/**
 * @class FastDataOutput
 * @brief High-performance binary stream output with automatic endianness conversion.
 *
 * Provides type-safe writing of binary data with automatic native to big-endian
 * conversion. Also manages string interning to avoid writing duplicate strings.
 *
 * @note This class is primarily for internal use. Consider using the high-level
 *       API functions for most use cases.
 */
class FastDataOutput {
   private:
    std::ostream& output_stream;
    std::unordered_map<std::string, uint16_t> string_pool;
    std::vector<std::string> interned_strings;

   public:
    /**
     * @brief Maximum size for a single value (2^16 - 1).
     *
     * This constant limits the size of strings, byte arrays, and other
     * variable-length data that use a 16-bit length prefix.
     */
    static const uint16_t MAX_UNSIGNED_SHORT = 65535;

    /**
     * @brief Construct a FastDataOutput wrapper around an output stream.
     *
     * @param os Reference to the output stream to wrap
     */
    explicit FastDataOutput(std::ostream& os);

    /**
     * @brief Write a single unsigned byte.
     *
     * @param value Value to write (0-255)
     */
    void writeByte(uint8_t value);

    /**
     * @brief Write a 16-bit unsigned integer in big-endian format.
     *
     * @param value Value to write
     */
    void writeShort(uint16_t value);

    /**
     * @brief Write a 32-bit signed integer in big-endian format.
     *
     * @param value Value to write
     */
    void writeInt(int32_t value);

    /**
     * @brief Write a 64-bit signed integer in big-endian format.
     *
     * @param value Value to write
     */
    void writeLong(int64_t value);

    /**
     * @brief Write a 32-bit IEEE 754 floating point value.
     *
     * @param value Float to write
     */
    void writeFloat(float value);

    /**
     * @brief Write a 64-bit IEEE 754 double precision value.
     *
     * @param value Double to write
     */
    void writeDouble(double value);

    /**
     * @brief Write a UTF-8 string with length prefix.
     *
     * The string is prefixed with a 16-bit length field containing the byte count.
     * Maximum string length is 65535 bytes.
     *
     * @param str String to write
     * @throws std::runtime_error if string length exceeds MAX_UNSIGNED_SHORT
     */
    void writeUTF(const std::string& str);

    /**
     * @brief Write a string with automatic interning.
     *
     * If the string has been written before, only a reference index is written.
     * Otherwise, 0xFFFF is written followed by the string, and the string is
     * added to the interning pool.
     *
     * @param str String to write
     * @throws std::runtime_error if string pool overflow
     */
    void writeInternedUTF(const std::string& str);

    /**
     * @brief Write raw binary data.
     *
     * @param data Vector of bytes to write
     */
    void write(const std::vector<uint8_t>& data);

    /**
     * @brief Flush the output stream to ensure all data is written.
     */
    void flush();
};

/** @} */

// ============================================================================
// CORE SERIALIZER/DESERIALIZER - Pure stream-based conversion
// ============================================================================

/**
 * @defgroup CoreSerialization Core Serialization Classes
 * @brief Low-level binary XML format handling.
 * @{
 */

/**
 * @class BinaryXmlSerializer
 * @brief Serializes XML data into ABX binary format.
 *
 * This class handles the conversion from XML logical structure into the binary
 * ABX format. It manages token generation, type encoding, and string interning.
 *
 * The ABX format consists of:
 * - A 4-byte magic header: 0x41 0x42 0x58 0x00 ("ABX\0")
 * - A sequence of tokens, each with a command (low nibble) and type (high nibble)
 * - Type-specific data following each token
 *
 * @note Typically used indirectly through high-level API functions.
 *
 * @example
 * @code
 * std::ofstream file("output.abx", std::ios::binary);
 * libabx::BinaryXmlSerializer serializer(file);
 * serializer.startDocument();
 * serializer.startTag("root");
 * serializer.attribute("id", "123");
 * serializer.text("Hello World");
 * serializer.endTag("root");
 * serializer.endDocument();
 * file.close();
 * @endcode
 */
class BinaryXmlSerializer {
   public:
    // Magic number identifying ABX format version 0
    static const uint8_t PROTOCOL_MAGIC_VERSION_0[4];

    // Token command types (low nibble)
    static const uint8_t START_DOCUMENT = 0;    ///< Begin XML document
    static const uint8_t END_DOCUMENT = 1;      ///< End XML document
    static const uint8_t START_TAG = 2;         ///< Begin element
    static const uint8_t END_TAG = 3;           ///< End element
    static const uint8_t TEXT = 4;              ///< Text content
    static const uint8_t CDSECT = 5;            ///< CDATA section
    static const uint8_t ENTITY_REF = 6;        ///< Entity reference
    static const uint8_t IGNORABLE_WHITESPACE = 7;  ///< Whitespace outside elements
    static const uint8_t PROCESSING_INSTRUCTION = 8; ///< XML processing instruction
    static const uint8_t COMMENT = 9;           ///< XML comment
    static const uint8_t DOCDECL = 10;          ///< DOCTYPE declaration
    static const uint8_t ATTRIBUTE = 15;        ///< Element attribute

    // Token type encoding (high nibble, shifted left by 4)
    static const uint8_t TYPE_NULL = 1 << 4;           ///< No data
    static const uint8_t TYPE_STRING = 2 << 4;         ///< UTF-8 string
    static const uint8_t TYPE_STRING_INTERNED = 3 << 4; ///< Interned string reference
    static const uint8_t TYPE_BYTES_HEX = 4 << 4;      ///< Binary data as hex string
    static const uint8_t TYPE_BYTES_BASE64 = 5 << 4;   ///< Binary data as base64
    static const uint8_t TYPE_INT = 6 << 4;            ///< 32-bit signed integer
    static const uint8_t TYPE_INT_HEX = 7 << 4;        ///< 32-bit integer displayed as hex
    static const uint8_t TYPE_LONG = 8 << 4;           ///< 64-bit signed integer
    static const uint8_t TYPE_LONG_HEX = 9 << 4;       ///< 64-bit integer displayed as hex
    static const uint8_t TYPE_FLOAT = 10 << 4;         ///< 32-bit floating point
    static const uint8_t TYPE_DOUBLE = 11 << 4;        ///< 64-bit floating point
    static const uint8_t TYPE_BOOLEAN_TRUE = 12 << 4;  ///< Boolean true
    static const uint8_t TYPE_BOOLEAN_FALSE = 13 << 4; ///< Boolean false

   private:
    std::unique_ptr<FastDataOutput> mOut;
    int mTagCount = 0;
    std::vector<std::string> mTagNames;

    void writeToken(uint8_t token, const std::string* text = nullptr);

   public:
    /**
     * @brief Construct a BinaryXmlSerializer wrapping an output stream.
     *
     * @param os Output stream for writing binary ABX data
     */
    explicit BinaryXmlSerializer(std::ostream& os);

    /**
     * @brief Write XML document start marker.
     *
     * Should be called once at the beginning of serialization.
     */
    void startDocument();

    /**
     * @brief Write XML document end marker.
     *
     * Should be called once at the end of serialization. Also flushes the output stream.
     */
    void endDocument();

    /**
     * @brief Begin an XML element.
     *
     * @param name Tag name (element name)
     * @throws std::runtime_error if tag stack overflows
     */
    void startTag(const std::string& name);

    /**
     * @brief End an XML element.
     *
     * @param name Tag name (must match the corresponding startTag)
     * @throws std::runtime_error if tag name mismatch or no matching startTag
     */
    void endTag(const std::string& name);

    /**
     * @brief Add an attribute with string value.
     *
     * @param name Attribute name
     * @param value Attribute value as string
     */
    void attribute(const std::string& name, const std::string& value);

    /**
     * @brief Add an attribute with interned string value.
     *
     * @param name Attribute name
     * @param value Attribute value (will be deduplicated if repeated)
     */
    void attributeInterned(const std::string& name, const std::string& value);

    /**
     * @brief Add an attribute containing binary data encoded as hexadecimal.
     *
     * @param name Attribute name
     * @param value Binary data
     * @throws std::runtime_error if value size exceeds MAX_UNSIGNED_SHORT
     */
    void attributeBytesHex(const std::string& name, const std::vector<uint8_t>& value);

    /**
     * @brief Add an attribute containing binary data encoded as base64.
     *
     * @param name Attribute name
     * @param value Binary data
     * @throws std::runtime_error if value size exceeds MAX_UNSIGNED_SHORT
     */
    void attributeBytesBase64(const std::string& name, const std::vector<uint8_t>& value);

    /**
     * @brief Add an attribute with 32-bit signed integer value.
     *
     * @param name Attribute name
     * @param value Integer value
     */
    void attributeInt(const std::string& name, int32_t value);

    /**
     * @brief Add an attribute with 32-bit integer value displayed as hexadecimal.
     *
     * @param name Attribute name
     * @param value Integer value
     */
    void attributeIntHex(const std::string& name, int32_t value);

    /**
     * @brief Add an attribute with 64-bit signed integer value.
     *
     * @param name Attribute name
     * @param value Long integer value
     */
    void attributeLong(const std::string& name, int64_t value);

    /**
     * @brief Add an attribute with 64-bit integer value displayed as hexadecimal.
     *
     * @param name Attribute name
     * @param value Long integer value
     */
    void attributeLongHex(const std::string& name, int64_t value);

    /**
     * @brief Add an attribute with 32-bit floating point value.
     *
     * @param name Attribute name
     * @param value Float value
     */
    void attributeFloat(const std::string& name, float value);

    /**
     * @brief Add an attribute with 64-bit floating point (double) value.
     *
     * @param name Attribute name
     * @param value Double value
     */
    void attributeDouble(const std::string& name, double value);

    /**
     * @brief Add an attribute with boolean value.
     *
     * @param name Attribute name
     * @param value Boolean value
     */
    void attributeBoolean(const std::string& name, bool value);

    /**
     * @brief Add text content to the current element.
     *
     * @param text Text content (will be XML-entity-encoded automatically)
     */
    void text(const std::string& text);

    /**
     * @brief Add a CDATA section.
     *
     * @param text CDATA content (special characters are preserved)
     */
    void cdsect(const std::string& text);

    /**
     * @brief Add an XML comment.
     *
     * @param text Comment content
     */
    void comment(const std::string& text);

    /**
     * @brief Add an XML processing instruction.
     *
     * @param target Processing instruction target (e.g., "xml-stylesheet")
     * @param data Optional processing instruction data (default: empty)
     */
    void processingInstruction(const std::string& target, const std::string& data = "");

    /**
     * @brief Add a DOCTYPE declaration.
     *
     * @param text DOCTYPE declaration content
     */
    void docdecl(const std::string& text);

    /**
     * @brief Add ignorable whitespace (whitespace outside elements).
     *
     * @param text Whitespace content
     */
    void ignorableWhitespace(const std::string& text);

    /**
     * @brief Add an entity reference.
     *
     * @param text Entity name (without & and ;)
     */
    void entityRef(const std::string& text);
};

/**
 * @class BinaryXmlDeserializer
 * @brief Deserializes ABX binary format back into XML.
 *
 * This class handles the conversion from ABX binary format into XML output.
 * It reads tokens and type information and reconstructs the XML document.
 *
 * @note Typically used indirectly through high-level API functions.
 *
 * @example
 * @code
 * std::ifstream file("input.abx", std::ios::binary);
 * std::ofstream xml_out("output.xml");
 * libabx::BinaryXmlDeserializer deserializer(file, xml_out);
 * deserializer.deserialize();
 * file.close();
 * xml_out.close();
 * @endcode
 */
class BinaryXmlDeserializer {
   public:
    // Magic number identifying ABX format version 0
    static const uint8_t PROTOCOL_MAGIC_VERSION_0[4];

    // Token command types (low nibble)
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

    // Token type encoding (high nibble, shifted left by 4)
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
    /**
     * @brief Construct a BinaryXmlDeserializer wrapping input and output streams.
     *
     * @param is Input stream containing ABX binary data
     * @param os Output stream for writing XML data
     * @throws std::runtime_error if magic header is invalid
     */
    BinaryXmlDeserializer(std::istream& is, std::ostream& os);

    /**
     * @brief Deserialize the ABX binary format to XML.
     *
     * Reads the entire ABX stream and writes the reconstructed XML to the output stream.
     *
     * @throws std::runtime_error if parsing fails
     */
    void deserialize();
};

/** @} */

// ============================================================================
// XML PARSING LAYER
// ============================================================================

/**
 * @defgroup XMLParsing XML Parsing Configuration
 * @brief Options for controlling XML-to-ABX conversion behavior.
 * @{
 */

/**
 * @struct XmlToAbxOptions
 * @brief Configuration options for XML to ABX conversion.
 *
 * Allows customization of how XML is converted to the binary format.
 */
struct XmlToAbxOptions {
    /**
     * @brief If true, collapse insignificant whitespace in element content.
     *
     * When enabled, whitespace-only text nodes between tags are ignored.
     * Whitespace within text content is always preserved.
     *
     * Default: false (preserve all whitespace)
     */
    bool collapse_whitespaces = false;

    /**
     * @brief Optional callback function for warning messages.
     *
     * Called during conversion to report non-fatal issues such as:
     * - Namespace prefixes in elements or attributes
     * - Namespace declarations
     *
     * Default: nullptr (no warnings reported)
     */
    WarningCallback warning_callback = nullptr;
};

/** @} */

// ============================================================================
// LOW-LEVEL API
// ============================================================================

/**
 * @defgroup LowLevelAPI Low-Level API
 * @brief Direct serialization of parsed XML nodes.
 *
 * Use these functions when you need to manipulate XML data before conversion,
 * such as filtering elements or transforming attributes.
 * @{
 */

/**
 * @brief Serialize a parsed XML node to ABX binary format.
 *
 * Recursively processes the given XML node and all its descendants, writing
 * the binary representation to the serializer.
 *
 * This is the core function used by the high-level API functions.
 *
 * @param serializer The serializer to write binary data to
 * @param xml_node The XML node to serialize
 * @param options Conversion options
 *
 * @example
 * @code
 * pugi::xml_document doc;
 * doc.load_file("input.xml");
 * std::ofstream output("output.abx", std::ios::binary);
 * libabx::BinaryXmlSerializer serializer(output);
 * serializer.startDocument();
 * libabx::serializeXmlToAbx(serializer, doc.document_element(), {});
 * serializer.endDocument();
 * @endcode
 */
void serializeXmlToAbx(BinaryXmlSerializer& serializer, const pugi::xml_node& xml_node,
                       const XmlToAbxOptions& options);

/** @} */

// ============================================================================
// HIGH-LEVEL API
// ============================================================================

/**
 * @defgroup HighLevelAPI High-Level API
 * @brief Convenient file and string conversion functions.
 *
 * These functions handle all parsing, serialization, and I/O automatically.
 * Use these for most common use cases.
 * @{
 */

/**
 * @brief Convert an XML file to ABX binary format.
 *
 * Loads the XML file, automatically infers data types in attributes,
 * and writes the binary result to the output stream.
 *
 * @param xml_path Path to the XML file to convert
 * @param abx_output Output stream for binary ABX data
 * @param options Conversion options (default: empty options)
 *
 * @throws std::runtime_error if file cannot be opened or XML parsing fails
 *
 * @example
 * @code
 * std::ofstream output("config.abx", std::ios::binary);
 * libabx::convertXmlFileToAbx("config.xml", output);
 * output.close();
 * @endcode
 */
void convertXmlFileToAbx(const std::string& xml_path, std::ostream& abx_output,
                         const XmlToAbxOptions& options = {});

/**
 * @brief Convert an XML string to ABX binary format.
 *
 * Parses the XML string directly (without loading from disk), infers data types
 * in attributes, and writes the binary result to the output stream.
 *
 * @param xml_string XML content as a string
 * @param abx_output Output stream for binary ABX data
 * @param options Conversion options (default: empty options)
 *
 * @throws std::runtime_error if XML parsing fails
 *
 * @example
 * @code
 * std::ostringstream output;
 * std::string xml = "<root><item id=\"42\">value</item></root>";
 * libabx::convertXmlStringToAbx(xml, output);
 * @endcode
 */
void convertXmlStringToAbx(const std::string& xml_string,
                           std::ostream& abx_output,
                           const XmlToAbxOptions& options = {});

/**
 * @brief Convert an ABX binary stream to an XML file.
 *
 * Reads ABX binary data from the input stream and writes the reconstructed
 * XML document to the specified file path.
 *
 * @param abx_input Input stream containing binary ABX data
 * @param xml_path Path where the XML file will be written
 *
 * @throws std::runtime_error if input is invalid or output file cannot be opened
 *
 * @example
 * @code
 * std::ifstream input("data.abx", std::ios::binary);
 * libabx::convertAbxToXmlFile(input, "output.xml");
 * input.close();
 * @endcode
 */
void convertAbxToXmlFile(std::istream& abx_input, const std::string& xml_path);

/**
 * @brief Convert an ABX binary stream to an XML string.
 *
 * Reads ABX binary data from the input stream and returns the reconstructed
 * XML document as a string. Useful for in-memory processing without file I/O.
 *
 * @param abx_input Input stream containing binary ABX data
 * @return Reconstructed XML as a string
 *
 * @throws std::runtime_error if input is invalid
 *
 * @example
 * @code
 * std::ifstream input("data.abx", std::ios::binary);
 * std::string xml = libabx::convertAbxToXmlString(input);
 * input.close();
 * std::cout << xml << std::endl;
 * @endcode
 */
std::string convertAbxToXmlString(std::istream& abx_input);

/** @} */

}  // namespace libabx

#endif  // LIBABX_H