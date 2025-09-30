#ifndef LIBABX_C_API_H
#define LIBABX_C_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct abx_serializer abx_serializer_t;
typedef struct abx_deserializer abx_deserializer_t;

// error codes
typedef enum {
    ABX_OK = 0,
    ABX_ERROR_NULL_POINTER = -1,
    ABX_ERROR_INVALID_HANDLE = -2,
    ABX_ERROR_FILE_NOT_FOUND = -3,
    ABX_ERROR_PARSE_FAILED = -4,
    ABX_ERROR_WRITE_FAILED = -5,
    ABX_ERROR_INVALID_FORMAT = -6,
    ABX_ERROR_BUFFER_TOO_SMALL = -7,
    ABX_ERROR_TAG_MISMATCH = -8,
    ABX_ERROR_OUT_OF_MEMORY = -9,
    ABX_ERROR_UNKNOWN = -100
} abx_error_t;

// options for XML to ABX conversion
typedef struct {
    int collapse_whitespaces;  // 0 = false, 1 = true
} abx_options_t;

// callback for warnings during conversion
typedef void (*abx_warning_callback_t)(const char* category, const char* message, void* user_data);

// ============================================================================
// ERROR HANDLING
// ============================================================================

// get the last error message 
const char* abx_get_last_error(void);

// ============================================================================
// SERIALIZER API (XML to ABX)
// ============================================================================

// Create a new serializer that writes to a file
abx_serializer_t* abx_serializer_create_file(const char* filepath, abx_error_t* error);

// Create a new serializer that writes to a memory buffer
// The buffer will be automatically managed and can be retrieved with abx_serializer_get_buffer
abx_serializer_t* abx_serializer_create_buffer(abx_error_t* error);

// Start the XML document
abx_error_t abx_serializer_start_document(abx_serializer_t* serializer);

// End the XML document and flush
abx_error_t abx_serializer_end_document(abx_serializer_t* serializer);

// Start an XML tag
abx_error_t abx_serializer_start_tag(abx_serializer_t* serializer, const char* name);

// End an XML tag
abx_error_t abx_serializer_end_tag(abx_serializer_t* serializer, const char* name);

// Add attributes (various types)
abx_error_t abx_serializer_attribute_string(abx_serializer_t* serializer, const char* name, const char* value);
abx_error_t abx_serializer_attribute_int(abx_serializer_t* serializer, const char* name, int32_t value);
abx_error_t abx_serializer_attribute_int_hex(abx_serializer_t* serializer, const char* name, int32_t value);
abx_error_t abx_serializer_attribute_long(abx_serializer_t* serializer, const char* name, int64_t value);
abx_error_t abx_serializer_attribute_long_hex(abx_serializer_t* serializer, const char* name, int64_t value);
abx_error_t abx_serializer_attribute_float(abx_serializer_t* serializer, const char* name, float value);
abx_error_t abx_serializer_attribute_double(abx_serializer_t* serializer, const char* name, double value);
abx_error_t abx_serializer_attribute_bool(abx_serializer_t* serializer, const char* name, int value);
abx_error_t abx_serializer_attribute_bytes_hex(abx_serializer_t* serializer, const char* name, const uint8_t* data, size_t length);
abx_error_t abx_serializer_attribute_bytes_base64(abx_serializer_t* serializer, const char* name, const uint8_t* data, size_t length);

// Add text content
abx_error_t abx_serializer_text(abx_serializer_t* serializer, const char* text);

// Add CDATA section
abx_error_t abx_serializer_cdata(abx_serializer_t* serializer, const char* text);

// Add comment
abx_error_t abx_serializer_comment(abx_serializer_t* serializer, const char* text);

// Get buffer contents (only for buffer-based serializers)
// Returns the size of the buffer, and copies data to 'out_buffer' if provided
// If out_buffer is NULL, just returns the required size
size_t abx_serializer_get_buffer(abx_serializer_t* serializer, uint8_t* out_buffer, size_t buffer_size);

// Free serializer resources
void abx_serializer_free(abx_serializer_t* serializer);

// ============================================================================
// DESERIALIZER API (ABX to XML)
// ============================================================================

// Create a new deserializer from a file
abx_deserializer_t* abx_deserializer_create_file(const char* filepath, abx_error_t* error);

// Create a new deserializer from a memory buffer
abx_deserializer_t* abx_deserializer_create_buffer(const uint8_t* data, size_t length, abx_error_t* error);

// Deserialize to XML file
abx_error_t abx_deserializer_to_file(abx_deserializer_t* deserializer, const char* output_path);

// deserialize to XML string
// returns the size of the XML string, and copies it to 'out_buffer' if provided
// if out_buffer is NULL, just returns the required size (including null terminator)
size_t abx_deserializer_to_string(abx_deserializer_t* deserializer, char* out_buffer, size_t buffer_size);

// Free deserializer resources
void abx_deserializer_free(abx_deserializer_t* deserializer);

// ============================================================================
// HIGH-LEVEL CONVENIENCE FUNCTIONS
// ============================================================================

// Convert XML file directly to ABX file
abx_error_t abx_convert_xml_file_to_abx_file(const char* xml_path, const char* abx_path, const abx_options_t* options);

// Convert XML string directly to ABX file
abx_error_t abx_convert_xml_string_to_abx_file(const char* xml_string, const char* abx_path, const abx_options_t* options);

// Convert XML file to ABX buffer
// Returns the size of the buffer, and copies data to 'out_buffer' if provided
size_t abx_convert_xml_file_to_buffer(const char* xml_path, uint8_t* out_buffer, size_t buffer_size, const abx_options_t* options, abx_error_t* error);

// Convert XML string to ABX buffer
size_t abx_convert_xml_string_to_buffer(const char* xml_string, uint8_t* out_buffer, size_t buffer_size, const abx_options_t* options, abx_error_t* error);

// Convert ABX file directly to XML file
abx_error_t abx_convert_abx_file_to_xml_file(const char* abx_path, const char* xml_path);

// Convert ABX buffer to XML file
abx_error_t abx_convert_abx_buffer_to_xml_file(const uint8_t* abx_data, size_t length, const char* xml_path);

// Convert ABX file to XML string
size_t abx_convert_abx_file_to_string(const char* abx_path, char* out_buffer, size_t buffer_size, abx_error_t* error);

// Convert ABX buffer to XML string
size_t abx_convert_abx_buffer_to_string(const uint8_t* abx_data, size_t length, char* out_buffer, size_t buffer_size, abx_error_t* error);

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

// Base64 encode
// Returns the size of the encoded string, and copies it to 'out' if provided
size_t abx_base64_encode(const uint8_t* data, size_t length, char* out, size_t out_size);

// Base64 decode
// Returns the size of the decoded data, and copies it to 'out' if provided
size_t abx_base64_decode(const char* encoded, uint8_t* out, size_t out_size);

// Hex encode
size_t abx_hex_encode(const uint8_t* data, size_t length, char* out, size_t out_size);

// Hex decode
size_t abx_hex_decode(const char* hex, uint8_t* out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif // LIBABX_C_API_H
