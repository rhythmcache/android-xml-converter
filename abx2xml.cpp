#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <stdexcept>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <algorithm>
#include <cstring>

// Based on https://github.com/cclgroupltd/android-bits/blob/main/ccl_abx/ccl_abx.py

std::string base64_encode(const unsigned char* data, size_t len) {
    static const char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string encoded;
    encoded.reserve(((len + 2) / 3) * 4);

    for (size_t i = 0; i < len; i += 3) {
        uint32_t triple = (i + 0 < len ? (data[i] << 16) : 0) |
                          (i + 1 < len ? (data[i + 1] << 8) : 0) |
                          (i + 2 < len ? data[i + 2] : 0);

        encoded += base64_chars[(triple >> 18) & 0x3F];
        encoded += base64_chars[(triple >> 12) & 0x3F];
        encoded += (i + 1 < len) ? base64_chars[(triple >> 6) & 0x3F] : '=';
        encoded += (i + 2 < len) ? base64_chars[triple & 0x3F] : '=';
    }
    return encoded;
}

class XMLElement {
public:
    std::string tag;
    std::string text;
    std::unordered_map<std::string, std::string> attrib;
    std::vector<std::shared_ptr<XMLElement>> children;

    XMLElement() = default;
    explicit XMLElement(const std::string& tag_name) : tag(tag_name) {}

    void add_child(const std::shared_ptr<XMLElement>& child) {
        children.push_back(child);
    }
};

enum class XmlType : uint8_t {
    START_DOCUMENT = 0,
    END_DOCUMENT = 1,
    START_TAG = 2,
    END_TAG = 3,
    TEXT = 4,
    ATTRIBUTE = 15
};

enum class DataType : uint8_t {
    TYPE_NULL = 1 << 4,
    TYPE_STRING = 2 << 4,
    TYPE_STRING_INTERNED = 3 << 4,
    TYPE_BYTES_HEX = 4 << 4,
    TYPE_BYTES_BASE64 = 5 << 4,
    TYPE_INT = 6 << 4,
    TYPE_INT_HEX = 7 << 4,
    TYPE_LONG = 8 << 4,
    TYPE_LONG_HEX = 9 << 4,
    TYPE_FLOAT = 10 << 4,
    TYPE_DOUBLE = 11 << 4,
    TYPE_BOOLEAN_TRUE = 12 << 4,
    TYPE_BOOLEAN_FALSE = 13 << 4
};

class AbxDecodeError : public std::runtime_error {
public:
    explicit AbxDecodeError(const std::string& msg) : std::runtime_error(msg) {}
};

class AbxReader {
private:
    std::ifstream stream;
    std::vector<std::string> interned_strings;
    static constexpr char MAGIC[] = "ABX\0";

    uint8_t read_byte() {
        uint8_t byte;
        if (!stream.read(reinterpret_cast<char*>(&byte), 1))
            throw std::runtime_error("Could not read byte");
        return byte;
    }

    int16_t read_short() {
        int16_t val;
        if (!stream.read(reinterpret_cast<char*>(&val), 2))
            throw std::runtime_error("Could not read short");
        return __builtin_bswap16(val);
    }

    uint16_t read_unsigned_short() {
        uint16_t val;
        if (!stream.read(reinterpret_cast<char*>(&val), 2))
            throw std::runtime_error("Could not read unsigned short");
        return __builtin_bswap16(val);
    }

    int32_t read_int() {
        int32_t val;
        if (!stream.read(reinterpret_cast<char*>(&val), 4))
            throw std::runtime_error("Could not read int");
        return __builtin_bswap32(val);
    }

    int64_t read_long() {
        int64_t val;
        if (!stream.read(reinterpret_cast<char*>(&val), 8))
            throw std::runtime_error("Could not read long");
        return __builtin_bswap64(val);
    }

    float read_float() {
        float val;
        if (!stream.read(reinterpret_cast<char*>(&val), 4))
            throw std::runtime_error("Could not read float");
        val = __builtin_bswap32(*reinterpret_cast<int32_t*>(&val));
        return val;
    }

    double read_double() {
        double val;
        if (!stream.read(reinterpret_cast<char*>(&val), 8))
            throw std::runtime_error("Could not read double");
        val = __builtin_bswap64(*reinterpret_cast<int64_t*>(&val));
        return val;
    }

    std::string read_string_raw() {
        uint16_t length = read_unsigned_short();
        std::vector<char> buffer(length);
        if (!stream.read(buffer.data(), length))
            throw std::runtime_error("Could not read string");
        return std::string(buffer.begin(), buffer.end());
    }

