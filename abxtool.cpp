/*Copyright [2025] [rhythmcache]

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/


/*
This C++ implementation originates from:
https://github.com/rhythmcache/android-xml-converter/
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stack>
#include <memory>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <unordered_map>
#include <iomanip>
class AbxReader;
class AbxWriter;
class XmlParser;
class XmlToAbxConverter;
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
class XmlNode {
public:
    enum class Type { ELEMENT, TEXT, CDATA, COMMENT };
    Type type;
    std::string name;
    std::string text;
    std::vector<std::pair<std::string, std::string>> attributes;
    std::vector<XmlNode> children;
    XmlNode(Type t, const std::string& n = "") : type(t), name(n) {}
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
    void skip_header_extension() {
        while (true) {
            uint8_t token = read_byte();
            if ((token & 0x0f) == static_cast<uint8_t>(XmlType::START_DOCUMENT)) {
                stream.seekg(-1, std::ios::cur);
                break;
            }
            uint8_t data_type = token & 0xf0;
            switch (static_cast<DataType>(data_type)) {
                case DataType::TYPE_NULL:
                    break;
                case DataType::TYPE_INT:
                    read_int();
                    break;
                case DataType::TYPE_LONG:
                    read_long();
                    break;
                case DataType::TYPE_FLOAT:
                    read_float();
                    break;
                case DataType::TYPE_DOUBLE:
                    read_double();
                    break;
                case DataType::TYPE_STRING:
                case DataType::TYPE_STRING_INTERNED:
                    read_string_raw();
                    break;
                case DataType::TYPE_BYTES_HEX:
                case DataType::TYPE_BYTES_BASE64:
                    {
                        uint16_t length = read_short();
                        stream.seekg(length, std::ios::cur);
                    }
                    break;
                default:
                    if ((token & 0x0f) > 0) {
                        stream.seekg(token & 0x0f, std::ios::cur);
                    }
                    break;
            }
        }
    }
public:
    explicit AbxReader(const std::string& filename) {
        stream.open(filename, std::ios::binary);
        if (!stream)
            throw std::runtime_error("Could not open file");
    }
    std::shared_ptr<XMLElement> read(bool is_multi_root = false) {
        char magic_check[4];
        if (!stream.read(magic_check, 4) || memcmp(magic_check, MAGIC, 4) != 0)
            throw AbxDecodeError("Invalid magic number");
        skip_header_extension();
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
                if (std::all_of(value.begin(), value.end(), ::isspace))
                    continue;
                if (element_stack.empty())
                    throw AbxDecodeError("Unexpected TEXT outside of element");
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
                if (data_type != 0) {
                    switch (static_cast<DataType>(data_type)) {
                        case DataType::TYPE_INT:
                            read_int();
                            break;
                        case DataType::TYPE_STRING:
                        case DataType::TYPE_STRING_INTERNED:
                            read_string_raw();
                            break;
                        default:
                            throw AbxDecodeError("Unexpected XML type");
                    }
                }
            }
        }
        if (!root)
            throw AbxDecodeError("No root element found");
        return root;
    }
    void print_xml(const std::shared_ptr<XMLElement>& element, int indent = 0) {
        if (indent == 0) {
            std::cout << "<?xml version='1.0' encoding='UTF-8' standalone='yes' ?>\n";
        }
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
class XmlParser {
private:
    std::string xml_content;
    size_t pos = 0;
    std::string trim(const std::string& str) {
        auto start = std::find_if_not(str.begin(), str.end(), ::isspace);
        auto end = std::find_if_not(str.rbegin(), str.rend(), ::isspace).base();
        return (start < end) ? std::string(start, end) : "";
    }
    void skip_whitespace() {
        while (pos < xml_content.length() && ::isspace(xml_content[pos])) pos++;
    }
    std::pair<std::string, std::string> parse_attribute() {
        skip_whitespace();
        size_t name_end = xml_content.find_first_of("=", pos);
        if (name_end == std::string::npos)
            throw std::runtime_error("Invalid attribute format");
        std::string name = trim(xml_content.substr(pos, name_end - pos));
        pos = name_end + 1;
        skip_whitespace();
        char quote = xml_content[pos];
        if (quote != '"' && quote != '\'')
            throw std::runtime_error("Attribute value must be quoted");
        pos++;
        size_t value_end = xml_content.find(quote, pos);
        if (value_end == std::string::npos)
            throw std::runtime_error("Unclosed attribute value");
        std::string value = xml_content.substr(pos, value_end - pos);
        pos = value_end + 1;
        return {name, value};
    }
    XmlNode parse_comment() {
        if (xml_content.substr(pos, 4) != "<!--")
            throw std::runtime_error("Expected comment start");
        pos += 4;
        size_t comment_end = xml_content.find("-->", pos);
        if (comment_end == std::string::npos)
            throw std::runtime_error("Unclosed comment");
        std::string comment_text = xml_content.substr(pos, comment_end - pos);
        pos = comment_end + 3;
        XmlNode node(XmlNode::Type::COMMENT);
        node.text = comment_text;
        return node;
    }
    XmlNode parse_cdata() {
        if (xml_content.substr(pos, 9) != "<![CDATA[")
            throw std::runtime_error("Expected CDATA start");
        pos += 9;
        size_t cdata_end = xml_content.find("]]>", pos);
        if (cdata_end == std::string::npos)
            throw std::runtime_error("Unclosed CDATA section");
        std::string cdata_text = xml_content.substr(pos, cdata_end - pos);
        pos = cdata_end + 3;
        XmlNode node(XmlNode::Type::CDATA);
        node.text = cdata_text;
        return node;
    }
    XmlNode parse_node() {
        skip_whitespace();
        if (xml_content.substr(pos, 4) == "<!--")
            return parse_comment();
        if (xml_content.substr(pos, 9) == "<![CDATA[")
            return parse_cdata();
        if (xml_content[pos] != '<')
            throw std::runtime_error("Expected opening tag");
        pos++;
        skip_whitespace();
        if (xml_content[pos] == '/')
            throw std::runtime_error("Unexpected closing tag");
        size_t name_end = xml_content.find_first_of(" />", pos);
        std::string tag_name = xml_content.substr(pos, name_end - pos);
        pos = name_end;
        XmlNode node(XmlNode::Type::ELEMENT, tag_name);
        skip_whitespace();
        while (pos < xml_content.length() &&
               xml_content[pos] != '>' &&
               xml_content[pos] != '/') {
            node.attributes.push_back(parse_attribute());
            skip_whitespace();
        }
        bool is_self_closing = false;
        if (xml_content[pos] == '/') {
            is_self_closing = true;
            pos++;
        }
        if (xml_content[pos] != '>')
            throw std::runtime_error("Expected '>' to close tag");
        pos++;
        if (is_self_closing)
            return node;
        while (pos < xml_content.length()) {
            skip_whitespace();
            if (xml_content.substr(pos, 2) == "</") {
                pos += 2;
                skip_whitespace();
                size_t close_end = xml_content.find('>', pos);
                std::string closing_tag = trim(xml_content.substr(pos, close_end - pos));
                if (closing_tag != tag_name)
                    throw std::runtime_error("Mismatched closing tag");
                pos = close_end + 1;
                break;
            }
            if (xml_content[pos] == '<') {
                node.children.push_back(parse_node());
            } else {
                size_t text_end = xml_content.find('<', pos);
                std::string text = trim(xml_content.substr(pos, text_end - pos));
                if (!text.empty()) {
                    XmlNode text_node(XmlNode::Type::TEXT);
                    text_node.text = text;
                    node.children.push_back(text_node);
                }
                pos = text_end;
            }
        }
        return node;
    }
public:
    XmlNode parse(const std::string& xml) {
        xml_content = xml;
        pos = 0;
        if (xml_content.substr(0, 5) == "<?xml") {
            size_t decl_end = xml_content.find("?>", 5);
            if (decl_end != std::string::npos)
                pos = decl_end + 2;
        }
        return parse_node();
    }
};
class AbxWriter {
public:
    AbxWriter(const std::string& output_path) : output_stream(output_path, std::ios::binary) {
        if (!output_stream) {
            throw std::runtime_error("Could not open output file");
        }
        const char magic[] = "ABX\0";
        output_stream.write(magic, 4);
    }
    void write_start_document() {
        write_token(XmlType::START_DOCUMENT, DataType::TYPE_NULL);
    }
    void write_end_document() {
        write_token(XmlType::END_DOCUMENT, DataType::TYPE_NULL);
    }
    void write_start_tag(const std::string& tag_name) {
        write_token(XmlType::START_TAG, DataType::TYPE_STRING_INTERNED);
        write_string_interned(tag_name);
    }
    void write_end_tag(const std::string& tag_name) {
        write_token(XmlType::END_TAG, DataType::TYPE_STRING_INTERNED);
        write_string_interned(tag_name);
    }
    void write_attribute(const std::string& name, const std::string& value) {
        write_token(XmlType::ATTRIBUTE, DataType::TYPE_STRING);
        write_string_interned(name);
        write_string(value);
    }
    void write_text(const std::string& text) {
        write_token(XmlType::TEXT, DataType::TYPE_STRING);
        write_string(text);
    }
private:
    std::ofstream output_stream;
    std::vector<std::string> interned_strings;
    void write_token(XmlType xml_type, DataType data_type) {
        uint8_t token = static_cast<uint8_t>(xml_type) | static_cast<uint8_t>(data_type);
        output_stream.write(reinterpret_cast<char*>(&token), 1);
    }
    void write_string(const std::string& str) {
        uint16_t length = str.length();
        uint16_t be_length = __builtin_bswap16(length);
        output_stream.write(reinterpret_cast<char*>(&be_length), 2);
        output_stream.write(str.data(), length);
    }
    void write_string_interned(const std::string& str) {
        auto it = std::find(interned_strings.begin(), interned_strings.end(), str);
        if (it != interned_strings.end()) {
            int16_t index = std::distance(interned_strings.begin(), it);
            int16_t be_index = __builtin_bswap16(index);
            output_stream.write(reinterpret_cast<char*>(&be_index), 2);
        } else {
            int16_t index = -1;
            int16_t be_index = __builtin_bswap16(index);
            output_stream.write(reinterpret_cast<char*>(&be_index), 2);
            write_string(str);
            interned_strings.push_back(str);
        }
    }
};
class XmlToAbxConverter {
public:
    static void convert(const std::string& input_path, const std::string& output_path) {
        std::string xml_content;
        if (input_path == "-") {
            xml_content = read_from_stdin();
        } else {
            std::ifstream input_file(input_path);
            if (!input_file) {
                throw std::runtime_error("Could not open input file");
            }
            xml_content = std::string((std::istreambuf_iterator<char>(input_file)),
                                     std::istreambuf_iterator<char>());
        }
        XmlParser parser;
        XmlNode root = parser.parse(xml_content);
        AbxWriter writer(output_path);
        writer.write_start_document();
        process_node(writer, root);
        writer.write_end_document();
    }
private:
    static std::string read_from_stdin() {
        std::stringstream buffer;
        buffer << std::cin.rdbuf();
        return buffer.str();
    }
    static void process_node(AbxWriter& writer, const XmlNode& node) {
        if (node.type == XmlNode::Type::ELEMENT) {
            writer.write_start_tag(node.name);
            for (const auto& attr : node.attributes) {
                writer.write_attribute(attr.first, attr.second);
            }
            for (const auto& child : node.children) {
                process_node(writer, child);
            }
            writer.write_end_tag(node.name);
        }
        else if (node.type == XmlNode::Type::TEXT) {
            if (!std::all_of(node.text.begin(), node.text.end(), ::isspace)) {
                writer.write_text(node.text);
            }
        }
    }
};
void print_usage() {
    std::cerr << "usage: abxtool <command> [options] input [output]\n"
              << "\n"
              << "Commands:\n"
              << "  abx2xml  : Convert Android Binary XML to human-readable XML\n"
              << "  xml2abx  : Convert human-readable XML to Android Binary XML\n"
              << "\n"
              << "Options:\n"
              << "  -i       : Overwrite input file with output\n"
              << "  -mr      : Enable Multi-Root Processing (abx2xml only)\n"
              << "\n"
              << "Input:\n"
              << "  Use '-' as input to read from stdin (xml2abx only)\n"
              << "  When reading from stdin, output path must be specified\n"
              << "\n"
              << "Output:\n"
              << "  Use '-' as output to write to stdout (abx2xml only)\n";
}
int main(int argc, char* argv[]) {
    if (argc < 3) {
        print_usage();
        return 1;
    }
    std::string command = argv[1];
    if (command != "abx2xml" && command != "xml2abx") {
        std::cerr << "Error: Invalid command. Use 'abx2xml' or 'xml2abx'\n";
        print_usage();
        return 1;
    }
    bool is_abx2xml = (command == "abx2xml");
    std::string input_path;
    std::string output_path;
    bool overwrite_input = false;
    bool multi_root = false;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i") {
            overwrite_input = true;
        } 
        else if (arg == "-mr" && is_abx2xml) {
            multi_root = true;
        }
        else if (input_path.empty()) {
            input_path = arg;
        } 
        else if (output_path.empty()) {
            output_path = arg;
        } 
        else {
            std::cerr << "Error: Too many arguments\n";
            print_usage();
            return 1;
        }
    }
    if (input_path.empty()) {
        std::cerr << "Error: Input path is required\n";
        print_usage();
        return 1;
    }
    if (input_path == "-" && !is_abx2xml) {
        if (output_path.empty()) {
            std::cerr << "Error: Output path is required when reading from stdin\n";
            return 1;
        }
    } else if (overwrite_input) {
        if (!output_path.empty()) {
            std::cerr << "Error: Cannot specify output path with -i option\n";
            return 1;
        }
        output_path = input_path + ".tmp";
    } else if (output_path.empty()) {
        output_path = is_abx2xml ? "-" : input_path + ".abx";
    }
    try {
        if (is_abx2xml) {
            AbxReader reader(input_path);
            auto root = reader.read(multi_root);
            if (output_path == "-") {
                reader.print_xml(root);
            } else {
                std::ofstream output_file(output_path);
                if (!output_file) {
                    throw std::runtime_error("Could not open output file");
                }
                auto cout_buf = std::cout.rdbuf(output_file.rdbuf());
                reader.print_xml(root);
                std::cout.rdbuf(cout_buf);
            }
        } else {
            XmlToAbxConverter::convert(input_path, output_path);
        }
        if (overwrite_input) {
            std::remove(input_path.c_str());
            std::rename(output_path.c_str(), input_path.c_str());
        }
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        if (overwrite_input) {
            std::remove(output_path.c_str());
        }
        return 1;
    }
}