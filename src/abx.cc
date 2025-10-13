#include "abx.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace libabx {

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

std::string base64_encode(const unsigned char *data, size_t len) {
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

std::vector<uint8_t> base64_decode(const std::string &data) {
  const std::string chars =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::vector<uint8_t> result;
  uint32_t val = 0;
  int valb = -8;
  for (char c : data) {
    if (c == '=')
      break;
    auto pos = chars.find(c);
    if (pos == std::string::npos)
      continue;
    val = (val << 6) + static_cast<uint32_t>(pos);
    valb += 6;
    if (valb >= 0) {
      result.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
      valb -= 8;
    }
  }
  return result;
}

std::vector<uint8_t> hex_decode(const std::string &hex) {
  if (hex.length() % 2 != 0) {
    throw std::runtime_error("Invalid hex string: odd length");
  }

  std::vector<uint8_t> result;
  result.reserve(hex.length() / 2);

  for (size_t i = 0; i < hex.length(); i += 2) {
    std::string byte_str = hex.substr(i, 2);
    uint8_t byte = static_cast<uint8_t>(std::stoul(byte_str, nullptr, 16));
    result.push_back(byte);
  }
  return result;
}

// FIXED: Single-pass implementation for better performance
std::string encode_xml_entities(const std::string &text) {
  std::string result;
  result.reserve(static_cast<size_t>(text.size() * 1.2));

  for (char c : text) {
    switch (c) {
    case '&':
      result += "&amp;";
      break;
    case '<':
      result += "&lt;";
      break;
    case '>':
      result += "&gt;";
      break;
    case '"':
      result += "&quot;";
      break;
    case '\'':
      result += "&apos;";
      break;
    default:
      result += c;
      break;
    }
  }

  return result;
}

// ============================================================================
// STREAM WRAPPERS
// ============================================================================

FastDataInput::FastDataInput(std::istream &is) : input_stream(is) {}

uint8_t FastDataInput::readByte() {
  uint8_t value;
  input_stream.read(reinterpret_cast<char *>(&value), 1);
  if (!input_stream) {
    throw std::runtime_error("Failed to read byte from stream");
  }
  return value;
}

uint16_t FastDataInput::readShort() {
  uint16_t be_value;
  input_stream.read(reinterpret_cast<char *>(&be_value), 2);
  if (!input_stream) {
    throw std::runtime_error("Failed to read short from stream");
  }
  return ((be_value & 0xFF) << 8) | ((be_value >> 8) & 0xFF);
}

int32_t FastDataInput::readInt() {
  int32_t be_value;
  input_stream.read(reinterpret_cast<char *>(&be_value), 4);
  if (!input_stream) {
    throw std::runtime_error("Failed to read int from stream");
  }
  return ((be_value & 0xFF) << 24) | (((be_value >> 8) & 0xFF) << 16) |
         (((be_value >> 16) & 0xFF) << 8) | ((be_value >> 24) & 0xFF);
}

int64_t FastDataInput::readLong() {
  int64_t value = 0;
  for (int i = 7; i >= 0; i--) {
    uint8_t byte = readByte();
    value |= (static_cast<int64_t>(byte) << (i * 8));
  }
  return value;
}

float FastDataInput::readFloat() {
  uint32_t int_value = static_cast<uint32_t>(readInt());
  float result;
  std::memcpy(&result, &int_value, sizeof(float));
  return result;
}

double FastDataInput::readDouble() {
  uint64_t int_value = static_cast<uint64_t>(readLong());
  double result;
  std::memcpy(&result, &int_value, sizeof(double));
  return result;
}

std::string FastDataInput::readUTF() {
  uint16_t length = readShort();
  std::string result(length, '\0');
  input_stream.read(&result[0], length);
  if (!input_stream) {
    throw std::runtime_error("Failed to read UTF string from stream");
  }
  return result;
}

std::string FastDataInput::readInternedUTF() {
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

std::vector<uint8_t> FastDataInput::readBytes(uint16_t length) {
  std::vector<uint8_t> data(length);
  input_stream.read(reinterpret_cast<char *>(data.data()), length);
  if (!input_stream) {
    throw std::runtime_error("Failed to read bytes from stream");
  }
  return data;
}

bool FastDataInput::eof() const {
  return input_stream.eof() || input_stream.peek() == EOF;
}

std::streampos FastDataInput::tellg() { return input_stream.tellg(); }

void FastDataInput::seekg(std::streampos pos) { input_stream.seekg(pos); }

// FastDataOutput implementation
FastDataOutput::FastDataOutput(std::ostream &os) : output_stream(os) {}

void FastDataOutput::writeByte(uint8_t value) {
  output_stream.write(reinterpret_cast<const char *>(&value), 1);
}

void FastDataOutput::writeShort(uint16_t value) {
  uint16_t be_value = ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
  output_stream.write(reinterpret_cast<const char *>(&be_value), 2);
}

void FastDataOutput::writeInt(int32_t value) {
  int32_t be_value = ((value & 0xFF) << 24) | (((value >> 8) & 0xFF) << 16) |
                     (((value >> 16) & 0xFF) << 8) | ((value >> 24) & 0xFF);
  output_stream.write(reinterpret_cast<const char *>(&be_value), 4);
}

void FastDataOutput::writeLong(int64_t value) {
  for (int i = 7; i >= 0; i--) {
    uint8_t byte = (value >> (i * 8)) & 0xFF;
    writeByte(byte);
  }
}

void FastDataOutput::writeFloat(float value) {
  uint32_t int_value;
  std::memcpy(&int_value, &value, sizeof(float));
  writeInt(int_value);
}

void FastDataOutput::writeDouble(double value) {
  uint64_t int_value;
  std::memcpy(&int_value, &value, sizeof(double));
  writeLong(int_value);
}

void FastDataOutput::writeUTF(const std::string &str) {
  if (str.length() > MAX_UNSIGNED_SHORT) {
    throw std::runtime_error("String length exceeds maximum allowed size");
  }
  writeShort(static_cast<uint16_t>(str.length()));
  output_stream.write(str.data(), str.length());
}

void FastDataOutput::writeInternedUTF(const std::string &str) {
  auto it = string_pool.find(str);
  if (it != string_pool.end()) {
    writeShort(it->second);
  } else {
    if (interned_strings.size() >= MAX_UNSIGNED_SHORT) {
      throw std::runtime_error(
          "String pool overflow - too many unique strings");
    }

    writeShort(0xFFFF);
    writeUTF(str);
    string_pool[str] = static_cast<uint16_t>(interned_strings.size());
    interned_strings.push_back(str);
  }
}

void FastDataOutput::write(const std::vector<uint8_t> &data) {
  output_stream.write(reinterpret_cast<const char *>(data.data()), data.size());
}

void FastDataOutput::flush() { output_stream.flush(); }

// ============================================================================
// DESERIALIZER
// ============================================================================

const uint8_t BinaryXmlDeserializer::PROTOCOL_MAGIC_VERSION_0[4] = {0x41, 0x42,
                                                                    0x58, 0x00};

std::string
BinaryXmlDeserializer::bytesToHex(const std::vector<uint8_t> &bytes) {
  std::stringstream ss;
  ss << std::hex << std::nouppercase;
  for (uint8_t byte : bytes) {
    ss << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
  }
  return ss.str();
}

void BinaryXmlDeserializer::processAttribute(uint8_t token) {
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
  case TYPE_INT_HEX: {
    int32_t value = mIn->readInt();
    if (value == -1) {
      mOut << value;
    } else {
      mOut << std::hex << std::nouppercase << static_cast<uint32_t>(value)
           << std::dec;
    }
    break;
  }
  case TYPE_LONG:
    mOut << mIn->readLong();
    break;
  case TYPE_LONG_HEX: {
    int64_t value = mIn->readLong();
    if (value == -1LL) {
      mOut << value;
    } else {
      mOut << std::hex << std::nouppercase << static_cast<uint64_t>(value)
           << std::dec;
    }
    break;
  }
  case TYPE_FLOAT: {
    float value = mIn->readFloat();
    if (value == std::floor(value) && std::isfinite(value)) {
      mOut << static_cast<int>(value) << ".0";
    } else {
      mOut << value;
    }
    break;
  }
  case TYPE_DOUBLE: {
    double value = mIn->readDouble();
    if (value == std::floor(value) && std::isfinite(value)) {
      mOut << static_cast<long long>(value) << ".0";
    } else {
      mOut << value;
    }
    break;
  }
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
    throw std::runtime_error("Unknown attribute type: " + std::to_string(type));
  }

  mOut << "\"";
}

BinaryXmlDeserializer::BinaryXmlDeserializer(std::istream &is, std::ostream &os)
    : mOut(os) {
  mIn = std::make_unique<FastDataInput>(is);

  uint8_t magic[4];
  for (int i = 0; i < 4; ++i) {
    magic[i] = mIn->readByte();
  }

  if (memcmp(magic, PROTOCOL_MAGIC_VERSION_0, 4) != 0) {
    throw std::runtime_error("Invalid ABX file format - magic header mismatch");
  }
}

void BinaryXmlDeserializer::deserialize() {
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
          } catch (const std::runtime_error &) {
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
          mOut << "<![CDATA[" << mIn->readUTF() << "]]>";
        }
        break;

      case COMMENT:
        if (type == TYPE_STRING) {
          mOut << "<!--" << mIn->readUTF() << "-->";
        }
        break;

      case PROCESSING_INSTRUCTION:
        if (type == TYPE_STRING) {
          mOut << "<?" << mIn->readUTF() << "?>";
        }
        break;

      case DOCDECL:
        if (type == TYPE_STRING) {
          mOut << "<!DOCTYPE " << mIn->readUTF() << ">";
        }
        break;

      case ENTITY_REF:
        if (type == TYPE_STRING) {
          mOut << "&" << mIn->readUTF() << ";";
        }
        break;

      case IGNORABLE_WHITESPACE:
        if (type == TYPE_STRING) {
          mOut << mIn->readUTF();
        }
        break;

      default:
        break;
      }
    } catch (const std::runtime_error &) {
      if (mIn->eof()) {
        break;
      }
      throw;
    }
  }
}