    std::string read_interned_string() {
        int16_t reference = read_short();
        if (reference == -1) {
            std::string value = read_string_raw();
            interned_strings.push_back(value);
            return value;
        }
        return interned_strings[reference];
    }

public:
    explicit AbxReader(const std::string& filename) {
        stream.open(filename, std::ios::binary);
        if (!stream)
            throw std::runtime_error("Could not open file");
    }

    std::shared_ptr<XMLElement> read(bool is_multi_root = false) {
        // Validate magic number
        char magic_check[4];
        if (!stream.read(magic_check, 4) || memcmp(magic_check, MAGIC, 4) != 0)
            throw AbxDecodeError("Invalid magic number");

        bool document_opened = true;
        bool root_closed = false;
        std::vector<std::shared_ptr<XMLElement>> element_stack;
        std::shared_ptr<XMLElement> root;

        if (is_multi_root) {
            root = std::make_shared<XMLElement>("root");
            element_stack.push_back(root);
        }

        while (true) {
            if (stream.eof())
                break;

            uint8_t token = read_byte();
            uint8_t xml_type = token & 0x0f;
            uint8_t data_type = token & 0xf0;

            if (xml_type == static_cast<uint8_t>(XmlType::START_DOCUMENT)) {
                if (data_type != static_cast<uint8_t>(DataType::TYPE_NULL))
                    throw AbxDecodeError("Invalid START_DOCUMENT data type");
                document_opened = true;
            }
            else if (xml_type == static_cast<uint8_t>(XmlType::END_DOCUMENT)) {
                if (data_type != static_cast<uint8_t>(DataType::TYPE_NULL))
                    throw AbxDecodeError("Invalid END_DOCUMENT data type");
                if (!(element_stack.empty() || (is_multi_root && element_stack.size() == 1)))
                    throw AbxDecodeError("Unclosed elements at END_DOCUMENT");
                break;
            }
            else if (xml_type == static_cast<uint8_t>(XmlType::START_TAG)) {
                if (data_type != static_cast<uint8_t>(DataType::TYPE_STRING_INTERNED))
                    throw AbxDecodeError("Invalid START_TAG data type");

                std::string tag_name = read_interned_string();
                auto element = std::make_shared<XMLElement>(tag_name);

                if (element_stack.empty()) {
                    root = element;
                    element_stack.push_back(element);
                } else {
                    element_stack.back()->add_child(element);
                    element_stack.push_back(element);
                }
            }
            else if (xml_type == static_cast<uint8_t>(XmlType::END_TAG)) {
                if (data_type != static_cast<uint8_t>(DataType::TYPE_STRING_INTERNED))
                    throw AbxDecodeError("Invalid END_TAG data type");

                if (element_stack.empty() || (is_multi_root && element_stack.size() == 1))
                    throw AbxDecodeError("Unexpected END_TAG");

                std::string tag_name = read_interned_string();
                if (element_stack.back()->tag != tag_name)
                    throw AbxDecodeError("Mismatched END_TAG");

                element_stack.pop_back();
                if (element_stack.empty())
                    root_closed = true;
            }
            else if (xml_type == static_cast<uint8_t>(XmlType::TEXT)) {
                std::string value = read_string_raw();

                // Ignore whitespace
                if (std::all_of(value.begin(), value.end(), ::isspace))
                    continue;

                if (element_stack.back()->text.empty())
                    element_stack.back()->text = value;
                else
                    element_stack.back()->text += value;
            }
            else if (xml_type == static_cast<uint8_t>(XmlType::ATTRIBUTE)) {
                if (element_stack.empty() || (is_multi_root && element_stack.size() == 1))
                    throw AbxDecodeError("Unexpected ATTRIBUTE");

                std::string attribute_name = read_interned_string();
                std::string value;

                switch (static_cast<DataType>(data_type)) {
                    case DataType::TYPE_NULL:
                        value = "null";
                        break;
                    case DataType::TYPE_BOOLEAN_TRUE:
                        value = "true";
                        break;
                    case DataType::TYPE_BOOLEAN_FALSE:
                        value = "false";
                        break;
                    case DataType::TYPE_INT:
                        value = std::to_string(read_int());
                        break;
                    case DataType::TYPE_INT_HEX: {
                        std::stringstream ss;
                        ss << std::hex << read_int();
                        value = ss.str();
                        break;
                    }
                    case DataType::TYPE_LONG:
                        value = std::to_string(read_long());
                        break;
                    case DataType::TYPE_LONG_HEX: {
                        std::stringstream ss;
                        ss << std::hex << read_long();
                        value = ss.str();
                        break;
                    }
                    case DataType::TYPE_FLOAT:
                        value = std::to_string(read_float());
                        break;
                    case DataType::TYPE_DOUBLE:
                        value = std::to_string(read_double());
                        break;
                    case DataType::TYPE_STRING:
                        value = read_string_raw();
                        break;
                    case DataType::TYPE_STRING_INTERNED:
                        value = read_interned_string();
                        break;
                    case DataType::TYPE_BYTES_HEX: {
                        uint16_t length = read_short();
                        std::vector<unsigned char> buffer(length);
                        if (!stream.read(reinterpret_cast<char*>(buffer.data()), length))
                            throw std::runtime_error("Could not read bytes");
                        
                        std::stringstream ss;
                        for (auto byte : buffer)
                            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
                        value = ss.str();
                        break;
                    }
                    case DataType::TYPE_BYTES_BASE64: {
                        uint16_t length = read_short();
                        std::vector<unsigned char> buffer(length);
                        if (!stream.read(reinterpret_cast<char*>(buffer.data()), length))
                            throw std::runtime_error("Could not read bytes");
                        
                        value = base64_encode(buffer.data(), buffer.size());
                        break;
                    }
                    default:
                        throw AbxDecodeError("Unexpected attribute data type");
                }

                element_stack.back()->attrib[attribute_name] = value;
            }
            else {
                throw AbxDecodeError("Unexpected XML type");
            }
        }

        if (!root)
            throw AbxDecodeError("No root element found");

        return root;
    }

