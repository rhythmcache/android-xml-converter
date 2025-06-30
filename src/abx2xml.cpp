#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

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

std::string encode_xml_entities(const std::string& text) {
  std::string result = text;
  size_t pos = 0;

  while ((pos = result.find("&", pos)) != std::string::npos) {
    result.replace(pos, 1, "&amp;");
    pos += 5;
  }
  pos = 0;
  while ((pos = result.find("<", pos)) != std::string::npos) {
    result.replace(pos, 1, "&lt;");
    pos += 4;
  }
  pos = 0;
  while ((pos = result.find(">", pos)) != std::string::npos) {
    result.replace(pos, 1, "&gt;");
    pos += 4;
  }
  pos = 0;
  while ((pos = result.find("\"", pos)) != std::string::npos) {
    result.replace(pos, 1, "&quot;");
    pos += 6;
  }
  pos = 0;
  while ((pos = result.find("'", pos)) != std::string::npos) {
    result.replace(pos, 1, "&apos;");
    pos += 6;
  }

  return result;
}

class FastDataInput {
 private:
  std::istringstream input_stream;
  std::vector<std::string> interned_strings;

 public:
  // Constructor for stdin input
  FastDataInput(std::istream& is) : input_stream(std::ios::binary) {
    // Read entire stream into string buffer
    std::ostringstream buffer;
    buffer << is.rdbuf();
    input_stream.str(buffer.str());
  }
  // Constructor for file-based input
  FastDataInput(std::ifstream& is) : input_stream(std::ios::binary) {
    // Read entire file into string buffer
    std::ostringstream buffer;
    buffer << is.rdbuf();
    input_stream.str(buffer.str());
  }

  // Constructor for memory-based input
  FastDataInput(const std::vector<char>& data)
      : input_stream(std::string(data.begin(), data.end()), std::ios::binary) {}

  uint8_t readByte() {
    uint8_t value;
    input_stream.read(reinterpret_cast<char*>(&value), 1);
    if (!input_stream) {
      throw std::runtime_error("Failed to read byte from stream");
    }
    return value;
  }

  uint16_t readShort() {
    uint16_t be_value;
    input_stream.read(reinterpret_cast<char*>(&be_value), 2);
    if (!input_stream) {
      throw std::runtime_error("Failed to read short from stream");
    }

    return ((be_value & 0xFF) << 8) | ((be_value >> 8) & 0xFF);
  }

  int32_t readInt() {
    int32_t be_value;
    input_stream.read(reinterpret_cast<char*>(&be_value), 4);
    if (!input_stream) {
      throw std::runtime_error("Failed to read int from stream");
    }

    return ((be_value & 0xFF) << 24) | (((be_value >> 8) & 0xFF) << 16) |
           (((be_value >> 16) & 0xFF) << 8) | ((be_value >> 24) & 0xFF);
  }

  int64_t readLong() {
    int64_t value = 0;
    for (int i = 7; i >= 0; i--) {
      uint8_t byte = readByte();
      value |= (static_cast<int64_t>(byte) << (i * 8));
    }
    return value;
  }

  float readFloat() {
    uint32_t int_value = static_cast<uint32_t>(readInt());
    return *reinterpret_cast<float*>(&int_value);
  }

  double readDouble() {
    uint64_t int_value = static_cast<uint64_t>(readLong());
    return *reinterpret_cast<double*>(&int_value);
  }

  std::string readUTF() {
    uint16_t length = readShort();
    std::vector<char> buffer(length);
    input_stream.read(buffer.data(), length);
    if (!input_stream) {
      throw std::runtime_error("Failed to read UTF string from stream");
    }
    return std::string(buffer.begin(), buffer.end());
  }

  std::string readInternedUTF() {
    uint16_t index = readShort();
    if (index == 0xFFFF) {
      std::string str = readUTF();
      interned_strings.push_back(str);
      return str;
    } else {
      if (index >= interned_strings.size()) {
        throw std::runtime_error("Invalid interned string index");
      }
      return interned_strings[index];
    }
  }

  std::vector<uint8_t> readBytes(uint16_t length) {
    std::vector<uint8_t> data(length);
    input_stream.read(reinterpret_cast<char*>(data.data()), length);
    if (!input_stream) {
      throw std::runtime_error("Failed to read bytes from stream");
    }
    return data;
  }

  bool eof() const { return input_stream.eof(); }

  std::streampos tellg() { return input_stream.tellg(); }

  void seekg(std::streampos pos) { input_stream.seekg(pos); }
};

class BinaryXmlDeserializer {
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
  std::unique_ptr<FastDataInput> mIn;
  std::ostream& mOut;