// ============================================================================
// SERIALIZER
// ============================================================================

const uint8_t BinaryXmlSerializer::PROTOCOL_MAGIC_VERSION_0[4] = {0x41, 0x42,
                                                                  0x58, 0x00};

void BinaryXmlSerializer::writeToken(uint8_t token, const std::string *text) {
  if (text != nullptr) {
    mOut->writeByte(token | TYPE_STRING);
    mOut->writeUTF(*text);
  } else {
    mOut->writeByte(token | TYPE_NULL);
  }
}

BinaryXmlSerializer::BinaryXmlSerializer(std::ostream &os) {
  mOut = std::make_unique<FastDataOutput>(os);
  os.write(reinterpret_cast<const char *>(PROTOCOL_MAGIC_VERSION_0), 4);
  mTagCount = 0;
  mTagNames.reserve(8);
}

void BinaryXmlSerializer::startDocument() {
  mOut->writeByte(START_DOCUMENT | TYPE_NULL);
}

void BinaryXmlSerializer::endDocument() {
  mOut->writeByte(END_DOCUMENT | TYPE_NULL);
  mOut->flush();
}

void BinaryXmlSerializer::startTag(const std::string &name) {
  if (mTagCount == static_cast<int>(mTagNames.size())) {
    mTagNames.resize(mTagCount + std::max(1, mTagCount / 2));
  }
  mTagNames[mTagCount++] = name;
  mOut->writeByte(START_TAG | TYPE_STRING_INTERNED);
  mOut->writeInternedUTF(name);
}

