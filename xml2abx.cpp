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
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <map>
#include <unordered_map>
#include <memory>


void show_warning(const std::string& feature, const std::string& details = "") {
    std::cerr << "WARNING: " << feature << " is not supported and will be lost." << std::endl;
    if (!details.empty()) {
        std::cerr << "  " << details << std::endl;
    }
}

class XmlNode {
public:
    enum class Type { ELEMENT, TEXT, CDATA, COMMENT, PROCESSING_INSTRUCTION, DOCDECL, ENTITY_REF, IGNORABLE_WHITESPACE };

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
    bool namespace_warning_shown = false;
    
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
    
    // Check for namespace declarations and prefixed attributes
    if ((name.find("xmlns") == 0 || name.find(':') != std::string::npos) && !namespace_warning_shown) {
        show_warning("Namespaces and prefixes", "Found namespace declaration or prefixed attribute: " + name);
        namespace_warning_shown = true;
    }
    
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

    XmlNode parse_processing_instruction() {
        if (xml_content.substr(pos, 2) != "<?")
            throw std::runtime_error("Expected PI start");

        pos += 2;
        size_t pi_end = xml_content.find("?>", pos);
        if (pi_end == std::string::npos)
            throw std::runtime_error("Unclosed processing instruction");

        std::string pi_text = xml_content.substr(pos, pi_end - pos);
        pos = pi_end + 2;

        XmlNode node(XmlNode::Type::PROCESSING_INSTRUCTION);
        node.text = pi_text;
        return node;
    }

    XmlNode parse_doctype() {
        if (xml_content.substr(pos, 9) != "<!DOCTYPE")
            throw std::runtime_error("Expected DOCTYPE start");

        pos += 9;
        size_t doctype_end = xml_content.find('>', pos);
        if (doctype_end == std::string::npos)
            throw std::runtime_error("Unclosed DOCTYPE");

        std::string doctype_text = xml_content.substr(pos, doctype_end - pos);
        pos = doctype_end + 1;

        XmlNode node(XmlNode::Type::DOCDECL);
        node.text = doctype_text;
        return node;
    }

