#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "abx.hpp"
#include "pugixml.hpp"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

void print_usage() {
  std::cerr
      << "usage: abx2xml [-i|--in-place] [-p|--pretty-print] input [output]\n\n"
      << "Converts between Android Binary XML and human-readable XML.\n\n"
      << "Options:\n"
      << "  -i, --in-place   Overwrite the input file with the output\n"
      << "  -p, --pretty-print  Format the XML with proper indentation\n"
      << "  -h, --help       Show this help message\n\n"
      << "Input can be '-' to use stdin, and output can be '-' to use "
         "stdout.\n";
}

int main(int argc, char *argv[]) {
  bool in_place = false;
  bool pretty_print = false;
  std::string input_path;
  std::string output_path;

  int arg_idx = 1;
  while (arg_idx < argc) {
    const char *arg = argv[arg_idx];

    if (arg[0] == '-' && arg[1] != '-' && arg[1] != '\0' && arg[2] != '\0') {
      bool valid_combo = true;
      for (int i = 1; arg[i] != '\0'; i++) {
        if (arg[i] == 'i') {
          in_place = true;
        } else if (arg[i] == 'p') {
          pretty_print = true;
        } else if (arg[i] == 'h') {
          print_usage();
          return 0;
        } else {
          valid_combo = false;
          break;
        }
      }
      if (valid_combo) {
        arg_idx++;
        continue;
      }
    }

    if (strcmp(arg, "-i") == 0 || strcmp(arg, "--in-place") == 0) {
      in_place = true;
      arg_idx++;
    } else if (strcmp(arg, "-p") == 0 || strcmp(arg, "--pretty-print") == 0) {
      pretty_print = true;
      arg_idx++;
    } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
      print_usage();
      return 0;
    } else {
      break;
    }
  }
  if (arg_idx >= argc) {
    if (!isatty(fileno(stdin))) {
      input_path = "-";
      output_path = "-";
    } else {
      std::cerr << "Error: Missing input file\n\n";
      print_usage();
      return 1;
    }
  } else {
    input_path = argv[arg_idx++];

    if (in_place) {
      if (input_path == "-") {
        std::cerr << "Error: Cannot use -i/--in-place flag with stdin\n";
        return 1;
      }
      if (arg_idx < argc) {
        std::cerr
            << "Error: Cannot specify output file with -i/--in-place flag\n";
        return 1;
      }
      output_path = input_path;
    } else {
      if (arg_idx < argc) {
        output_path = argv[arg_idx++];
      } else {
        output_path = "-";
      }
    }
  }

  try {
    std::ostringstream memory_output;
    bool needs_memory = in_place || pretty_print;
    std::ostream *output_stream = needs_memory ? &memory_output : nullptr;
    std::ifstream input_file;

    if (input_path == "-") {
      if (!output_stream) {
        output_stream = &std::cout;
      }
      libabx::BinaryXmlDeserializer deserializer(std::cin, *output_stream);
      deserializer.deserialize();
    } else {
      input_file.open(input_path, std::ios::binary);
      if (!input_file) {
        std::cerr << "Error: Cannot open input file: " << input_path << "\n";
        return 1;
      }

      if (!output_stream) {
        if (output_path == "-") {
          output_stream = &std::cout;
        } else {
        }
      }

      if (needs_memory) {
        libabx::BinaryXmlDeserializer deserializer(input_file, memory_output);
        deserializer.deserialize();
        input_file.close();

        std::string xml_content = memory_output.str();
        if (pretty_print) {
          pugi::xml_document doc;
          pugi::xml_parse_result result = doc.load_string(xml_content.c_str());

          if (!result) {
            std::cerr << "Error: Failed to parse XML: " << result.description()
                      << "\n";
            return 1;
          }

          memory_output.str("");
          memory_output.clear();

          doc.save(memory_output, "  ",
                   pugi::format_default | pugi::format_indent);
          xml_content = memory_output.str();
        }

        if (output_path == "-") {
          std::cout << xml_content;
        } else {
          std::ofstream output_file(output_path);
          if (!output_file) {
            std::cerr << "Error: Cannot open output file: " << output_path
                      << "\n";
            return 1;
          }
          output_file << xml_content;
          output_file.close();
        }
      } else {

        if (output_path == "-") {
          libabx::BinaryXmlDeserializer deserializer(input_file, std::cout);
          deserializer.deserialize();
        } else {
          std::ofstream output_file(output_path);
          if (!output_file) {
            std::cerr << "Error: Cannot open output file: " << output_path
                      << "\n";
            return 1;
          }
          libabx::BinaryXmlDeserializer deserializer(input_file, output_file);
          deserializer.deserialize();
          output_file.close();
        }
        input_file.close();
      }
    }

    return 0;

  } catch (const std::exception &e) {
    std::cerr << "Error: " << e.what() << "\n";
    return 1;
  }
}