// FIXED: Add bounds checking for tag count
void BinaryXmlSerializer::endTag(const std::string &name) {
  if (mTagCount <= 0) {
    throw std::runtime_error("endTag() called without matching startTag()");
  }

  if (mTagNames[mTagCount - 1] != name) {
    throw std::runtime_error("Mismatched tags: expected '" +
                             mTagNames[mTagCount - 1] + "', got '" + name +
                             "'");
  }

  mTagCount--;
  mOut->writeByte(END_TAG | TYPE_STRING_INTERNED);
  mOut->writeInternedUTF(name);
}

void BinaryXmlSerializer::attribute(const std::string &name,
                                    const std::string &value) {
  mOut->writeByte(ATTRIBUTE | TYPE_STRING);
  mOut->writeInternedUTF(name);
  mOut->writeUTF(value);
}

void BinaryXmlSerializer::attributeInterned(const std::string &name,
                                            const std::string &value) {
  mOut->writeByte(ATTRIBUTE | TYPE_STRING_INTERNED);
  mOut->writeInternedUTF(name);
  mOut->writeInternedUTF(value);
}

void BinaryXmlSerializer::attributeBytesHex(const std::string &name,
                                            const std::vector<uint8_t> &value) {
  if (value.size() > FastDataOutput::MAX_UNSIGNED_SHORT) {
    throw std::runtime_error("attributeBytesHex: input size exceeds maximum");
  }
  mOut->writeByte(ATTRIBUTE | TYPE_BYTES_HEX);
  mOut->writeInternedUTF(name);
  mOut->writeShort(static_cast<uint16_t>(value.size()));
  mOut->write(value);
}

