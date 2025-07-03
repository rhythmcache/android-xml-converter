#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "pugixml.hpp"

void show_warning(const std::string& feature, const std::string& details = "") {
  std::cerr << "WARNING: " << feature << " is not supported and might* be lost."
            << std::endl;
  if (!details.empty()) {
    std::cerr << "  " << details << std::endl;
  }
}

std::vector<uint8_t> base64_decode(const std::string& data) {
  const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<uint8_t> result;
  int val = 0, valb = -8;
  for (char c : data) {
    if (c == '=') break;
    auto pos = chars.find(c);
    if (pos == std::string::npos) continue;
    val = (val << 6) + pos;
    valb += 6;
    if (valb >= 0) {
      result.push_back((val >> valb) & 0xFF);
      valb -= 8;
    }
  }
  return result;
}

std::vector<uint8_t> hex_decode(const std::string& hex) {
  std::vector<uint8_t> result;
  for (size_t i = 0; i < hex.length(); i += 2) {
    std::string byte_str = hex.substr(i, 2);
    uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
    result.push_back(byte);
  }
  return result;
}

class FastDataOutput {
 private:
  std::ostream& output_stream;
  std::unordered_map<std::string, uint16_t> string_pool;
  std::vector<std::string> interned_strings;

 public:
  static const uint16_t MAX_UNSIGNED_SHORT = 65535;

  FastDataOutput(std::ostream& os) : output_stream(os) {}

  void writeByte(uint8_t value) {
    output_stream.write(reinterpret_cast<const char*>(&value), 1);
  }

  void writeShort(uint16_t value) {
    uint16_t be_value = ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
    output_stream.write(reinterpret_cast<const char*>(&be_value), 2);
  }

