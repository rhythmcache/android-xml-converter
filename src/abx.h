/**
 * @file abx.h
 * @brief C language binding for the libabx.
 *
 * This header provides a C API for converting between XML and ABX binary format.
 * It's suitable for use in C projects, C++ projects avoiding C++ exceptions, or
 * language bindings to other languages.
 *
 * @defgroup CBinding C Language Binding
 * @brief Complete C API for XML/ABX conversion.
 *
 * The C API provides:
 * - Opaque handle-based interface
 * - Error codes for all operations
 * - Thread-safe error reporting via thread-local storage
 * - Support for both file and in-memory operations
 * - High-level convenience functions
 * - Low-level builder pattern for custom serialization
 *
 * @section usage_overview Quick Start
 *
 * ### Convert XML file to ABX file:
 * @code
 * abx_error_t err;
 * err = abx_convert_xml_file_to_abx_file("input.xml", "output.abx", NULL);
 * if (err != ABX_OK) {
 *     fprintf(stderr, "Error: %s\n", abx_get_last_error());
 * }
 * @endcode
 *
 * ### Convert ABX file to XML file:
 * @code
 * abx_error_t err;
 * err = abx_convert_abx_file_to_xml_file("input.abx", "output.xml");
 * if (err != ABX_OK) {
 *     fprintf(stderr, "Error: %s\n", abx_get_last_error());
 * }
 * @endcode
 *
 * ### Custom serialization with builder pattern:
 * @code
 * abx_error_t err = ABX_OK;
 * abx_serializer_t* ser = abx_serializer_create_file("output.abx", &err);
 * if (err != ABX_OK) {
 *     fprintf(stderr, "Failed to create serializer: %s\n", abx_get_last_error());
 *     return;
 * }
 *
 * abx_serializer_start_document(ser);
 * abx_serializer_start_tag(ser, "root");
 * abx_serializer_attribute_string(ser, "version", "1.0");
 * abx_serializer_attribute_int(ser, "count", 42);
 * abx_serializer_text(ser, "Hello World");
 * abx_serializer_end_tag(ser, "root");
 * abx_serializer_end_document(ser);
 * abx_serializer_free(ser);
 * @endcode
 *
 * ### Work with in-memory buffers:
 * @code
 * abx_error_t err = ABX_OK;
 * uint8_t buffer[4096];
 *
 * // Convert XML string to ABX buffer
 * size_t size = abx_convert_xml_string_to_buffer(
 *     "<root><item>test</item></root>",
 *     buffer,
 *     sizeof(buffer),
 *     NULL,
 *     &err
 * );
 *
 * if (err != ABX_OK) {
 *     fprintf(stderr, "Conversion failed: %s\n", abx_get_last_error());
 * } else {
 *     printf("Generated %zu bytes of ABX data\n", size);
 * }
 * @endcode
 */

#ifndef LIBABX_C_API_H
#define LIBABX_C_API_H