void BinaryXmlSerializer::attributeBytesBase64(
    const std::string &name, const std::vector<uint8_t> &value) {
  if (value.size() > FastDataOutput::MAX_UNSIGNED_SHORT) {
    throw std::runtime_error(
        "attributeBytesBase64: input size exceeds maximum");
  }
  mOut->writeByte(ATTRIBUTE | TYPE_BYTES_BASE64);
  mOut->writeInternedUTF(name);
  mOut->writeShort(static_cast<uint16_t>(value.size()));
  mOut->write(value);
}

void BinaryXmlSerializer::attributeInt(const std::string &name, int32_t value) {
  mOut->writeByte(ATTRIBUTE | TYPE_INT);
  mOut->writeInternedUTF(name);
  mOut->writeInt(value);
}

void BinaryXmlSerializer::attributeIntHex(const std::string &name,
                                          int32_t value) {
  mOut->writeByte(ATTRIBUTE | TYPE_INT_HEX);
  mOut->writeInternedUTF(name);
  mOut->writeInt(value);
}

void BinaryXmlSerializer::attributeLong(const std::string &name,
                                        int64_t value) {
  mOut->writeByte(ATTRIBUTE | TYPE_LONG);
  mOut->writeInternedUTF(name);
  mOut->writeLong(value);
}

void BinaryXmlSerializer::attributeLongHex(const std::string &name,
                                           int64_t value) {
  mOut->writeByte(ATTRIBUTE | TYPE_LONG_HEX);
  mOut->writeInternedUTF(name);
  mOut->writeLong(value);
}

void BinaryXmlSerializer::attributeFloat(const std::string &name, float value) {
  mOut->writeByte(ATTRIBUTE | TYPE_FLOAT);
  mOut->writeInternedUTF(name);
  mOut->writeFloat(value);
}

void BinaryXmlSerializer::attributeDouble(const std::string &name,
                                          double value) {
  mOut->writeByte(ATTRIBUTE | TYPE_DOUBLE);
  mOut->writeInternedUTF(name);
  mOut->writeDouble(value);
}

void BinaryXmlSerializer::attributeBoolean(const std::string &name,
                                           bool value) {
  if (value) {
    mOut->writeByte(ATTRIBUTE | TYPE_BOOLEAN_TRUE);
  } else {
    mOut->writeByte(ATTRIBUTE | TYPE_BOOLEAN_FALSE);
  }
  mOut->writeInternedUTF(name);
}

void BinaryXmlSerializer::text(const std::string &text) {
  writeToken(TEXT, &text);
}

