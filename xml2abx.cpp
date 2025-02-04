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
#include <stack>
#include <algorithm>
#include <variant>
#include <cstring>

#include <libxml/parser.h>
#include <libxml/tree.h>

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
        xmlInitParser();
        LIBXML_TEST_VERSION

        try {
            xmlDocPtr doc = xmlParseFile(input_path.c_str());
            if (doc == nullptr) {
                throw std::runtime_error("Failed to parse XML file");
            }
            AbxWriter writer(output_path);
            writer.write_start_document();
            xmlNodePtr root = xmlDocGetRootElement(doc);
            process_node(writer, root);
            writer.write_end_document();
            xmlFreeDoc(doc);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            throw;
        }
        xmlCleanupParser();
    }

private:
    static void process_node(AbxWriter& writer, xmlNodePtr node) {
        for (xmlNodePtr current = node; current; current = current->next) {
            if (current->type == XML_ELEMENT_NODE) {
                std::string tag_name = reinterpret_cast<const char*>(current->name);
                writer.write_start_tag(tag_name);

                // Process attributes
                for (xmlAttrPtr attr = current->properties; attr; attr = attr->next) {
                    std::string attr_name = reinterpret_cast<const char*>(attr->name);
                    xmlChar* content = xmlNodeListGetString(attr->doc, attr->children, 1);
                    if (content) {
                        std::string attr_value = reinterpret_cast<const char*>(content);
                        writer.write_attribute(attr_name, attr_value);
                        xmlFree(content);
                    }
                }

                // Process child nodes and text
                if (current->children) {
                    process_node(writer, current->children);
                }

                writer.write_end_tag(tag_name);
            }
            else if (current->type == XML_TEXT_NODE) {
                std::string text = reinterpret_cast<const char*>(current->content);
                if (!std::all_of(text.begin(), text.end(), ::isspace)) {
                    writer.write_text(text);
                }
            }
        }
    }
};

void print_usage() {
    std::cerr << "Usage: xml2abx -i <input-path> -o <output-path>\n";
}

int main(int argc, char* argv[]) {
    std::string input_path;
    std::string output_path;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-i") {
            if (++i < argc) {
                input_path = argv[i];
            } else {
                std::cerr << "Error: -i requires an input path\n";
                print_usage();
                return 1;
            }
        } else if (arg == "-o") {
            if (++i < argc) {
                output_path = argv[i];
            } else {
                std::cerr << "Error: -o requires an output path\n";
                print_usage();
                return 1;
            }
        }
    }
    if (input_path.empty() || output_path.empty()) {
        std::cerr << "Error: Both input and output paths are required\n";
        print_usage();
        return 1;
    }

    try {
        XmlToAbxConverter::convert(input_path, output_path);
        std::cout << "Successfully converted " << input_path << " to " << output_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Conversion failed: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
