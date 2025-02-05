#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <stack>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>


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
        pos = cdata_end + 3; // Skip "]]>"

        XmlNode node(XmlNode::Type::CDATA);
        node.text = cdata_text;
        return node;
    }

    // Parse XML node
    XmlNode parse_node() {
        skip_whitespace();
        if (xml_content.substr(pos, 4) == "<!--")
            return parse_comment();
        if (xml_content.substr(pos, 9) == "<![CDATA[")
            return parse_cdata();

        // Check for opening tag
        if (xml_content[pos] != '<')
            throw std::runtime_error("Expected opening tag");

        pos++;
        skip_whitespace();
        if (xml_content[pos] == '/')
            throw std::runtime_error("Unexpected closing tag");

        // Parse tag name (including namespace prefix if present)
        size_t name_end = xml_content.find_first_of(" />", pos);
        std::string tag_name = xml_content.substr(pos, name_end - pos);
        pos = name_end;

        // Create node
        XmlNode node(XmlNode::Type::ELEMENT, tag_name);

        // Parse attributes
        skip_whitespace();
        while (pos < xml_content.length() &&
               xml_content[pos] != '>' &&
               xml_content[pos] != '/') {
            node.attributes.push_back(parse_attribute());
            skip_whitespace();
        }

        // Check for self-closing tag
        bool is_self_closing = false;
        if (xml_content[pos] == '/') {
            is_self_closing = true;
            pos++;
        }

        // Close tag
        if (xml_content[pos] != '>')
            throw std::runtime_error("Expected '>' to close tag");
        pos++;

        // If self-closing, return node
        if (is_self_closing)
            return node;

        // Parse children and text
        while (pos < xml_content.length()) {
            skip_whitespace();

            // Check for closing tag
            if (xml_content.substr(pos, 2) == "</") {
                pos += 2;
                skip_whitespace();

                // Verify closing tag matches opening tag
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
                // Text content
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
        TYPE_INT = 6 << 4,
        TYPE_FLOAT = 10 << 4,
        TYPE_BOOLEAN_TRUE = 12 << 4,
        TYPE_BOOLEAN_FALSE = 13 << 4
    };

    AbxWriter(const std::string& output_path) : output_stream(output_path, std::ios::binary) {
        if (!output_stream) {
            throw std::runtime_error("Could not open output file");
        }

        // Write magic number
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
        // Read entire XML file
        std::ifstream input_file(input_path);
        if (!input_file) {
            throw std::runtime_error("Could not open input file");
        }
        
        std::string xml_content((std::istreambuf_iterator<char>(input_file)),
                                 std::istreambuf_iterator<char>());
        XmlParser parser;
        XmlNode root = parser.parse(xml_content);
        AbxWriter writer(output_path);
        writer.write_start_document();
        process_node(writer, root);
        writer.write_end_document();
    }

private:
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
    std::cerr << "usage: xml2abx [-i] input [output]\n"
              << "\n"
              << "Converts between human-readable XML and Android Binary XML.\n\n"
              << "When invoked with the '-i' argument, the output of a successful conversion\n"
              << "will overwrite the original input file\n";
}


int main(int argc, char* argv[]) {
    if (argc < 2) { // Need at least input path
        print_usage();
        return 1;
    }

    std::string input_path;
    std::string output_path;
    bool overwrite_input = false;
    
    //  arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i") {
            overwrite_input = true;
        } else if (input_path.empty()) {
            input_path = arg;
        } else if (output_path.empty()) {
            output_path = arg;
        } else {
            std::cerr << "Error: Too many arguments\n";
            print_usage();
            return 1;
        }
    }

    // Validate input path
    if (input_path.empty()) {
        std::cerr << "Error: Input path is required\n";
        print_usage();
        return 1;
    }

    // Determine output path
    if (output_path.empty()) {
        if (overwrite_input) {
            output_path = input_path;
        } else {
            std::cerr << "Error: Output path is required\n";
            print_usage();
            return 1;
        }
    }

    try {
        XmlToAbxConverter::convert(input_path, output_path);
        std::cout << "Successfully converted " << input_path << " to " << output_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