void BinaryXmlSerializer::cdsect(const std::string &text) {
  writeToken(CDSECT, &text);
}

void BinaryXmlSerializer::comment(const std::string &text) {
  writeToken(COMMENT, &text);
}

void BinaryXmlSerializer::processingInstruction(const std::string &target,
                                                const std::string &data) {
  std::string full_pi;
  if (!data.empty()) {
    full_pi = target + " " + data;
  } else {
    full_pi = target;
  }
  writeToken(PROCESSING_INSTRUCTION, &full_pi);
}

void BinaryXmlSerializer::docdecl(const std::string &text) {
  writeToken(DOCDECL, &text);
}

void BinaryXmlSerializer::ignorableWhitespace(const std::string &text) {
  writeToken(IGNORABLE_WHITESPACE, &text);
}

void BinaryXmlSerializer::entityRef(const std::string &text) {
  writeToken(ENTITY_REF, &text);
}

// ============================================================================
// XML PARSING LAYER - Type inference and conversion logic
// ============================================================================

namespace {

bool is_numeric(const std::string &str) {
  if (str.empty())
    return false;
  size_t start = (str[0] == '-') ? 1 : 0;
  return std::all_of(str.begin() + start, str.end(), ::isdigit);
}

bool is_hex_number(const std::string &str) {
  if (str.length() < 3)
    return false;
  if (str.substr(0, 2) != "0x" && str.substr(0, 2) != "0X")
    return false;
  return std::all_of(str.begin() + 2, str.end(), [](char c) {
    return std::isdigit(c) ||
           (std::tolower(c) >= 'a' && std::tolower(c) <= 'f');
  });
}

bool is_float(const std::string &str) {
  if (str.empty())
    return false;
  bool has_dot = false;
  size_t start = (str[0] == '-') ? 1 : 0;
  for (size_t i = start; i < str.length(); ++i) {
    if (str[i] == '.') {
      if (has_dot)
        return false;
      has_dot = true;
    } else if (!::isdigit(str[i])) {
      return false;
    }
  }
  return has_dot;
}

bool is_double(const std::string &str) {
  if (str.find('e') != std::string::npos ||
      str.find('E') != std::string::npos) {
    return true;
  }
  if (is_float(str) && str.length() > 10) {
    return true;
  }
  return false;
}

bool is_boolean(const std::string &str) {
  return str == "true" || str == "false";
}

bool is_base64(const std::string &str) {
  if (str.empty() || str.length() % 4 != 0)
    return false;
  return std::all_of(str.begin(), str.end(),
                     [](char c) {
                       return std::isalnum(c) || c == '+' || c == '/' ||
                              c == '=';
                     }) &&
         str.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstu"
                               "vwxyz0123456789+/=") == std::string::npos;
}

bool is_hex_string(const std::string &str) {
  if (str.length() % 2 != 0)
    return false;
  return std::all_of(str.begin(), str.end(), [](char c) {
    return std::isdigit(c) ||
           (std::tolower(c) >= 'a' && std::tolower(c) <= 'f');
  });
}

bool is_whitespace_only(const std::string &str) {
  return std::all_of(str.begin(), str.end(), [](char c) {
    return std::isspace(static_cast<unsigned char>(c));
  });
}

