#include "xml2abx.hpp"
#include "pugixml.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <functional>

namespace xml2abx {


template<typename T>
void write_big_endian(std::ostream& out, T value) {
    uint8_t bytes[sizeof(T)];
    for (size_t i = 0; i < sizeof(T); ++i) {
        bytes[sizeof(T) - 1 - i] = static_cast<uint8_t>(value & 0xFF);
        value >>= 8;
    }
    out.write(reinterpret_cast<const char*>(bytes), sizeof(T));
}


template<>
void write_big_endian<float>(std::ostream& out, float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(float));
    write_big_endian(out, bits);
}


template<>
void write_big_endian<double>(std::ostream& out, double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(double));
    write_big_endian(out, bits);
}

void show_warning(const std::string& feature, const std::optional<std::string>& details) {
    std::cerr << "WARNING: " << feature << " is not supported and might be lost." << std::endl;
    if (details.has_value()) {
        std::cerr << "  " << details.value() << std::endl;
    }
}


FastDataOutput::FastDataOutput(std::ostream& output) 
    : writer_(output) {}

void FastDataOutput::write_byte(uint8_t value) {
    writer_.put(static_cast<char>(value));
}

void FastDataOutput::write_short(uint16_t value) {
    write_big_endian(writer_, value);
}

void FastDataOutput::write_int(int32_t value) {
    write_big_endian(writer_, value);
}

void FastDataOutput::write_long(int64_t value) {
    write_big_endian(writer_, value);
}

void FastDataOutput::write_float(float value) {
    write_big_endian(writer_, value);
}

void FastDataOutput::write_double(double value) {
    write_big_endian(writer_, value);
}

void FastDataOutput::write_utf(const std::string& s) {
    if (s.length() > MAX_UNSIGNED_SHORT) {
        throw StringTooLongError(s.length(), MAX_UNSIGNED_SHORT);
    }
    write_short(static_cast<uint16_t>(s.length()));
    writer_.write(s.data(), s.length());
}

void FastDataOutput::write_interned_utf(const std::string& s) {
    auto it = string_pool_.find(s);
    if (it != string_pool_.end()) {
        write_short(it->second);
    } else {
        write_short(0xFFFF);
        write_utf(s);
        uint16_t index = static_cast<uint16_t>(interned_strings_.size());
        string_pool_[s] = index;
        interned_strings_.push_back(s);
    }
}

void FastDataOutput::write_bytes(const uint8_t* data, size_t length) {
    writer_.write(reinterpret_cast<const char*>(data), length);
}

void FastDataOutput::flush() {
    writer_.flush();
}


BinaryXmlSerializer::BinaryXmlSerializer(std::ostream& output, bool preserve_whitespace)
    : output_(output), tag_count_(0), preserve_whitespace_(preserve_whitespace) {
    output_.write_bytes(PROTOCOL_MAGIC_VERSION_0, 4);
}

void BinaryXmlSerializer::write_token(uint8_t token, const std::optional<std::string>& text) {
    if (text.has_value()) {
        output_.write_byte(token | TYPE_STRING);
        output_.write_utf(text.value());
    } else {
        output_.write_byte(token | TYPE_NULL);
    }
}

void BinaryXmlSerializer::start_document() {
    output_.write_byte(START_DOCUMENT | TYPE_NULL);
}

void BinaryXmlSerializer::end_document() {
    output_.write_byte(END_DOCUMENT | TYPE_NULL);
    output_.flush();
}

void BinaryXmlSerializer::start_tag(const std::string& name) {
    if (tag_count_ == tag_names_.size()) {
        size_t new_size = tag_count_ + std::max(size_t(1), tag_count_ / 2);
        tag_names_.resize(new_size);
    }
    tag_names_[tag_count_] = name;
    tag_count_++;

    output_.write_byte(START_TAG | TYPE_STRING_INTERNED);
    output_.write_interned_utf(name);
}

void BinaryXmlSerializer::end_tag(const std::string& name) {
    tag_count_--;
    output_.write_byte(END_TAG | TYPE_STRING_INTERNED);
    output_.write_interned_utf(name);
}

void BinaryXmlSerializer::attribute(const std::string& name, const std::string& value) {
    output_.write_byte(ATTRIBUTE | TYPE_STRING);
    output_.write_interned_utf(name);
    output_.write_utf(value);
}

void BinaryXmlSerializer::attribute_interned(const std::string& name, const std::string& value) {
    output_.write_byte(ATTRIBUTE | TYPE_STRING_INTERNED);
    output_.write_interned_utf(name);
    output_.write_interned_utf(value);
}

void BinaryXmlSerializer::attribute_boolean(const std::string& name, bool value) {
    uint8_t token = value ? (ATTRIBUTE | TYPE_BOOLEAN_TRUE) : (ATTRIBUTE | TYPE_BOOLEAN_FALSE);
    output_.write_byte(token);
    output_.write_interned_utf(name);
}

void BinaryXmlSerializer::attribute_int(const std::string& name, int32_t value) {
    output_.write_byte(ATTRIBUTE | TYPE_INT);
    output_.write_interned_utf(name);
    output_.write_int(value);
}

void BinaryXmlSerializer::attribute_long(const std::string& name, int64_t value) {
    output_.write_byte(ATTRIBUTE | TYPE_LONG);
    output_.write_interned_utf(name);
    output_.write_long(value);
}

void BinaryXmlSerializer::attribute_float(const std::string& name, float value) {
    output_.write_byte(ATTRIBUTE | TYPE_FLOAT);
    output_.write_interned_utf(name);
    output_.write_float(value);
}

