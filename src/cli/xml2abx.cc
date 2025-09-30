#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "abx.hpp"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

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
    bool in_place = false;
    bool collapse_whitespaces = false;
    std::string input_path;
    std::string output_path;

    int arg_idx = 1;
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "-i") == 0) {
            in_place = true;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "--collapse-whitespaces") == 0) {
            collapse_whitespaces = true;
            arg_idx++;
        } else if (strcmp(argv[arg_idx], "-h") == 0 || strcmp(argv[arg_idx], "--help") == 0) {
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
                std::cerr << "Error: Cannot use -i flag with stdin\n";
                return 1;
            }
            if (arg_idx < argc) {
                std::cerr << "Error: Cannot specify output file with -i flag\n";
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
        libabx::XmlToAbxOptions options;
        options.collapse_whitespaces = collapse_whitespaces;

        options.warning_callback = [](const std::string& category, const std::string& message) {
            std::cerr << "Warning [" << category << "]: " << message << "\n";
        };

        if (in_place) {
            std::ostringstream memory_output;

            libabx::convertXmlFileToAbx(input_path, memory_output, options);

            std::ofstream output_file(output_path, std::ios::binary);
            if (!output_file) {
                std::cerr << "Error: Cannot open output file: " << output_path << "\n";
                return 1;
            }
            output_file << memory_output.str();
            output_file.close();
        } else {
            std::ofstream output_file;
            std::ostream* output_stream = nullptr;

            if (output_path == "-") {
                output_stream = &std::cout;
            } else {
                output_file.open(output_path, std::ios::binary);
                if (!output_file) {
                    std::cerr << "Error: Cannot open output file: " << output_path << "\n";
                    return 1;
                }
                output_stream = &output_file;
            }

            if (input_path == "-") {
                std::ostringstream buffer;
                buffer << std::cin.rdbuf();
                libabx::convertXmlStringToAbx(buffer.str(), *output_stream, options);
            } else {
                libabx::convertXmlFileToAbx(input_path, *output_stream, options);
            }

            output_file.close();
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
