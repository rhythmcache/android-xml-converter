#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

#include "abx.hh"

#ifdef _WIN32
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

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
    bool in_place = false;
    std::string input_path;
    std::string output_path;

    int arg_idx = 1;
    while (arg_idx < argc) {
        if (strcmp(argv[arg_idx], "-i") == 0) {
            in_place = true;
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
        std::ostringstream memory_output;
        std::ostream* output_stream = in_place ? &memory_output : nullptr;
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

            if (in_place) {
                libabx::BinaryXmlDeserializer deserializer(input_file, memory_output);
                deserializer.deserialize();
                input_file.close();

                std::ofstream output_file(output_path);
                if (!output_file) {
                    std::cerr << "Error: Cannot open output file: " << output_path << "\n";
                    return 1;
                }
                output_file << memory_output.str();
                output_file.close();
            } else {
                if (output_path == "-") {
                    libabx::BinaryXmlDeserializer deserializer(input_file, std::cout);
                    deserializer.deserialize();
                } else {
                    std::ofstream output_file(output_path);
                    if (!output_file) {
                        std::cerr << "Error: Cannot open output file: " << output_path << "\n";
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

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