  std::string bytesToHex(const std::vector<uint8_t>& bytes) {
    std::stringstream ss;
    ss << std::hex << std::uppercase;
    for (uint8_t byte : bytes) {
      ss << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    return ss.str();
  }

  void processAttribute(uint8_t token) {
    uint8_t type = token & 0xF0;
    std::string name = mIn->readInternedUTF();

    mOut << " " << name << "=\"";

    switch (type) {
      case TYPE_STRING:
        mOut << encode_xml_entities(mIn->readUTF());
        break;
      case TYPE_STRING_INTERNED:
        mOut << encode_xml_entities(mIn->readInternedUTF());
        break;
      case TYPE_INT:
        mOut << mIn->readInt();
        break;
      case TYPE_INT_HEX:
        mOut << "0x" << std::hex << mIn->readInt() << std::dec;
        break;
      case TYPE_LONG:
        mOut << mIn->readLong();
        break;
      case TYPE_LONG_HEX:
        mOut << "0x" << std::hex << mIn->readLong() << std::dec;
        break;
      case TYPE_FLOAT:
        mOut << mIn->readFloat();
        break;
      case TYPE_DOUBLE:
        mOut << mIn->readDouble();
        break;
      case TYPE_BOOLEAN_TRUE:
        mOut << "true";
        break;
      case TYPE_BOOLEAN_FALSE:
        mOut << "false";
        break;
      case TYPE_BYTES_HEX: {
        uint16_t length = mIn->readShort();
        std::vector<uint8_t> bytes = mIn->readBytes(length);
        mOut << bytesToHex(bytes);
        break;
      }
      case TYPE_BYTES_BASE64: {
        uint16_t length = mIn->readShort();
        std::vector<uint8_t> bytes = mIn->readBytes(length);
        mOut << base64_encode(bytes.data(), bytes.size());
        break;
      }
      default:
        throw std::runtime_error("Unknown attribute type: " +
                                 std::to_string(type));
    }

    mOut << "\"";
  }

 public:
  // Constructor for stdin input
  BinaryXmlDeserializer(std::istream& is, std::ostream& os) : mOut(os) {
    mIn = std::make_unique<FastDataInput>(is);

    // Check magic header
    uint8_t magic[4];
    for (int i = 0; i < 4; ++i) {
      magic[i] = mIn->readByte();
    }

    if (memcmp(magic, PROTOCOL_MAGIC_VERSION_0, 4) != 0) {
      throw std::runtime_error(
          "Invalid ABX file format - magic header mismatch");
    }
  }

  // Constructor for file-based input
  BinaryXmlDeserializer(std::ifstream& is, std::ostream& os) : mOut(os) {
    mIn = std::make_unique<FastDataInput>(is);

    // Check magic header
    uint8_t magic[4];
    for (int i = 0; i < 4; ++i) {
      magic[i] = mIn->readByte();
    }

    if (memcmp(magic, PROTOCOL_MAGIC_VERSION_0, 4) != 0) {
      throw std::runtime_error(
          "Invalid ABX file format - magic header mismatch");
    }
  }

  // Constructor for memory-based input
  BinaryXmlDeserializer(const std::vector<char>& data, std::ostream& os)
      : mOut(os) {
    mIn = std::make_unique<FastDataInput>(data);

    // Check magic header
    uint8_t magic[4];
    for (int i = 0; i < 4; ++i) {
      magic[i] = mIn->readByte();
    }

    if (memcmp(magic, PROTOCOL_MAGIC_VERSION_0, 4) != 0) {
      throw std::runtime_error(
          "Invalid ABX file format - magic header mismatch");
    }
  }

  void deserialize() {
    mOut << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";

    while (!mIn->eof()) {
      try {
        uint8_t token = mIn->readByte();
        uint8_t command = token & 0x0F;
        uint8_t type = token & 0xF0;

        switch (command) {
          case START_DOCUMENT:

            break;

          case END_DOCUMENT:
            return;

          case START_TAG: {
            std::string tagName = mIn->readInternedUTF();
            mOut << "<" << tagName;

            while (true) {
              std::streampos pos = mIn->tellg();

              try {
                uint8_t nextToken = mIn->readByte();
                if ((nextToken & 0x0F) == ATTRIBUTE) {
                  processAttribute(nextToken);
                } else {
                  mIn->seekg(pos);
                  break;
                }
              } catch (...) {
                mIn->seekg(pos);
                break;
              }
            }

            mOut << ">";
            break;
          }

          case END_TAG: {
            std::string tagName = mIn->readInternedUTF();
            mOut << "</" << tagName << ">";
            break;
          }

          case TEXT:
            if (type == TYPE_STRING) {
              std::string text = mIn->readUTF();
              if (!text.empty()) {
                mOut << encode_xml_entities(text);
              }
            }
            break;

          case CDSECT:
            if (type == TYPE_STRING) {
              std::string text = mIn->readUTF();
              mOut << "<![CDATA[" << text << "]]>";
            }
            break;

          case COMMENT:
            if (type == TYPE_STRING) {
              std::string text = mIn->readUTF();
              mOut << "<!--" << text << "-->";
            }
            break;

          case PROCESSING_INSTRUCTION:
            if (type == TYPE_STRING) {
              std::string text = mIn->readUTF();
              mOut << "<?" << text << "?>";
            }
            break;

          case DOCDECL:
            if (type == TYPE_STRING) {
              std::string text = mIn->readUTF();
              mOut << "<!DOCTYPE " << text << ">";
            }
            break;

          case ENTITY_REF:
            if (type == TYPE_STRING) {
              std::string text = mIn->readUTF();
              mOut << "&" << text << ";";
            }
            break;

          case IGNORABLE_WHITESPACE:
            if (type == TYPE_STRING) {
              std::string text = mIn->readUTF();
              mOut << text;
            }
            break;

          default:
            std::cerr << "Warning: Unknown token: " << static_cast<int>(command)
                      << std::endl;
            break;
        }
      } catch (const std::exception& e) {
        // Try to continue parsing
        std::cerr << "Warning: Error parsing token: " << e.what() << std::endl;
        break;
      }
    }
  }
};

const uint8_t BinaryXmlDeserializer::PROTOCOL_MAGIC_VERSION_0[4] = {0x41, 0x42,
                                                                    0x58, 0x00};

class AbxToXmlConverter {
 public:
  static void convert(const std::string& input_path,
                      const std::string& output_path) {
    if (input_path == output_path) {
      convertInPlace(input_path);
      return;
    }

    std::ostream* output;
    std::ofstream output_file;

    if (output_path == "-") {
      output = &std::cout;
    } else {
      output_file.open(output_path);
      if (!output_file) {
        throw std::runtime_error("Could not open output file: " + output_path);
      }
      output = &output_file;
    }

    if (input_path == "-") {
      // Read from stdin
      BinaryXmlDeserializer deserializer(std::cin, *output);
      deserializer.deserialize();
    } else {
      // Read from file
      std::ifstream input_file(input_path, std::ios::binary);
      if (!input_file) {
        throw std::runtime_error("Could not open input file: " + input_path);
      }
      BinaryXmlDeserializer deserializer(input_file, *output);
      deserializer.deserialize();
    }
  }