void process_xml_node_internal(BinaryXmlSerializer &serializer,
                               const pugi::xml_node &node,
                               const XmlToAbxOptions &options) {
  pugi::xml_node_type node_type = node.type();

  switch (node_type) {
  case pugi::node_element: {
    std::string name = node.name();

    if (options.warning_callback && name.find(':') != std::string::npos) {
      options.warning_callback("Namespaces and prefixes",
                               "Found prefixed element: " + name);
    }

    serializer.startTag(name);

    for (const auto &attr : node.attributes()) {
      std::string attr_name = attr.name();
      std::string attr_value = attr.value();

      if (options.warning_callback &&
          (attr_name.find("xmlns") == 0 ||
           attr_name.find(':') != std::string::npos)) {
        options.warning_callback(
            "Namespaces and prefixes",
            "Found namespace declaration or prefixed attribute: " + attr_name);
      }

      // Type inference for attributes
      if (is_boolean(attr_value)) {
        // "true" or "false" only
        serializer.attributeBoolean(attr_name, attr_value == "true");

      } else if (is_hex_number(attr_value)) {
        // Only 0x prefixed hex like 0xFF
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

      } else if (is_numeric(attr_value) && attr_value.length() < 15) {
        // simple integers only, reasonable length
        // excludes things like certificate keys
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

      } else if (is_float(attr_value) && !is_hex_string(attr_value) &&
                 attr_value.length() < 20) {
        // only real floats like 3.14 not hex strings
        try {
          float float_val = std::stof(attr_value);
          serializer.attributeFloat(attr_name, float_val);
        } catch (...) {
          serializer.attribute(attr_name, attr_value);
        }

      } else {
        // everything else stays as string the safe default
        // this includes uuids certs long hex strings etc.
        if (attr_value.length() < 50 &&
            attr_value.find(' ') == std::string::npos &&
            attr_value.find('-') == std::string::npos) {
          // short simple strings can be interned
          serializer.attributeInterned(attr_name, attr_value);
        } else {
          // everything else as regular string
          serializer.attribute(attr_name, attr_value);
        }
      }
    }

    for (const auto &child : node.children()) {
      process_xml_node_internal(serializer, child, options);
    }

    serializer.endTag(name);
    break;
  }

  case pugi::node_pcdata: {
    std::string text_content = node.value();
    if (is_whitespace_only(text_content)) {
      if (!options.collapse_whitespaces) {
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

  case pugi::node_declaration:
    // skip XML declarations
    break;

  default:
    break;
  }
}

} // anonymous namespace

// ============================================================================
// LOW-LEVEL API
// ============================================================================

void serializeXmlToAbx(BinaryXmlSerializer &serializer,
                       const pugi::xml_node &xml_node,
                       const XmlToAbxOptions &options) {
  process_xml_node_internal(serializer, xml_node, options);
}

// ============================================================================
// HIGH-LEVEL API
// ============================================================================

void convertXmlFileToAbx(const std::string &xml_path, std::ostream &abx_output,
                         const XmlToAbxOptions &options) {
  // parse the XML file
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_file(xml_path.c_str());

  if (!result) {
    throw std::runtime_error("Failed to parse XML file '" + xml_path +
                             "': " + std::string(result.description()));
  }

  // create serializer and convert
  BinaryXmlSerializer serializer(abx_output);
  serializer.startDocument();

  // process all children of the document
  for (const auto &child : doc.children()) {
    serializeXmlToAbx(serializer, child, options);
  }

  serializer.endDocument();
}

void convertXmlStringToAbx(const std::string &xml_string,
                           std::ostream &abx_output,
                           const XmlToAbxOptions &options) {
  // parse the XML string
  pugi::xml_document doc;
  pugi::xml_parse_result result = doc.load_string(xml_string.c_str());

  if (!result) {
    throw std::runtime_error("Failed to parse XML string: " +
                             std::string(result.description()));
  }

  // create serializer and convert
  BinaryXmlSerializer serializer(abx_output);
  serializer.startDocument();

  for (const auto &child : doc.children()) {
    serializeXmlToAbx(serializer, child, options);
  }

  serializer.endDocument();
}

void convertAbxToXmlFile(std::istream &abx_input, const std::string &xml_path) {
  // Open output file
  std::ofstream out(xml_path);
  if (!out) {
    throw std::runtime_error("Failed to open output file: " + xml_path);
  }

  // deserialize ABX to XML
  BinaryXmlDeserializer deserializer(abx_input, out);
  deserializer.deserialize();
}

std::string convertAbxToXmlString(std::istream &abx_input) {
  // use stringstream as output
  std::ostringstream out;

  // deserialize ABX to XML
  BinaryXmlDeserializer deserializer(abx_input, out);
  deserializer.deserialize();

  return out.str();
}

} // namespace libabx