void BinaryXmlSerializer::attribute_double(const std::string& name, double value) {
    output_.write_byte(ATTRIBUTE | TYPE_DOUBLE);
    output_.write_interned_utf(name);
    output_.write_double(value);
}

void BinaryXmlSerializer::attribute_bytes_hex(const std::string& name, const uint8_t* data, size_t length) {
    if (length > FastDataOutput::MAX_UNSIGNED_SHORT) {
        throw BinaryDataTooLongError(length, FastDataOutput::MAX_UNSIGNED_SHORT);
    }
    output_.write_byte(ATTRIBUTE | TYPE_BYTES_HEX);
    output_.write_interned_utf(name);
    output_.write_short(static_cast<uint16_t>(length));
    output_.write_bytes(data, length);
}

void BinaryXmlSerializer::attribute_bytes_base64(const std::string& name, const uint8_t* data, size_t length) {
    if (length > FastDataOutput::MAX_UNSIGNED_SHORT) {
        throw BinaryDataTooLongError(length, FastDataOutput::MAX_UNSIGNED_SHORT);
    }
    output_.write_byte(ATTRIBUTE | TYPE_BYTES_BASE64);
    output_.write_interned_utf(name);
    output_.write_short(static_cast<uint16_t>(length));
    output_.write_bytes(data, length);
}

void BinaryXmlSerializer::text(const std::string& text) {
    write_token(TEXT, text);
}

void BinaryXmlSerializer::cdsect(const std::string& text) {
    write_token(CDSECT, text);
}

void BinaryXmlSerializer::comment(const std::string& text) {
    write_token(COMMENT, text);
}

void BinaryXmlSerializer::processing_instruction(const std::string& target, const std::optional<std::string>& data) {
    std::string full_pi;
    if (data.has_value() && !data.value().empty()) {
        full_pi = target + " " + data.value();
    } else {
        full_pi = target;
    }
    write_token(PROCESSING_INSTRUCTION, full_pi);
}

void BinaryXmlSerializer::docdecl(const std::string& text) {
    write_token(DOCDECL, text);
}

void BinaryXmlSerializer::ignorable_whitespace(const std::string& text) {
    write_token(IGNORABLE_WHITESPACE, text);
}

void BinaryXmlSerializer::entity_ref(const std::string& text) {
    write_token(ENTITY_REF, text);
}


namespace type_detection {
    bool is_boolean(const std::string& s) {
        return s == "true" || s == "false";
    }

    bool is_whitespace_only(const std::string& s) {
        return std::all_of(s.begin(), s.end(), [](unsigned char c) {
            return std::isspace(c);
        });
    }
}


void XmlToAbxConverter::write_attribute(BinaryXmlSerializer& serializer, 
                                       const std::string& name, 
                                       const std::string& value) {
    
    if (type_detection::is_boolean(value)) {
        serializer.attribute_boolean(name, value == "true");
    } else {
        
        
        if (value.length() < 50 && value.find(' ') == std::string::npos) {
            serializer.attribute_interned(name, value);
        } else {
            serializer.attribute(name, value);
        }
    }
}

void XmlToAbxConverter::convert_from_string(const std::string& xml, std::ostream& output, 
                                           bool preserve_whitespace) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(xml.c_str(), 
        preserve_whitespace ? pugi::parse_default : pugi::parse_trim_pcdata);
    
    if (!result) {
        throw ConversionError(std::string("XML parsing failed: ") + result.description());
    }

    BinaryXmlSerializer serializer(output, preserve_whitespace);
    serializer.start_document();

    
    std::function<void(const pugi::xml_node&)> process_node;
    process_node = [&](const pugi::xml_node& node) {
        for (const auto& child : node.children()) {
            switch (child.type()) {
                case pugi::node_element: {
                    std::string name = child.name();
                    if (name.find(':') != std::string::npos) {
                        show_warning("Namespaces and prefixes", 
                                   "Found prefixed element: " + name);
                    }

                    serializer.start_tag(name);

                    
                    for (const auto& attr : child.attributes()) {
                        std::string attr_name = attr.name();
                        std::string attr_value = attr.value();
                        
                        if (attr_name.find("xmlns") == 0 || attr_name.find(':') != std::string::npos) {
                            show_warning("Namespaces and prefixes",
                                       "Found namespace declaration or prefixed attribute: " + attr_name);
                        }

                        write_attribute(serializer, attr_name, attr_value);
                    }

                    
                    process_node(child);

                    serializer.end_tag(name);
                    break;
                }
                case pugi::node_pcdata: {
                    std::string text = child.value();
                    if (type_detection::is_whitespace_only(text)) {
                        if (preserve_whitespace) {
                            serializer.ignorable_whitespace(text);
                        }
                    } else {
                        serializer.text(text);
                    }
                    break;
                }
                case pugi::node_cdata: {
                    serializer.cdsect(child.value());
                    break;
                }
                case pugi::node_comment: {
                    serializer.comment(child.value());
                    break;
                }
                case pugi::node_pi: {
                    std::string target = child.name();
                    std::string data = child.value();
                    serializer.processing_instruction(target, 
                        data.empty() ? std::nullopt : std::optional<std::string>(data));
                    break;
                }
                case pugi::node_doctype: {
                    serializer.docdecl(child.value());
                    break;
                }
                default:
                    break;
            }
        }
    };

    process_node(doc);

    serializer.end_document();
}

void XmlToAbxConverter::convert_from_file(const std::string& input_path, std::ostream& output,
                                         bool preserve_whitespace) {
    std::ifstream file(input_path);
    if (!file) {
        throw ConversionError("Cannot open file: " + input_path);
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    convert_from_string(buffer.str(), output, preserve_whitespace);
}

} 