  void writeInt(int32_t value) {
    int32_t be_value = ((value & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) |
                       (((value >> 16) & 0xFF) << 8) | ((value >> 24) & 0xFF);
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
      show_warning("String length exceeds 65,535 bytes",
                   "String will be truncated: " + str.substr(0, 50) + "...");
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
    output_stream.write(reinterpret_cast<const char*>(data.data()),
                        data.size());
  }

  void flush() { output_stream.flush(); }
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
  BinaryXmlSerializer(std::ostream& os) {
    mOut = std::make_unique<FastDataOutput>(os);
    os.write(reinterpret_cast<const char*>(PROTOCOL_MAGIC_VERSION_0), 4);
    mTagCount = 0;
    mTagNames.reserve(8);
  }

  void startDocument() { mOut->writeByte(START_DOCUMENT | TYPE_NULL); }

  void endDocument() {
    mOut->writeByte(END_DOCUMENT | TYPE_NULL);
    mOut->flush();
  }

  void startTag(const std::string& name) {
    if (mTagCount == static_cast<int>(mTagNames.size())) {
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

  void attributeBytesHex(const std::string& name,
                         const std::vector<uint8_t>& value) {
    if (value.size() > FastDataOutput::MAX_UNSIGNED_SHORT) {
      throw std::runtime_error(
          "attributeBytesHex: input size (" + std::to_string(value.size()) +
          ") exceeds maximum allowed size (" +
          std::to_string(FastDataOutput::MAX_UNSIGNED_SHORT) + ")");
    }
    mOut->writeByte(ATTRIBUTE | TYPE_BYTES_HEX);
    mOut->writeInternedUTF(name);
    mOut->writeShort(static_cast<uint16_t>(value.size()));
    mOut->write(value);
  }

  void attributeBytesBase64(const std::string& name,
                            const std::vector<uint8_t>& value) {
    if (value.size() > FastDataOutput::MAX_UNSIGNED_SHORT) {
      throw std::runtime_error(
          "attributeBytesBase64: input size (" + std::to_string(value.size()) +
          ") exceeds maximum allowed size (" +
          std::to_string(FastDataOutput::MAX_UNSIGNED_SHORT) + ")");
    }
    mOut->writeByte(ATTRIBUTE | TYPE_BYTES_BASE64);
    mOut->writeInternedUTF(name);
    mOut->writeShort(static_cast<uint16_t>(value.size()));
    mOut->write(value);
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

  void text(const std::string& text) { writeToken(TEXT, &text); }

  void cdsect(const std::string& text) { writeToken(CDSECT, &text); }

  void comment(const std::string& text) { writeToken(COMMENT, &text); }

  void processingInstruction(const std::string& target,
                             const std::string& data = "") {
    // For processing instructions, we need to handle target and data separately
    // The Android format expects the full PI content as a single string
    std::string full_pi;
    if (!data.empty()) {
      full_pi = target + " " + data;
    } else {
      full_pi = target;
    }
    writeToken(PROCESSING_INSTRUCTION, &full_pi);
  }

  void docdecl(const std::string& text) { writeToken(DOCDECL, &text); }

  void ignorableWhitespace(const std::string& text) {
    writeToken(IGNORABLE_WHITESPACE, &text);
  }

  void entityRef(const std::string& text) { writeToken(ENTITY_REF, &text); }
};

const uint8_t BinaryXmlSerializer::PROTOCOL_MAGIC_VERSION_0[4] = {0x41, 0x42,
                                                                  0x58, 0x00};

class XmlToAbxConverter {
 private:
  static bool is_numeric(const std::string& str) {
    if (str.empty()) return false;
    size_t start = (str[0] == '-') ? 1 : 0;
    return std::all_of(str.begin() + start, str.end(), ::isdigit);
  }

  static bool is_hex_number(const std::string& str) {
    if (str.length() < 3) return false;
    if (str.substr(0, 2) != "0x" && str.substr(0, 2) != "0X") return false;
    return std::all_of(str.begin() + 2, str.end(), [](char c) {
      return std::isdigit(c) ||
             (std::tolower(c) >= 'a' && std::tolower(c) <= 'f');
    });
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

  static bool is_double(const std::string& str) {
    // Check for scientific notation or very long float values
    if (str.find('e') != std::string::npos ||
        str.find('E') != std::string::npos) {
      return true;
    }
    if (is_float(str) &&
        str.length() > 10) {  // Arbitrary threshold for double precision
      return true;
    }
    return false;
  }

  static bool is_boolean(const std::string& str) {
    return str == "true" || str == "false";
  }

  static bool is_base64(const std::string& str) {
    if (str.empty() || str.length() % 4 != 0) return false;
    return std::all_of(str.begin(), str.end(),
                       [](char c) {
                         return std::isalnum(c) || c == '+' || c == '/' ||
                                c == '=';
                       }) &&
           str.find_first_not_of(
               "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+"
               "/=") == std::string::npos;
  }

  static bool is_hex_string(const std::string& str) {
    if (str.length() % 2 != 0) return false;
    return std::all_of(str.begin(), str.end(), [](char c) {
      return std::isdigit(c) ||
             (std::tolower(c) >= 'a' && std::tolower(c) <= 'f');
    });
  }

  // function to check if a string is only whitespace
  static bool is_whitespace_only(const std::string& str) {
    return std::all_of(str.begin(), str.end(), [](char c) {
      return std::isspace(static_cast<unsigned char>(c));
    });
  }

  static void process_node(BinaryXmlSerializer& serializer,
                           const pugi::xml_node& node, bool collapse_whitespaces) {
    pugi::xml_node_type node_type = node.type();

    switch (node_type) {
      case pugi::node_element: {
        std::string name = node.name();

        if (name.find(':') != std::string::npos) {
          show_warning("Namespaces and prefixes",
                       "Found prefixed element: " + name);
        }

        serializer.startTag(name);
        for (const auto& attr : node.attributes()) {
          std::string attr_name = attr.name();
          std::string attr_value = attr.value();

          if (attr_name.find("xmlns") == 0 ||
              attr_name.find(':') != std::string::npos) {
            show_warning("Namespaces and prefixes",
                         "Found namespace declaration or prefixed attribute: " +
                             attr_name);
          }

          if (is_boolean(attr_value)) {
            serializer.attributeBoolean(attr_name, attr_value == "true");
          } else if (is_hex_number(attr_value)) {
            try {
              if (attr_value.length() <= 10) {
                int32_t int_val = std::stoi(attr_value, nullptr, 16);
                serializer.attributeIntHex(attr_name, int_val);
              } else {
                int64_t long_val = std::stoll(attr_value, nullptr, 16);
                serializer.attributeLongHex(attr_name, long_val);
              }
            } catch (...) {
              serializer.attribute(attr_name, attr_value);
            }
          } else if (is_numeric(attr_value)) {
            try {
              int32_t int_val = std::stoi(attr_value);
              serializer.attributeInt(attr_name, int_val);
            } catch (...) {
              try {
                int64_t long_val = std::stoll(attr_value);
                serializer.attributeLong(attr_name, long_val);
              } catch (...) {
                serializer.attribute(attr_name, attr_value);
              }
            }
          } else if (is_double(attr_value)) {
            try {
              double double_val = std::stod(attr_value);
              serializer.attributeDouble(attr_name, double_val);
            } catch (...) {
              serializer.attribute(attr_name, attr_value);
            }
          } else if (is_float(attr_value)) {
            try {
              float float_val = std::stof(attr_value);
              serializer.attributeFloat(attr_name, float_val);
            } catch (...) {
              serializer.attribute(attr_name, attr_value);
            }
          } else if (is_base64(attr_value) && attr_value.length() > 8) {
            try {
              std::vector<uint8_t> decoded = base64_decode(attr_value);
              serializer.attributeBytesBase64(attr_name, decoded);
            } catch (...) {
              serializer.attribute(attr_name, attr_value);
            }
          } else if (is_hex_string(attr_value) && attr_value.length() > 2) {
            try {
              std::vector<uint8_t> decoded = hex_decode(attr_value);
              serializer.attributeBytesHex(attr_name, decoded);
            } catch (...) {
              serializer.attribute(attr_name, attr_value);
            }
          } else {
            if (attr_value.length() < 50 &&
                (attr_value.find(' ') == std::string::npos ||
                 attr_value == "true" || attr_value == "false" ||
                 attr_value.find('.') != std::string::npos)) {
              serializer.attributeInterned(attr_name, attr_value);
            } else {
              serializer.attribute(attr_name, attr_value);
            }
          }
        }

        for (const auto& child : node.children()) {
          process_node(serializer, child, collapse_whitespaces);
        }

        serializer.endTag(name);
        break;
      }

      case pugi::node_pcdata: {
        std::string text_content = node.value();
        if (is_whitespace_only(text_content)) {
          // Only write whitespace if not collapsing whitespaces
          if (!collapse_whitespaces) {
            serializer.ignorableWhitespace(text_content);
          }
        } else {
          serializer.text(text_content);
        }
        break;
      }

      case pugi::node_cdata:
        serializer.cdsect(node.value());
        break;

      case pugi::node_comment:
        serializer.comment(node.value());
        break;

      case pugi::node_pi: {
        std::string target = node.name();
        std::string data = node.value();
        serializer.processingInstruction(target, data);
        break;
      }

      case pugi::node_doctype:
        serializer.docdecl(node.value());
        break;

      default:
        break;
    }
  }

 public:
  static void convert(const std::string& input_path,
                      const std::string& output_path, bool collapse_whitespaces = false) {
    pugi::xml_document doc;
    pugi::xml_parse_result result;

    // Enable whitespace parsing - this is crucial for proper formatting
    unsigned int parse_flags = pugi::parse_default | pugi::parse_pi |
                               pugi::parse_comments | pugi::parse_declaration |
                               pugi::parse_doctype | pugi::parse_ws_pcdata;

    if (input_path == "-") {
      std::stringstream buffer;
      buffer << std::cin.rdbuf();
      std::string xml_content = buffer.str();
      result = doc.load_string(xml_content.c_str(), parse_flags);
    } else {
      result = doc.load_file(input_path.c_str(), parse_flags);
    }

    if (!result) {
      throw std::runtime_error(
          "XML parsing failed: " + std::string(result.description()) +
          " at offset " + std::to_string(result.offset));
    }

    if (doc.first_child().type() == pugi::node_declaration) {
      auto decl = doc.first_child();
      auto encoding_attr = decl.attribute("encoding");
      if (encoding_attr) {
        std::string encoding = encoding_attr.value();
        std::transform(encoding.begin(), encoding.end(), encoding.begin(),
                       ::tolower);
        if (encoding != "utf-8" && encoding != "utf8") {
          show_warning("Non-UTF-8 encoding", "Found encoding: " + encoding +
                                                 ", only UTF-8 is supported");
        }
      }
    }

    if (output_path == "-") {
      BinaryXmlSerializer serializer(std::cout);
      serializer.startDocument();

      for (const auto& child : doc.children()) {
        process_node(serializer, child, collapse_whitespaces);
      }

      serializer.endDocument();
    } else {
      std::ofstream output_file(output_path, std::ios::binary);
      if (!output_file) {
        throw std::runtime_error("Could not open output file: " + output_path);
      }

      BinaryXmlSerializer serializer(output_file);
      serializer.startDocument();

      for (const auto& child : doc.children()) {
        process_node(serializer, child, collapse_whitespaces);
      }

      serializer.endDocument();
    }
  }
};

void print_usage() {
  std::cerr << "usage: xml2abx [-i] [--collapse-whitespaces] input [output]\n\n"
            << "Converts between human-readable XML and Android Binary XML.\n\n"
            << "When invoked with the '-i' argument, the output of a "
               "successful conversion\n"
            << "will overwrite the original input file. Input can be '-' to "
               "use stdin, and\n"
            << "output can be '-' to use stdout.\n\n"
            << "Options:\n"
            << "  -i                     Overwrite input file with output\n"
            << "  --collapse-whitespaces Skip whitespace-only text nodes\n";
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  std::string input_path;
  std::string output_path;
  bool overwrite_input = false;
  bool collapse_whitespaces = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-i") {
      overwrite_input = true;
    } else if (arg == "--collapse-whitespaces") {
      collapse_whitespaces = true;
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
    XmlToAbxConverter::convert(input_path, output_path, collapse_whitespaces);
    std::cout << "Successfully converted "
              << (input_path == "-" ? "stdin" : input_path) << " to "
              << output_path << std::endl;
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}