        XmlNode parse_node() {
        skip_whitespace();
        
        if (pos >= xml_content.length()) {
            throw std::runtime_error("Unexpected end of document");
        }
        
        if (xml_content.substr(pos, 4) == "<!--")
            return parse_comment();
        if (xml_content.substr(pos, 9) == "<![CDATA[")
            return parse_cdata();
        if (xml_content.substr(pos, 2) == "<?")
            return parse_processing_instruction();
        if (xml_content.substr(pos, 9) == "<!DOCTYPE")
            return parse_doctype();
        if (xml_content[pos] != '<')
            throw std::runtime_error("Expected opening tag");

        pos++;
        skip_whitespace();
        if (xml_content[pos] == '/')
            throw std::runtime_error("Unexpected closing tag");

        size_t name_end = xml_content.find_first_of(" />", pos);
std::string tag_name = xml_content.substr(pos, name_end - pos);

// Check for prefixed element names
if (tag_name.find(':') != std::string::npos && !namespace_warning_shown) {
    show_warning("Namespaces and prefixes", "Found prefixed element: " + tag_name);
    namespace_warning_shown = true;
}

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

        // Don't skip whitespace after opening tag
        // Skipping might break indentation and newlines
        while (pos < xml_content.length()) {
            if (xml_content.substr(pos, 2) == "</") {
                pos += 2;
                skip_whitespace();

                size_t close_end = xml_content.find('>', pos);
                std::string closing_tag = trim(xml_content.substr(pos, close_end - pos));

                if (closing_tag != tag_name)
                    throw std::runtime_error("Mismatched closing tag: expected " + tag_name + ", got " + closing_tag);

                pos = close_end + 1;
                break;
            }
            
            if (xml_content[pos] == '<') {
                node.children.push_back(parse_node());
            } else {
                size_t text_end = xml_content.find('<', pos);
                std::string text = xml_content.substr(pos, text_end - pos);

                // Always preserve text content, including whitespace
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
        
        // Create a document root to hold all top-level nodes
        XmlNode document_root(XmlNode::Type::ELEMENT, "document");
        
        // Don't skip leading whitespace - it might be Important ???
        
        // Skip XML declaration if present
        if (pos < xml_content.length() && xml_content.substr(pos, 5) == "<?xml") {
            size_t decl_end = xml_content.find("?>", pos + 5);
            if (decl_end != std::string::npos)
                pos = decl_end + 2;
        }
        
        // Parse all nodes in the document, preserving whitespace between top-level elements
        while (pos < xml_content.length()) {
            if (xml_content[pos] == '<') {
                try {
                    XmlNode node = parse_node();
                    document_root.children.push_back(node);
                } catch (const std::exception& e) {
                    std::cerr << "Warning: Failed to parse node at position " << pos << ": " << e.what() << std::endl;
                    // Try to recover by finding the next '<' character
                    size_t next_tag = xml_content.find('<', pos + 1);
                    if (next_tag != std::string::npos) {
                        pos = next_tag;
                        continue;
                    } else {
                        break;
                    }
                }
            } else {
                // Handle whitespace / text between top level elements
                size_t next_tag = xml_content.find('<', pos);
                if (next_tag != std::string::npos) {
                    std::string text = xml_content.substr(pos, next_tag - pos);
                    if (!text.empty()) {
                        // Check if this is just whitespace (newlines, spaces, tabs)
                        bool is_whitespace_only = std::all_of(text.begin(), text.end(), 
                            [](char c) { return std::isspace(c); });
                        
                        if (is_whitespace_only) {
                            // Preserve as ignorable whitespace for top-level
                            XmlNode text_node(XmlNode::Type::IGNORABLE_WHITESPACE);
                            text_node.text = text;
                            document_root.children.push_back(text_node);
                        } else {
                            // Non-whitespace text at document level
                            XmlNode text_node(XmlNode::Type::TEXT);
                            text_node.text = text;
                            document_root.children.push_back(text_node);
                        }
                    }
                    pos = next_tag;
                } else {
                    // No more tags, handle remaining content
                    std::string remaining = xml_content.substr(pos);
                    if (!remaining.empty() && !std::all_of(remaining.begin(), remaining.end(), ::isspace)) {
                        std::cerr << "Warning: Unexpected content at document level" << std::endl;
                    }
                    break;
                }
            }
        }
        
        return document_root;
    }
};


class FastDataOutput {
private:
    std::ofstream& output_stream;
    std::unordered_map<std::string, uint16_t> string_pool;
    std::vector<std::string> interned_strings;
    
public:
    static const uint16_t MAX_UNSIGNED_SHORT = 65535;
    
    FastDataOutput(std::ofstream& os) : output_stream(os) {}
    
    void writeByte(uint8_t value) {
        output_stream.write(reinterpret_cast<const char*>(&value), 1);
    }
    
    void writeShort(uint16_t value) {
        uint16_t be_value = ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
        output_stream.write(reinterpret_cast<const char*>(&be_value), 2);
    }
    
    void writeInt(int32_t value) {
        int32_t be_value = ((value & 0xFF) << 24) | 
                          (((value >> 8) & 0xFF) << 16) | 
                          (((value >> 16) & 0xFF) << 8) | 
                          ((value >> 24) & 0xFF);
        output_stream.write(reinterpret_cast<const char*>(&be_value), 4);
    }
    
    void writeLong(int64_t value) {
        for (int i = 7; i >= 0; i--) {
            uint8_t byte = (value >> (i * 8)) & 0xFF;
            writeByte(byte);
        }
    }
    
    void writeFloat(float value) {
        uint32_t int_value = *reinterpret_cast<uint32_t*>(&value);
        writeInt(int_value);
    }
    
    void writeDouble(double value) {
        uint64_t int_value = *reinterpret_cast<uint64_t*>(&value);
        writeLong(int_value);
    }
    
    void writeUTF(const std::string& str) {
    if (str.length() > MAX_UNSIGNED_SHORT) {
        show_warning("String length exceeds 65,535 bytes", "String will be truncated: " + str.substr(0, 50) + "...");
        throw std::runtime_error("String length exceeds maximum allowed size");
    }
    writeShort(static_cast<uint16_t>(str.length()));
    output_stream.write(str.data(), str.length());
}
    
    void writeInternedUTF(const std::string& str) {
        auto it = string_pool.find(str);
        if (it != string_pool.end()) {
            writeShort(it->second);
        } else {
            writeShort(0xFFFF);
            writeUTF(str);
            string_pool[str] = static_cast<uint16_t>(interned_strings.size());
            interned_strings.push_back(str);
        }
    }
    
    void write(const std::vector<uint8_t>& data) {
        output_stream.write(reinterpret_cast<const char*>(data.data()), data.size());
    }
    
    void flush() {
        output_stream.flush();
    }
};

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
    
    void writeToken(uint8_t token, const std::string* text = nullptr) {
        if (text != nullptr) {
            mOut->writeByte(token | TYPE_STRING);
            mOut->writeUTF(*text);
        } else {
            mOut->writeByte(token | TYPE_NULL);
        }
    }

public:
    BinaryXmlSerializer(std::ofstream& os) {
        mOut = std::make_unique<FastDataOutput>(os);
        os.write(reinterpret_cast<const char*>(PROTOCOL_MAGIC_VERSION_0), 4);
        mTagCount = 0;
        mTagNames.reserve(8);
    }
    
    void startDocument() {
        mOut->writeByte(START_DOCUMENT | TYPE_NULL);
    }
    
    void endDocument() {
        mOut->writeByte(END_DOCUMENT | TYPE_NULL);
        mOut->flush();
    }
    
    void startTag(const std::string& name) {
        if (mTagCount == mTagNames.size()) {
            mTagNames.resize(mTagCount + std::max(1, mTagCount / 2));
        }
        mTagNames[mTagCount++] = name;
        mOut->writeByte(START_TAG | TYPE_STRING_INTERNED);
        mOut->writeInternedUTF(name);
    }
    
    void endTag(const std::string& name) {
        mTagCount--;
        mOut->writeByte(END_TAG | TYPE_STRING_INTERNED);
        mOut->writeInternedUTF(name);
    }
    
    void attribute(const std::string& name, const std::string& value) {
        mOut->writeByte(ATTRIBUTE | TYPE_STRING);
        mOut->writeInternedUTF(name);
        mOut->writeUTF(value);
    }
    
    void attributeInterned(const std::string& name, const std::string& value) {
        mOut->writeByte(ATTRIBUTE | TYPE_STRING_INTERNED);
        mOut->writeInternedUTF(name);
        mOut->writeInternedUTF(value);
    }
    
    void attributeInt(const std::string& name, int32_t value) {
        mOut->writeByte(ATTRIBUTE | TYPE_INT);
        mOut->writeInternedUTF(name);
        mOut->writeInt(value);
    }
    
    void attributeIntHex(const std::string& name, int32_t value) {
        mOut->writeByte(ATTRIBUTE | TYPE_INT_HEX);
        mOut->writeInternedUTF(name);
        mOut->writeInt(value);
    }
    
    void attributeLong(const std::string& name, int64_t value) {
        mOut->writeByte(ATTRIBUTE | TYPE_LONG);
        mOut->writeInternedUTF(name);
        mOut->writeLong(value);
    }
    
    void attributeLongHex(const std::string& name, int64_t value) {
        mOut->writeByte(ATTRIBUTE | TYPE_LONG_HEX);
        mOut->writeInternedUTF(name);
        mOut->writeLong(value);
    }
    
    void attributeFloat(const std::string& name, float value) {
        mOut->writeByte(ATTRIBUTE | TYPE_FLOAT);
        mOut->writeInternedUTF(name);
        mOut->writeFloat(value);
    }
    
    void attributeDouble(const std::string& name, double value) {
        mOut->writeByte(ATTRIBUTE | TYPE_DOUBLE);
        mOut->writeInternedUTF(name);
        mOut->writeDouble(value);
    }
    
    void attributeBoolean(const std::string& name, bool value) {
        if (value) {
            mOut->writeByte(ATTRIBUTE | TYPE_BOOLEAN_TRUE);
        } else {
            mOut->writeByte(ATTRIBUTE | TYPE_BOOLEAN_FALSE);
        }
        mOut->writeInternedUTF(name);
    }
    
    void attributeBytesHex(const std::string& name, const std::vector<uint8_t>& value) {
    if (value.size() > FastDataOutput::MAX_UNSIGNED_SHORT) {
        show_warning("Byte array length exceeds 65,535 bytes", "Attribute: " + name + ", size: " + std::to_string(value.size()));
    }
        mOut->writeByte(ATTRIBUTE | TYPE_BYTES_HEX);
        mOut->writeInternedUTF(name);
        mOut->writeShort(static_cast<uint16_t>(value.size()));
        mOut->write(value);
    }
    
    void attributeBytesHex64(const std::string& name, const std::vector<uint8_t>& value) {
    if (value.size() > FastDataOutput::MAX_UNSIGNED_SHORT) {
        show_warning("Byte array length exceeds 65,535 bytes", "Attribute: " + name + ", size: " + std::to_string(value.size()));
    }
        mOut->writeByte(ATTRIBUTE | TYPE_BYTES_BASE64);
        mOut->writeInternedUTF(name);
        mOut->writeShort(static_cast<uint16_t>(value.size()));
        mOut->write(value);
    }
    
    void text(const std::string& text) {
        writeToken(TEXT, &text);
    }
    
    void cdsect(const std::string& text) {
        writeToken(CDSECT, &text);
    }
    
    void entityRef(const std::string& text) {
        writeToken(ENTITY_REF, &text);
    }
    
    void processingInstruction(const std::string& text) {
        writeToken(PROCESSING_INSTRUCTION, &text);
    }
    
    void comment(const std::string& text) {
        writeToken(COMMENT, &text);
    }
    
    void docdecl(const std::string& text) {
        writeToken(DOCDECL, &text);
    }
    
    void ignorableWhitespace(const std::string& text) {
        writeToken(IGNORABLE_WHITESPACE, &text);
    }
    
    int getDepth() const {
        return mTagCount;
    }
    
    std::string getName() const {
        return mTagNames[mTagCount - 1];
    }
};

const uint8_t BinaryXmlSerializer::PROTOCOL_MAGIC_VERSION_0[4] = { 0x41, 0x42, 0x58, 0x00 };

bool check_encoding(const std::string& xml_content) {
    // Look for XML declaration
    size_t decl_start = xml_content.find("<?xml");
    if (decl_start != std::string::npos) {
        size_t decl_end = xml_content.find("?>", decl_start);
        if (decl_end != std::string::npos) {
            std::string declaration = xml_content.substr(decl_start, decl_end - decl_start);
            
            // Look for encoding attribute
            size_t encoding_pos = declaration.find("encoding");
            if (encoding_pos != std::string::npos) {
                // Extract encoding value
                size_t quote_start = declaration.find_first_of("\"'", encoding_pos);
                if (quote_start != std::string::npos) {
                    char quote = declaration[quote_start];
                    size_t quote_end = declaration.find(quote, quote_start + 1);
                    if (quote_end != std::string::npos) {
                        std::string encoding = declaration.substr(quote_start + 1, quote_end - quote_start - 1);
                        std::transform(encoding.begin(), encoding.end(), encoding.begin(), ::tolower);
                        
                        if (encoding != "utf-8" && encoding != "utf8") {
                            show_warning("Non-UTF-8 encoding", "Found encoding: " + encoding + ", only UTF-8 is supported");
                            return false;
                        }
                    }
                }
            }
        }
    }
    return true;
}

class XmlToAbxConverter {
public:
    static void convert(const std::string& input_path, const std::string& output_path) {
        std::string xml_content;
        
        if (input_path == "-") {
            xml_content = read_from_stdin();
        } else {
            std::ifstream input_file(input_path);
            if (!input_file) {
                throw std::runtime_error("Could not open input file: " + input_path);
            }
            xml_content = std::string((std::istreambuf_iterator<char>(input_file)),
                                     std::istreambuf_iterator<char>());
        }

        check_encoding(xml_content);

        XmlParser parser;
        XmlNode root = parser.parse(xml_content);
        
        std::ofstream output_file(output_path, std::ios::binary);
        if (!output_file) {
            throw std::runtime_error("Could not open output file: " + output_path);
        }
        
        BinaryXmlSerializer serializer(output_file);
        serializer.startDocument();
        process_node(serializer, root);
        serializer.endDocument();
    }

private:
    static std::string read_from_stdin() {
        std::stringstream buffer;
        buffer << std::cin.rdbuf();
        return buffer.str();
    }
    
