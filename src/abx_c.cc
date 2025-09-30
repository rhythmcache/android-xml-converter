#include "abx.h"
#include "abx.hpp"

#include <cstring>
#include <fstream>
#include <sstream>
#include <memory>
#include <vector>


thread_local std::string g_last_error;


static abx_error_t set_error(abx_error_t code, const std::string& msg) {
    g_last_error = msg;
    return code;
}


static void clear_error() {
    g_last_error.clear();
}


static abx_error_t handle_exception(const std::exception& e) {
    g_last_error = e.what();
    return ABX_ERROR_UNKNOWN;
}

// ============================================================================
// INTERNAL STRUCTURES
// ============================================================================

struct abx_serializer {
    std::unique_ptr<std::ostream> stream;
    std::unique_ptr<std::ostringstream> buffer_stream;
    std::unique_ptr<libabx::BinaryXmlSerializer> serializer;
    bool document_started = false;
};

struct abx_deserializer {
    std::unique_ptr<std::istream> stream;
    std::unique_ptr<std::istringstream> buffer_stream;
    std::vector<char> data;
};

// ============================================================================
// ERROR HANDLING
// ============================================================================

extern "C" const char* abx_get_last_error(void) {
    return g_last_error.empty() ? nullptr : g_last_error.c_str();
}

// ============================================================================
// SERIALIZER IMPLEMENTATION
// ============================================================================

extern "C" abx_serializer_t* abx_serializer_create_file(const char* filepath, abx_error_t* error) {
    if (!filepath) {
        if (error) *error = set_error(ABX_ERROR_NULL_POINTER, "filepath is null");
        return nullptr;
    }

    try {
        clear_error();
        auto serializer = new abx_serializer;
        serializer->stream = std::make_unique<std::ofstream>(filepath, std::ios::binary);
        
        if (!serializer->stream->good()) {
            delete serializer;
            if (error) *error = set_error(ABX_ERROR_FILE_NOT_FOUND, "Failed to open file for writing");
            return nullptr;
        }

        serializer->serializer = std::make_unique<libabx::BinaryXmlSerializer>(*serializer->stream);
        
        if (error) *error = ABX_OK;
        return serializer;
    } catch (const std::exception& e) {
        if (error) *error = handle_exception(e);
        return nullptr;
    }
}

extern "C" abx_serializer_t* abx_serializer_create_buffer(abx_error_t* error) {
    try {
        clear_error();
        auto serializer = new abx_serializer;
        serializer->buffer_stream = std::make_unique<std::ostringstream>(std::ios::binary);
        serializer->stream = std::unique_ptr<std::ostream>(serializer->buffer_stream.get());
        serializer->serializer = std::make_unique<libabx::BinaryXmlSerializer>(*serializer->buffer_stream);
        
        if (error) *error = ABX_OK;
        return serializer;
    } catch (const std::exception& e) {
        if (error) *error = handle_exception(e);
        return nullptr;
    }
}