    void print_xml(const std::shared_ptr<XMLElement>& element, int indent = 0) {
        std::string indentation(indent, ' ');
        std::cout << indentation << "<" << element->tag;
        
        for (const auto& [key, value] : element->attrib)
            std::cout << " " << key << "=\"" << value << "\"";
        
        if (element->children.empty() && element->text.empty()) {
            std::cout << "/>" << std::endl;
            return;
        }
        
        std::cout << ">";
        
        if (!element->text.empty())
            std::cout << element->text;
        
        if (!element->children.empty()) {
            std::cout << std::endl;
            for (const auto& child : element->children)
                print_xml(child, indent + 2);
            std::cout << indentation;
        }
        
        std::cout << "</" << element->tag << ">" << std::endl;
    }
};

void print_usage() {
    std::cerr << "usage: abx2xml [-mr] -i input [output]\n"
              << "Converts between human-readable XML and Android Binary XML.\n\n"
              << "Options:\n"
              << "  -mr     Enable multi-root XML parsing\n"
              << "  -i      Specify input file (required)\n\n"
              << "When output is not specified, the input file will be overwritten.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 3) { // Need at least program name, -i, and input path
        print_usage();
        return 1;
    }

    bool multi_root = false;
    std::string input_path;
    std::string output_path;
    bool found_i = false;
    
    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-mr") {
            multi_root = true;
        } else if (arg == "-i") {
            if (++i < argc) {
                input_path = argv[i];
                found_i = true;
            } else {
                std::cerr << "Error: -i requires an input path\n";
                print_usage();
                return 1;
            }
        } else if (found_i && output_path.empty() && arg[0] != '-') {
            // If we've found -i and this is not a flag, treat it as output path
            output_path = arg;
        } else {
            std::cerr << "Error: Unknown argument '" << arg << "'\n";
            print_usage();
            return 1;
        }
    }

    if (!found_i) {
        std::cerr << "Error: -i argument is required\n";
        print_usage();
        return 1;
    }

    // If no output path specified, use input path
    if (output_path.empty()) {
        output_path = input_path;
    }

    try {
        AbxReader reader(input_path);
        auto doc = reader.read(multi_root);

        std::ofstream output_file(output_path);
        if (!output_file) {
            std::cerr << "Error: Could not open output file '" << output_path << "'\n";
            return 1;
        }

        // Redirect cout to output file
        auto old_buf = std::cout.rdbuf(output_file.rdbuf());
        
        // Write XML
        reader.print_xml(doc);
        
        // Restore cout
        std::cout.rdbuf(old_buf);

        std::cerr << "Successfully converted " << input_path << " to " << output_path 
                  << (multi_root ? " (multi-root mode)" : "") << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