  static void convertFromStdin(const std::string& output_path) {
    std::ostream* output;
    std::ofstream output_file;

    if (output_path == "-") {
      output = &std::cout;
    } else {
      output_file.open(output_path);
      if (!output_file) {
        throw std::runtime_error("Could not open output file: " + output_path);
      }
      output = &output_file;
    }

    BinaryXmlDeserializer deserializer(std::cin, *output);
    deserializer.deserialize();
  }

 private:
  static void convertInPlace(const std::string& file_path) {
    // Read entire file into memory first
    std::ifstream input_file(file_path, std::ios::binary);
    if (!input_file) {
      throw std::runtime_error("Could not open input file: " + file_path);
    }

    // Read file contents into memory
    std::vector<char> file_data;
    input_file.seekg(0, std::ios::end);
    file_data.resize(input_file.tellg());
    input_file.seekg(0, std::ios::beg);
    input_file.read(file_data.data(), file_data.size());
    input_file.close();

    // Convert from memory to string stream
    std::ostringstream output_stream;
    BinaryXmlDeserializer deserializer(file_data, output_stream);
    deserializer.deserialize();

    // Write the result back to the same file
    std::ofstream output_file(file_path);
    if (!output_file) {
      throw std::runtime_error("Could not open output file: " + file_path);
    }

    output_file << output_stream.str();
    output_file.close();
  }
};

void print_usage() {
  std::cerr << "usage: abx2xml [-i] input [output]\n\n"
            << "Converts between Android Binary XML and human-readable XML.\n\n"
            << "When invoked with the '-i' argument, the output of a "
               "successful conversion\n"
            << "will overwrite the original input file. Input can be '-' to "
               "use stdin, and\n"
            << "output can be '-' to use stdout.\n";
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    print_usage();
    return 1;
  }

  std::string input_path;
  std::string output_path;
  bool overwrite_input = false;
  bool use_stdin = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "-i") {
      overwrite_input = true;
    } else if (input_path.empty()) {
      input_path = arg;
      if (input_path == "-") {
        use_stdin = true;
      }
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

  if (overwrite_input && use_stdin) {
    std::cerr << "Error: Cannot use -i option with stdin input\n";
    print_usage();
    return 1;
  }

  if (output_path.empty()) {
    if (overwrite_input) {
      output_path = input_path;
    } else {
      output_path = "-";
    }
  }

  try {
    if (use_stdin) {
      AbxToXmlConverter::convertFromStdin(output_path);
    } else {
      AbxToXmlConverter::convert(input_path, output_path);
    }
    if (output_path != "-") {
      std::cout << "Successfully converted "
                << (use_stdin ? "stdin" : input_path) << " to " << output_path
                << std::endl;
    }
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }

  return 0;
}