/**
 * 
 * COMPATIBILITY:
 * =====================
 * - ABX → XML (Deserialization): compatible with Android system files.
 * 
 * - XML → ABX (Serialization): May produce different output than Android's official
 *   BinaryXmlSerializer. This library uses automatic type inference (guessing whether
 *   a value is int, float, boolean, etc.), while Android uses explicit type methods.
 *   
 *   This means:
 *   * The output is valid ABX format
 *   * Round-tripping (XML → ABX → XML) works correctly  
 *   * But binary output may differ from what Android generates
 *   * Android system components may reject files if they expect specific types
 * 
 *   Recommendation: For generating ABX files that Android will consume, use the
 *   low-level serializer API (abx_serializer_*) with explicit type methods.
 * 
 * THREAD SAFETY:
 * ==============
 * - Serializer and deserializer handles are NOT thread-safe
 * - Each thread should create its own instances
 * - Error messages are stored in thread-local storage
 * 
 * USAGE:
 * ======
 * High-level API (simple conversions):
 *   abx_convert_xml_file_to_abx_file() / abx_convert_abx_file_to_xml_file()
 * 
 * Low-level API (manual construction with explicit types):
 *   abx_serializer_create_*() → abx_serializer_start_tag() → 
 *   abx_serializer_attribute_int() → abx_serializer_end_tag()
 */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup OpaqueTypes Opaque Types
 * @{
 */

/**
 * @typedef abx_serializer_t
 * @brief Opaque handle to an XML serializer.
 *
 * Created with abx_serializer_create_file() or abx_serializer_create_buffer().
 * Must be freed with abx_serializer_free().
 */
typedef struct abx_serializer abx_serializer_t;

/**
 * @typedef abx_deserializer_t
 * @brief Opaque handle to an ABX deserializer.
 *
 * Created with abx_deserializer_create_file() or abx_deserializer_create_buffer().
 * Must be freed with abx_deserializer_free().
 */
typedef struct abx_deserializer abx_deserializer_t;

/** @} */

/**
 * @defgroup ErrorHandling Error Codes and Handling
 * @{
 */

/**
 * @enum abx_error_t
 * @brief Error codes returned by libabx C functions.
 *
 * All functions that return abx_error_t indicate success with ABX_OK and
 * error conditions with negative values.
 */
typedef enum {
    ABX_OK = 0,                          ///< Operation completed successfully
    ABX_ERROR_NULL_POINTER = -1,         ///< NULL pointer passed as argument
    ABX_ERROR_INVALID_HANDLE = -2,       ///< Invalid serializer or deserializer handle
    ABX_ERROR_FILE_NOT_FOUND = -3,       ///< File does not exist or cannot be opened
    ABX_ERROR_PARSE_FAILED = -4,         ///< XML or ABX parsing failed
    ABX_ERROR_WRITE_FAILED = -5,         ///< Failed to write to output file or buffer
    ABX_ERROR_INVALID_FORMAT = -6,       ///< Invalid ABX format or corrupted data
    ABX_ERROR_BUFFER_TOO_SMALL = -7,     ///< Output buffer too small for result
    ABX_ERROR_TAG_MISMATCH = -8,         ///< Tag nesting mismatch (endTag without matching startTag)
    ABX_ERROR_OUT_OF_MEMORY = -9,        ///< Memory allocation failed
    ABX_ERROR_UNKNOWN = -100             ///< Unknown error (check abx_get_last_error() for details)
} abx_error_t;

/**
 * @brief Get the last error message.
 *
 * Returns a human-readable description of the most recent error. The error
 * message is stored in thread-local storage, so different threads maintain
 * separate error states.
 *
 * @return Pointer to error message string, or NULL if no error has occurred
 * @note The returned pointer is valid until the next libabx call on this thread
 *
 * @example
 * @code
 * abx_error_t err = abx_convert_xml_file_to_abx_file("input.xml", "output.abx", NULL);
 * if (err != ABX_OK) {
 *     printf("Error: %s\n", abx_get_last_error());
 * }
 * @endcode
 */
const char* abx_get_last_error(void);

/** @} */

/**
 * @defgroup ConversionOptions Conversion Options
 * @{
 */

/**
 * @struct abx_options_t
 * @brief Options for controlling XML-to-ABX conversion behavior.
 *
 * Pass this structure to conversion functions to customize how XML is processed.
 * NULL is equivalent to default options.
 */
typedef struct {
    /**
     * @brief If non-zero, collapse insignificant whitespace.
     *
     * When set to 1 (true), whitespace-only text nodes between tags are ignored.
     * Whitespace within actual text content is always preserved.
     *
     * Default: 0 (false, preserve all whitespace)
     */
    int collapse_whitespaces;
} abx_options_t;

/**
 * @typedef abx_warning_callback_t
 * @brief Callback function for warnings during XML parsing.
 *
 * Called during conversion to report non-fatal issues such as namespace usage
 * or unsupported features.
 *
 * @param category Category of warning (e.g., "Namespaces and prefixes")
 * @param message Descriptive message about the warning
 * @param user_data User-provided context pointer (reserved for future use, currently NULL)
 *
 * @example
 * @code
 * void my_warning_handler(const char* category, const char* message, void* user_data) {
 *     fprintf(stderr, "Warning [%s]: %s\n", category, message);
 * }
 * @endcode
 */
typedef void (*abx_warning_callback_t)(const char* category, const char* message, void* user_data);

/** @} */

/**
 * @defgroup SerializerAPI Serializer API (XML to ABX)
 * @brief Functions for creating and using serializers to convert XML to ABX.
 * @{
 */

/**
 * @brief Create a new serializer that writes to a file.
 *
 * Creates a serializer that writes binary ABX data to the specified file path.
 * The file is opened in binary write mode and will be truncated if it exists.
 *
 * @param filepath Path to the output file to create
 * @param error Optional pointer to receive error code. If provided, will be set to
 *              ABX_OK on success or an error code on failure
 * @return Serializer handle on success, NULL on failure
 * @note Must be freed with abx_serializer_free()
 *
 * @example
 * @code
 * abx_error_t err;
 * abx_serializer_t* ser = abx_serializer_create_file("output.abx", &err);
 * if (!ser) {
 *     fprintf(stderr, "Failed: %s\n", abx_get_last_error());
 *     return;
 * }
 * // ... use serializer ...
 * abx_serializer_free(ser);
 * @endcode
 */
abx_serializer_t* abx_serializer_create_file(const char* filepath, abx_error_t* error);

/**
 * @brief Create a new serializer that writes to an in-memory buffer.
 *
 * Creates a serializer that writes binary ABX data to an internal memory buffer.
 * Use abx_serializer_get_buffer() to retrieve the generated data.
 *
 * @param error Optional pointer to receive error code
 * @return Serializer handle on success, NULL on failure
 * @note Must be freed with abx_serializer_free()
 *
 * @example
 * @code
 * abx_serializer_t* ser = abx_serializer_create_buffer(NULL);
 * // ... build document ...
 * abx_serializer_end_document(ser);
 * uint8_t buffer[4096];
 * size_t size = abx_serializer_get_buffer(ser, buffer, sizeof(buffer));
 * abx_serializer_free(ser);
 * @endcode
 */
abx_serializer_t* abx_serializer_create_buffer(abx_error_t* error);

/**
 * @brief Start an XML document.
 *
 * Must be called once at the beginning of serialization. Writes the ABX format
 * magic header and document start marker.
 *
 * @param serializer Serializer handle
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_start_document(abx_serializer_t* serializer);

/**
 * @brief End an XML document and flush output.
 *
 * Must be called once after all content is written. Writes the document end
 * marker and flushes the output stream.
 *
 * @param serializer Serializer handle
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_end_document(abx_serializer_t* serializer);

/**
 * @brief Begin an XML element.
 *
 * Opens a new element with the specified tag name. Attributes should be added
 * before starting child content or ending the tag.
 *
 * @param serializer Serializer handle
 * @param name Element tag name
 * @return ABX_OK on success, error code on failure
 *
 * @example
 * @code
 * abx_serializer_start_tag(ser, "root");
 * abx_serializer_start_tag(ser, "child");
 * abx_serializer_end_tag(ser, "child");
 * abx_serializer_end_tag(ser, "root");
 * @endcode
 */
abx_error_t abx_serializer_start_tag(abx_serializer_t* serializer, const char* name);

/**
 * @brief End an XML element.
 *
 * Closes the element with the specified tag name. The tag name must match the
 * most recent unmatched startTag call.
 *
 * @param serializer Serializer handle
 * @param name Element tag name (must match corresponding startTag)
 * @return ABX_OK on success, ABX_ERROR_TAG_MISMATCH if tag doesn't match
 */
abx_error_t abx_serializer_end_tag(abx_serializer_t* serializer, const char* name);

/**
 * @brief Add an attribute with string value.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param value Attribute value as string
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_string(abx_serializer_t* serializer, const char* name, const char* value);

/**
 * @brief Add an attribute with 32-bit signed integer value.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param value Integer value
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_int(abx_serializer_t* serializer, const char* name, int32_t value);

/**
 * @brief Add an attribute with 32-bit integer value displayed as hexadecimal.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param value Integer value
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_int_hex(abx_serializer_t* serializer, const char* name, int32_t value);

/**
 * @brief Add an attribute with 64-bit signed integer value.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param value Long integer value
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_long(abx_serializer_t* serializer, const char* name, int64_t value);

/**
 * @brief Add an attribute with 64-bit integer value displayed as hexadecimal.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param value Long integer value
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_long_hex(abx_serializer_t* serializer, const char* name, int64_t value);

/**
 * @brief Add an attribute with 32-bit floating point value.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param value Float value
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_float(abx_serializer_t* serializer, const char* name, float value);

/**
 * @brief Add an attribute with 64-bit floating point (double) value.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param value Double value
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_double(abx_serializer_t* serializer, const char* name, double value);

/**
 * @brief Add an attribute with boolean value.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param value Boolean value (0 = false, non-zero = true)
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_bool(abx_serializer_t* serializer, const char* name, int value);

/**
 * @brief Add an attribute containing binary data encoded as hexadecimal.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param data Pointer to binary data
 * @param length Number of bytes to encode
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_bytes_hex(abx_serializer_t* serializer, const char* name, const uint8_t* data, size_t length);

/**
 * @brief Add an attribute containing binary data encoded as base64.
 *
 * @param serializer Serializer handle
 * @param name Attribute name
 * @param data Pointer to binary data
 * @param length Number of bytes to encode
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_attribute_bytes_base64(abx_serializer_t* serializer, const char* name, const uint8_t* data, size_t length);

/**
 * @brief Add text content to the current element.
 *
 * @param serializer Serializer handle
 * @param text Text content (will be XML-entity-encoded automatically)
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_text(abx_serializer_t* serializer, const char* text);

/**
 * @brief Add a CDATA section.
 *
 * CDATA sections allow embedding special characters without XML entity encoding.
 *
 * @param serializer Serializer handle
 * @param text CDATA content
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_cdata(abx_serializer_t* serializer, const char* text);

/**
 * @brief Add an XML comment.
 *
 * @param serializer Serializer handle
 * @param text Comment content
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_serializer_comment(abx_serializer_t* serializer, const char* text);

/**
 * @brief Retrieve the buffer contents from a buffer-based serializer.
 *
 * This function returns the size of the generated binary data and optionally
 * copies it to the provided buffer.
 *
 * @param serializer Serializer handle (must be created with abx_serializer_create_buffer)
 * @param out_buffer Optional pointer to buffer where data will be copied.
 *                   If NULL, only the size is returned
 * @param buffer_size Size of out_buffer in bytes (ignored if out_buffer is NULL)
 * @return Number of bytes in the binary data (not including null terminator).
 *         If out_buffer was provided and is too small, the data is not copied.
 *
 * @example
 * @code
 * // First call: get the size
 * size_t size = abx_serializer_get_buffer(ser, NULL, 0);
 * uint8_t* buffer = malloc(size);
 *
 * // Second call: get the data
 * abx_serializer_get_buffer(ser, buffer, size);
 *
 * // Use buffer...
 * free(buffer);
 * @endcode
 */
size_t abx_serializer_get_buffer(abx_serializer_t* serializer, uint8_t* out_buffer, size_t buffer_size);

/**
 * @brief Free serializer resources.
 *
 * Releases all memory associated with the serializer. The handle is invalid
 * after this call.
 *
 * @param serializer Serializer handle
 */
void abx_serializer_free(abx_serializer_t* serializer);

/** @} */

/**
 * @defgroup DeserializerAPI Deserializer API (ABX to XML)
 * @brief Functions for creating and using deserializers to convert ABX to XML.
 * @{
 */

/**
 * @brief Create a new deserializer from a file.
 *
 * Opens an ABX binary file for deserialization to XML format.
 *
 * @param filepath Path to the ABX file to read
 * @param error Optional pointer to receive error code
 * @return Deserializer handle on success, NULL on failure
 * @note Must be freed with abx_deserializer_free()
 */
abx_deserializer_t* abx_deserializer_create_file(const char* filepath, abx_error_t* error);

/**
 * @brief Create a new deserializer from a memory buffer.
 *
 * Creates a deserializer that reads ABX binary data from memory.
 *
 * @param data Pointer to binary ABX data
 * @param length Size of the binary data in bytes
 * @param error Optional pointer to receive error code
 * @return Deserializer handle on success, NULL on failure
 * @note Must be freed with abx_deserializer_free()
 */
abx_deserializer_t* abx_deserializer_create_buffer(const uint8_t* data, size_t length, abx_error_t* error);

/**
 * @brief Reset deserializer to the beginning.
 *
 * Allows re-reading the same ABX data. For file-based deserializers, reopens
 * the file. For buffer-based deserializers, resets position to start.
 *
 * @param deserializer Deserializer handle
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_deserializer_reset(abx_deserializer_t* deserializer);

/**
 * @brief Deserialize ABX data to an XML file.
 *
 * Reads ABX binary data from the deserializer and writes XML to the
 * specified file path.
 *
 * @param deserializer Deserializer handle
 * @param output_path Path where the XML file will be written
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_deserializer_to_file(abx_deserializer_t* deserializer, const char* output_path);

/**
 * @brief Deserialize ABX data to an XML string.
 *
 * Reads ABX binary data from the deserializer and returns the XML as a string.
 * This function returns the required buffer size and optionally copies the data.
 *
 * @param deserializer Deserializer handle
 * @param out_buffer Optional pointer to buffer where XML string will be copied.
 *                   If NULL, only the size is returned
 * @param buffer_size Size of out_buffer in bytes
 * @return Required size including null terminator. If out_buffer was provided and
 *         is too small, the data is not copied.
 *
 * @example
 * @code
 * // First call: get the size
 * size_t size = abx_deserializer_to_string(deser, NULL, 0);
 * char* xml = malloc(size);
 *
 * // Second call: get the data
 * abx_deserializer_to_string(deser, xml, size);
 *
 * printf("%s\n", xml);
 * free(xml);
 * @endcode
 */
size_t abx_deserializer_to_string(abx_deserializer_t* deserializer, char* out_buffer, size_t buffer_size);

/**
 * @brief Free deserializer resources.
 *
 * Releases all memory associated with the deserializer. The handle is invalid
 * after this call.
 *
 * @param deserializer Deserializer handle
 */
void abx_deserializer_free(abx_deserializer_t* deserializer);

/** @} */

/**
 * @defgroup HighLevelAPI High-Level Convenience Functions
 * @brief Simple one-call functions for common conversion tasks.
 * @{
 */

/**
 * @brief Convert an XML file directly to an ABX file.
 *
 * Loads XML from a file, converts to ABX format, and writes to output file.
 * This is the simplest way to convert files.
 *
 * @param xml_path Path to the XML file to convert
 * @param abx_path Path where the ABX file will be written
 * @param options Optional conversion options (NULL for defaults)
 * @return ABX_OK on success, error code on failure
 *
 * @example
 * @code
 * abx_error_t err = abx_convert_xml_file_to_abx_file("config.xml", "config.abx", NULL);
 * if (err != ABX_OK) {
 *     fprintf(stderr, "Conversion failed: %s\n", abx_get_last_error());
 * }
 * @endcode
 */
abx_error_t abx_convert_xml_file_to_abx_file(const char* xml_path, const char* abx_path, const abx_options_t* options);

/**
 * @brief Convert an XML string directly to an ABX file.
 *
 * Parses XML from a string, converts to ABX format, and writes to output file.
 *
 * @param xml_string XML content as a string
 * @param abx_path Path where the ABX file will be written
 * @param options Optional conversion options (NULL for defaults)
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_convert_xml_string_to_abx_file(const char* xml_string, const char* abx_path, const abx_options_t* options);

/**
 * @brief Convert an XML file to ABX binary data in a buffer.
 *
 * Loads XML from a file, converts to ABX format, and writes to memory buffer.
 * Returns the required size and optionally copies the data.
 *
 * @param xml_path Path to the XML file to convert
 * @param out_buffer Optional pointer to buffer where ABX data will be copied
 * @param buffer_size Size of out_buffer in bytes
 * @param options Optional conversion options
 * @param error Optional pointer to receive error code
 * @return Number of bytes in the ABX data. If out_buffer was provided and
 *         is too small, the data is not copied.
 */
size_t abx_convert_xml_file_to_buffer(const char* xml_path, uint8_t* out_buffer, size_t buffer_size, const abx_options_t* options, abx_error_t* error);

/**
 * @brief Convert an XML string to ABX binary data in a buffer.
 *
 * Parses XML from a string, converts to ABX format, and writes to memory buffer.
 *
 * @param xml_string XML content as a string
 * @param out_buffer Optional pointer to buffer where ABX data will be copied
 * @param buffer_size Size of out_buffer in bytes
 * @param options Optional conversion options
 * @param error Optional pointer to receive error code
 * @return Number of bytes in the ABX data
 */
size_t abx_convert_xml_string_to_buffer(const char* xml_string, uint8_t* out_buffer, size_t buffer_size, const abx_options_t* options, abx_error_t* error);

/**
 * @brief Convert an ABX file directly to an XML file.
 *
 * Loads ABX binary data from a file and writes reconstructed XML to output file.
 *
 * @param abx_path Path to the ABX file to convert
 * @param xml_path Path where the XML file will be written
 * @return ABX_OK on success, error code on failure
 *
 * @example
 * @code
 * abx_error_t err = abx_convert_abx_file_to_xml_file("data.abx", "data.xml");
 * if (err != ABX_OK) {
 *     fprintf(stderr, "Conversion failed: %s\n", abx_get_last_error());
 * }
 * @endcode
 */
abx_error_t abx_convert_abx_file_to_xml_file(const char* abx_path, const char* xml_path);

/**
 * @brief Convert ABX binary data from a buffer to an XML file.
 *
 * Parses ABX data from memory and writes reconstructed XML to file.
 *
 * @param abx_data Pointer to binary ABX data
 * @param length Size of the ABX data in bytes
 * @param xml_path Path where the XML file will be written
 * @return ABX_OK on success, error code on failure
 */
abx_error_t abx_convert_abx_buffer_to_xml_file(const uint8_t* abx_data, size_t length, const char* xml_path);

/**
 * @brief Convert an ABX file to an XML string in a buffer.
 *
 * Loads ABX from a file and converts to XML string. Returns required size
 * and optionally copies the data.
 *
 * @param abx_path Path to the ABX file to convert
 * @param out_buffer Optional pointer to buffer where XML string will be copied
 * @param buffer_size Size of out_buffer in bytes
 * @param error Optional pointer to receive error code
 * @return Required size including null terminator
 */
size_t abx_convert_abx_file_to_string(const char* abx_path, char* out_buffer, size_t buffer_size, abx_error_t* error);

/**
 * @brief Convert ABX binary data from a buffer to an XML string.
 *
 * Parses ABX data from memory and returns XML as string. Returns required
 * size and optionally copies the data.
 *
 * @param abx_data Pointer to binary ABX data
 * @param length Size of the ABX data in bytes
 * @param out_buffer Optional pointer to buffer where XML string will be copied
 * @param buffer_size Size of out_buffer in bytes
 * @param error Optional pointer to receive error code
 * @return Required size including null terminator
 */
size_t abx_convert_abx_buffer_to_string(const uint8_t* abx_data, size_t length, char* out_buffer, size_t buffer_size, abx_error_t* error);

/** @} */

/**
 * @defgroup UtilityFunctions Utility Functions
 * @brief Standalone encoding/decoding utilities.
 * @{
 */

/**
 * @brief Encode binary data as base64.
 *
 * Converts raw binary data into a base64-encoded string suitable for
 * embedding in XML or other text formats.
 *
 * @param data Pointer to binary data to encode
 * @param length Number of bytes to encode
 * @param out Optional pointer to buffer where encoded string will be copied
 * @param out_size Size of out buffer in bytes
 * @return Required size including null terminator. If out was provided and
 *         is too small, the data is not copied.
 */
size_t abx_base64_encode(const uint8_t* data, size_t length, char* out, size_t out_size);

/**
 * @brief Decode a base64-encoded string into binary data.
 *
 * Converts a base64 string back into its original binary form. Padding
 * characters ('=') are handled automatically.
 *
 * @param encoded Base64-encoded string
 * @param out Optional pointer to buffer where decoded data will be copied
 * @param out_size Size of out buffer in bytes
 * @return Number of bytes in the decoded data. If out was provided and
 *         is too small, the data is not copied.
 */
size_t abx_base64_decode(const char* encoded, uint8_t* out, size_t out_size);

/**
 * @brief Encode binary data as hexadecimal.
 *
 * Converts raw binary data into a hexadecimal string (e.g., "DEADBEEF").
 *
 * @param data Pointer to binary data to encode
 * @param length Number of bytes to encode
 * @param out Optional pointer to buffer where hex string will be copied
 * @param out_size Size of out buffer in bytes
 * @return Required size including null terminator. If out was provided and
 *         is too small, the data is not copied.
 */
size_t abx_hex_encode(const uint8_t* data, size_t length, char* out, size_t out_size);

/**
 * @brief Decode a hexadecimal string into binary data.
 *
 * Converts a hex string (case-insensitive) into its binary representation.
 * The input must have even length (two hex characters per byte).
 *
 * @param hex Hexadecimal string to decode
 * @param out Optional pointer to buffer where decoded data will be copied
 * @param out_size Size of out buffer in bytes
 * @return Number of bytes in the decoded data. If out was provided and
 *         is too small, the data is not copied.
 */
size_t abx_hex_decode(const char* hex, uint8_t* out, size_t out_size);

/** @} */

#ifdef __cplusplus
}
#endif

#endif // LIBABX_C_API_H