    static bool is_numeric(const std::string& str) {
        if (str.empty()) return false;
        size_t start = (str[0] == '-') ? 1 : 0;
        return std::all_of(str.begin() + start, str.end(), ::isdigit);
    }
    
    static bool is_float(const std::string& str) {
        if (str.empty()) return false;
        bool has_dot = false;
        size_t start = (str[0] == '-') ? 1 : 0;
        for (size_t i = start; i < str.length(); ++i) {
            if (str[i] == '.') {
                if (has_dot) return false;
                has_dot = true;
            } else if (!::isdigit(str[i])) {
                return false;
            }
        }
        return has_dot;
    }
    
    static bool is_boolean(const std::string& str) {
        return str == "true" || str == "false";
    }
    
    static void process_node(BinaryXmlSerializer& serializer, const XmlNode& node) {
    switch (node.type) {
        case XmlNode::Type::ELEMENT:
            // Special handling for document root - don't serialize it as an element
            if (node.name == "document") {
                // Process children directly without wrapping element
                for (const auto& child : node.children) {
                    process_node(serializer, child);
                }
            } else {
                serializer.startTag(node.name);
                
                // Process attributes with type detection
                for (const auto& attr : node.attributes) {
                    const std::string& name = attr.first;
                    const std::string& value = attr.second;
                    
                    // Try to detect the appropriate type for the attribute value
                    if (is_boolean(value)) {
                        serializer.attributeBoolean(name, value == "true");
                    } else if (is_numeric(value)) {
                        try {
                            int32_t int_val = std::stoi(value);
                            serializer.attributeInt(name, int_val);
                        } catch (...) {
                            try {
                                int64_t long_val = std::stoll(value);
                                serializer.attributeLong(name, long_val);
                            } catch (...) {
                                serializer.attribute(name, value);
                            }
                        }
                    } else if (is_float(value)) {
                        try {
                            float float_val = std::stof(value);
                            serializer.attributeFloat(name, float_val);
                        } catch (...) {
                            serializer.attribute(name, value);
                        }
                    } else {
                        serializer.attribute(name, value);
                    }
                }
                
                // Process children
                for (const auto& child : node.children) {
                    process_node(serializer, child);
                }
                
                serializer.endTag(node.name);
            }
            break;
            
        case XmlNode::Type::TEXT:
            // Always serialize text content even if it's just whitespace
            // The decoder might expect this for proper formatting ???
            serializer.text(node.text);
            break;
            
        case XmlNode::Type::CDATA:
            serializer.cdsect(node.text);
            break;
            
        case XmlNode::Type::COMMENT:
            serializer.comment(node.text);
            break;
            
        case XmlNode::Type::PROCESSING_INSTRUCTION:
            serializer.processingInstruction(node.text);
            break;
            
        case XmlNode::Type::DOCDECL:
            serializer.docdecl(node.text);
            break;
            
        case XmlNode::Type::ENTITY_REF:
            serializer.entityRef(node.text);
            break;
            
        case XmlNode::Type::IGNORABLE_WHITESPACE:
            // Serialize as ignorable whitespace but only for top level whitespace
            // For element content whitespace, it should be TEXT
            serializer.ignorableWhitespace(node.text);
            break;
    }
}
};

void print_usage() {
    std::cerr << "usage: xml2abx [-i] input [output]\n"
              << "\n"
              << "Converts between human-readable XML and Android Binary XML.\n\n"
              << "When invoked with the '-i' argument, the output of a successful conversion\n"
              << "will overwrite the original input file\n"
              << "\n"
              << "Use '-' as input to read from stdin. When reading from stdin,\n"
              << "output path must be specified.\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string input_path;
    std::string output_path;
    bool overwrite_input = false;
    
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

    if (input_path.empty()) {
        std::cerr << "Error: Input path is required\n";
        print_usage();
        return 1;
    }

    if (output_path.empty()) {
        if (overwrite_input) {
            if (input_path == "-") {
                std::cerr << "Error: Cannot overwrite stdin, output path is required\n";
                print_usage();
                return 1;
            }
            output_path = input_path;
        } else {
            std::cerr << "Error: Output path is required\n";
            print_usage();
            return 1;
        }
    }

    try {
        XmlToAbxConverter::convert(input_path, output_path);
        std::cout << "Successfully converted " << (input_path == "-" ? "stdin" : input_path) 
                  << " to " << output_path << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