extern "C" abx_error_t abx_serializer_start_document(abx_serializer_t* serializer) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }

    try {
        clear_error();
        serializer->serializer->startDocument();
        serializer->document_started = true;
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_end_document(abx_serializer_t* serializer) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }

    try {
        clear_error();
        serializer->serializer->endDocument();
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_start_tag(abx_serializer_t* serializer, const char* name) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name) {
        return set_error(ABX_ERROR_NULL_POINTER, "Tag name is null");
    }

    try {
        clear_error();
        serializer->serializer->startTag(name);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_end_tag(abx_serializer_t* serializer, const char* name) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name) {
        return set_error(ABX_ERROR_NULL_POINTER, "Tag name is null");
    }

    try {
        clear_error();
        serializer->serializer->endTag(name);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_string(abx_serializer_t* serializer, const char* name, const char* value) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name || !value) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name or value is null");
    }

    try {
        clear_error();
        serializer->serializer->attribute(name, value);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_int(abx_serializer_t* serializer, const char* name, int32_t value) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name is null");
    }

    try {
        clear_error();
        serializer->serializer->attributeInt(name, value);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_int_hex(abx_serializer_t* serializer, const char* name, int32_t value) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name is null");
    }

    try {
        clear_error();
        serializer->serializer->attributeIntHex(name, value);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_long(abx_serializer_t* serializer, const char* name, int64_t value) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name is null");
    }

    try {
        clear_error();
        serializer->serializer->attributeLong(name, value);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_long_hex(abx_serializer_t* serializer, const char* name, int64_t value) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name is null");
    }

    try {
        clear_error();
        serializer->serializer->attributeLongHex(name, value);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_float(abx_serializer_t* serializer, const char* name, float value) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name is null");
    }

    try {
        clear_error();
        serializer->serializer->attributeFloat(name, value);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_double(abx_serializer_t* serializer, const char* name, double value) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name is null");
    }

    try {
        clear_error();
        serializer->serializer->attributeDouble(name, value);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_bool(abx_serializer_t* serializer, const char* name, int value) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name is null");
    }

    try {
        clear_error();
        serializer->serializer->attributeBoolean(name, value != 0);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_bytes_hex(abx_serializer_t* serializer, const char* name, const uint8_t* data, size_t length) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name || !data) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name or data is null");
    }

    try {
        clear_error();
        std::vector<uint8_t> vec(data, data + length);
        serializer->serializer->attributeBytesHex(name, vec);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_attribute_bytes_base64(abx_serializer_t* serializer, const char* name, const uint8_t* data, size_t length) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!name || !data) {
        return set_error(ABX_ERROR_NULL_POINTER, "Attribute name or data is null");
    }

    try {
        clear_error();
        std::vector<uint8_t> vec(data, data + length);
        serializer->serializer->attributeBytesBase64(name, vec);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_text(abx_serializer_t* serializer, const char* text) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!text) {
        return set_error(ABX_ERROR_NULL_POINTER, "Text is null");
    }

    try {
        clear_error();
        serializer->serializer->text(text);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_cdata(abx_serializer_t* serializer, const char* text) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!text) {
        return set_error(ABX_ERROR_NULL_POINTER, "Text is null");
    }

    try {
        clear_error();
        serializer->serializer->cdsect(text);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_serializer_comment(abx_serializer_t* serializer, const char* text) {
    if (!serializer || !serializer->serializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle");
    }
    if (!text) {
        return set_error(ABX_ERROR_NULL_POINTER, "Text is null");
    }

    try {
        clear_error();
        serializer->serializer->comment(text);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" size_t abx_serializer_get_buffer(abx_serializer_t* serializer, uint8_t* out_buffer, size_t buffer_size) {
    if (!serializer || !serializer->buffer_stream) {
        set_error(ABX_ERROR_INVALID_HANDLE, "Invalid serializer handle or not a buffer serializer");
        return 0;
    }

    try {
        clear_error();
        std::string str = serializer->buffer_stream->str();
        size_t size = str.size();

        if (out_buffer && buffer_size >= size) {
            std::memcpy(out_buffer, str.data(), size);
        }

        return size;
    } catch (const std::exception& e) {
        handle_exception(e);
        return 0;
    }
}

extern "C" void abx_serializer_free(abx_serializer_t* serializer) {
    delete serializer;
}

// ============================================================================
// DESERIALIZER IMPLEMENTATION
// ============================================================================

extern "C" abx_deserializer_t* abx_deserializer_create_file(const char* filepath, abx_error_t* error) {
    if (!filepath) {
        if (error) *error = set_error(ABX_ERROR_NULL_POINTER, "filepath is null");
        return nullptr;
    }

    try {
        clear_error();
        auto deserializer = new abx_deserializer;
        
        // Read file into memory
        std::ifstream file(filepath, std::ios::binary);
        if (!file) {
            delete deserializer;
            if (error) *error = set_error(ABX_ERROR_FILE_NOT_FOUND, "Failed to open file for reading");
            return nullptr;
        }

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        deserializer->data.resize(size);
        file.read(deserializer->data.data(), size);

        if (error) *error = ABX_OK;
        return deserializer;
    } catch (const std::exception& e) {
        if (error) *error = handle_exception(e);
        return nullptr;
    }
}

extern "C" abx_deserializer_t* abx_deserializer_create_buffer(const uint8_t* data, size_t length, abx_error_t* error) {
    if (!data) {
        if (error) *error = set_error(ABX_ERROR_NULL_POINTER, "data is null");
        return nullptr;
    }

    try {
        clear_error();
        auto deserializer = new abx_deserializer;
        deserializer->data.assign(reinterpret_cast<const char*>(data), reinterpret_cast<const char*>(data) + length);
        
        if (error) *error = ABX_OK;
        return deserializer;
    } catch (const std::exception& e) {
        if (error) *error = handle_exception(e);
        return nullptr;
    }
}

extern "C" abx_error_t abx_deserializer_to_file(abx_deserializer_t* deserializer, const char* output_path) {
    if (!deserializer) {
        return set_error(ABX_ERROR_INVALID_HANDLE, "Invalid deserializer handle");
    }
    if (!output_path) {
        return set_error(ABX_ERROR_NULL_POINTER, "output_path is null");
    }

    try {
        clear_error();
        std::ofstream out(output_path);
        if (!out) {
            return set_error(ABX_ERROR_WRITE_FAILED, "Failed to open output file");
        }

        libabx::BinaryXmlDeserializer deser(deserializer->data, out);
        deser.deserialize();

        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" size_t abx_deserializer_to_string(abx_deserializer_t* deserializer, char* out_buffer, size_t buffer_size) {
    if (!deserializer) {
        set_error(ABX_ERROR_INVALID_HANDLE, "Invalid deserializer handle");
        return 0;
    }

    try {
        clear_error();
        std::ostringstream out;
        libabx::BinaryXmlDeserializer deser(deserializer->data, out);
        deser.deserialize();

        std::string result = out.str();
        size_t needed = result.size() + 1; // +1 for null terminator

        if (out_buffer && buffer_size >= needed) {
            std::memcpy(out_buffer, result.c_str(), needed);
        }

        return needed;
    } catch (const std::exception& e) {
        handle_exception(e);
        return 0;
    }
}

extern "C" void abx_deserializer_free(abx_deserializer_t* deserializer) {
    delete deserializer;
}

// ============================================================================
// HIGH-LEVEL CONVENIENCE FUNCTIONS
// ============================================================================

extern "C" abx_error_t abx_convert_xml_file_to_abx_file(const char* xml_path, const char* abx_path, const abx_options_t* options) {
    if (!xml_path || !abx_path) {
        return set_error(ABX_ERROR_NULL_POINTER, "Path is null");
    }

    try {
        clear_error();
        std::ofstream out(abx_path, std::ios::binary);
        if (!out) {
            return set_error(ABX_ERROR_WRITE_FAILED, "Failed to open output file");
        }

        libabx::XmlToAbxOptions opts;
        if (options) {
            opts.collapse_whitespaces = options->collapse_whitespaces != 0;
        }

        libabx::convertXmlFileToAbx(xml_path, out, opts);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_convert_xml_string_to_abx_file(const char* xml_string, const char* abx_path, const abx_options_t* options) {
    if (!xml_string || !abx_path) {
        return set_error(ABX_ERROR_NULL_POINTER, "Parameter is null");
    }

    try {
        clear_error();
        std::ofstream out(abx_path, std::ios::binary);
        if (!out) {
            return set_error(ABX_ERROR_WRITE_FAILED, "Failed to open output file");
        }

        libabx::XmlToAbxOptions opts;
        if (options) {
            opts.collapse_whitespaces = options->collapse_whitespaces != 0;
        }

        libabx::convertXmlStringToAbx(xml_string, out, opts);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" size_t abx_convert_xml_file_to_buffer(const char* xml_path, uint8_t* out_buffer, size_t buffer_size, const abx_options_t* options, abx_error_t* error) {
    if (!xml_path) {
        if (error) *error = set_error(ABX_ERROR_NULL_POINTER, "xml_path is null");
        return 0;
    }

    try {
        clear_error();
        std::ostringstream out(std::ios::binary);

        libabx::XmlToAbxOptions opts;
        if (options) {
            opts.collapse_whitespaces = options->collapse_whitespaces != 0;
        }

        libabx::convertXmlFileToAbx(xml_path, out, opts);

        std::string str = out.str();
        size_t size = str.size();

        if (out_buffer && buffer_size >= size) {
            std::memcpy(out_buffer, str.data(), size);
        }

        if (error) *error = ABX_OK;
        return size;
    } catch (const std::exception& e) {
        if (error) *error = handle_exception(e);
        return 0;
    }
}

extern "C" size_t abx_convert_xml_string_to_buffer(const char* xml_string, uint8_t* out_buffer, size_t buffer_size, const abx_options_t* options, abx_error_t* error) {
    if (!xml_string) {
        if (error) *error = set_error(ABX_ERROR_NULL_POINTER, "xml_string is null");
        return 0;
    }

    try {
        clear_error();
        std::ostringstream out(std::ios::binary);

        libabx::XmlToAbxOptions opts;
        if (options) {
            opts.collapse_whitespaces = options->collapse_whitespaces != 0;
        }

        libabx::convertXmlStringToAbx(xml_string, out, opts);

        std::string str = out.str();
        size_t size = str.size();

        if (out_buffer && buffer_size >= size) {
            std::memcpy(out_buffer, str.data(), size);
        }

        if (error) *error = ABX_OK;
        return size;
    } catch (const std::exception& e) {
        if (error) *error = handle_exception(e);
        return 0;
    }
}

extern "C" abx_error_t abx_convert_abx_file_to_xml_file(const char* abx_path, const char* xml_path) {
    if (!abx_path || !xml_path) {
        return set_error(ABX_ERROR_NULL_POINTER, "Path is null");
    }

    try {
        clear_error();
        std::ifstream in(abx_path, std::ios::binary);
        if (!in) {
            return set_error(ABX_ERROR_FILE_NOT_FOUND, "Failed to open ABX file");
        }

        libabx::convertAbxToXmlFile(in, xml_path);
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" abx_error_t abx_convert_abx_buffer_to_xml_file(const uint8_t* abx_data, size_t length, const char* xml_path) {
    if (!abx_data || !xml_path) {
        return set_error(ABX_ERROR_NULL_POINTER, "Parameter is null");
    }

    try {
        clear_error();
        std::vector<char> data(reinterpret_cast<const char*>(abx_data), reinterpret_cast<const char*>(abx_data) + length);
        std::ofstream out(xml_path);
        if (!out) {
            return set_error(ABX_ERROR_WRITE_FAILED, "Failed to open output file");
        }

        libabx::BinaryXmlDeserializer deser(data, out);
        deser.deserialize();
        return ABX_OK;
    } catch (const std::exception& e) {
        return handle_exception(e);
    }
}

extern "C" size_t abx_convert_abx_file_to_string(const char* abx_path, char* out_buffer, size_t buffer_size, abx_error_t* error) {
    if (!abx_path) {
        if (error) *error = set_error(ABX_ERROR_NULL_POINTER, "abx_path is null");
        return 0;
    }

    try {
        clear_error();
        std::ifstream in(abx_path, std::ios::binary);
        if (!in) {
            if (error) *error = set_error(ABX_ERROR_FILE_NOT_FOUND, "Failed to open ABX file");
            return 0;
        }

        std::string result = libabx::convertAbxToXmlString(in);
        size_t needed = result.size() + 1;

        if (out_buffer && buffer_size >= needed) {
            std::memcpy(out_buffer, result.c_str(), needed);
        }

        if (error) *error = ABX_OK;
        return needed;
    } catch (const std::exception& e) {
        if (error) *error = handle_exception(e);
        return 0;
    }
}

extern "C" size_t abx_convert_abx_buffer_to_string(const uint8_t* abx_data, size_t length, char* out_buffer, size_t buffer_size, abx_error_t* error) {
    if (!abx_data) {
        if (error) *error = set_error(ABX_ERROR_NULL_POINTER, "abx_data is null");
        return 0;
    }

    try {
        clear_error();
        std::vector<char> data(reinterpret_cast<const char*>(abx_data), reinterpret_cast<const char*>(abx_data) + length);
        std::ostringstream out;

        libabx::BinaryXmlDeserializer deser(data, out);
        deser.deserialize();

        std::string result = out.str();
        size_t needed = result.size() + 1;

        if (out_buffer && buffer_size >= needed) {
            std::memcpy(out_buffer, result.c_str(), needed);
        }

        if (error) *error = ABX_OK;
        return needed;
    } catch (const std::exception& e) {
        if (error) *error = handle_exception(e);
        return 0;
    }
}

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

extern "C" size_t abx_base64_encode(const uint8_t* data, size_t length, char* out, size_t out_size) {
    if (!data) {
        set_error(ABX_ERROR_NULL_POINTER, "data is null");
        return 0;
    }

    try {
        clear_error();
        std::string encoded = libabx::base64_encode(data, length);
        size_t needed = encoded.size() + 1;

        if (out && out_size >= needed) {
            std::memcpy(out, encoded.c_str(), needed);
        }

        return needed;
    } catch (const std::exception& e) {
        handle_exception(e);
        return 0;
    }
}

extern "C" size_t abx_base64_decode(const char* encoded, uint8_t* out, size_t out_size) {
    if (!encoded) {
        set_error(ABX_ERROR_NULL_POINTER, "encoded is null");
        return 0;
    }

    try {
        clear_error();
        std::vector<uint8_t> decoded = libabx::base64_decode(encoded);
        size_t size = decoded.size();

        if (out && out_size >= size) {
            std::memcpy(out, decoded.data(), size);
        }

        return size;
    } catch (const std::exception& e) {
        handle_exception(e);
        return 0;
    }
}

extern "C" size_t abx_hex_encode(const uint8_t* data, size_t length, char* out, size_t out_size) {
    if (!data) {
        set_error(ABX_ERROR_NULL_POINTER, "data is null");
        return 0;
    }

    try {
        clear_error();
        std::stringstream ss;
        ss << std::hex << std::uppercase;
        for (size_t i = 0; i < length; ++i) {
            ss << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
        }

        std::string encoded = ss.str();
        size_t needed = encoded.size() + 1;

        if (out && out_size >= needed) {
            std::memcpy(out, encoded.c_str(), needed);
        }

        return needed;
    } catch (const std::exception& e) {
        handle_exception(e);
        return 0;
    }
}

extern "C" size_t abx_hex_decode(const char* hex, uint8_t* out, size_t out_size) {
    if (!hex) {
        set_error(ABX_ERROR_NULL_POINTER, "hex is null");
        return 0;
    }

    try {
        clear_error();
        std::vector<uint8_t> decoded = libabx::hex_decode(hex);
        size_t size = decoded.size();

        if (out && out_size >= size) {
            std::memcpy(out, decoded.data(), size);
        }

        return size;
    } catch (const std::exception& e) {
        handle_exception(e);
        return 0;
    }
}
