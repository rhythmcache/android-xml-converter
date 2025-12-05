#include "xml2abx.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
#include <sstream>


void print_usage(const char* program_name) {
    std::cerr << "Usage: " << program_name << " [options] <input.xml> [output.abx]" << std::endl;
    std::cerr << "Options:" << std::endl;
    std::cerr << "  -i, --in-place            Overwrite input file with output" << std::endl;
    std::cerr << "  -c, --collapse-whitespace Collapse whitespace" << std::endl;
    std::cerr << "  -h, --help                Show this help" << std::endl;
    std::cerr << std::endl;
    std::cerr << "Use '-' for stdin/stdout" << std::endl;
}

int main(int argc, char* argv[]) {
    try {
        std::string input_path;
        std::string output_path;
        bool in_place = false;
        bool collapse_whitespace = false;

        
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            } else if (arg == "-i" || arg == "--in-place") {
                in_place = true;
            } else if (arg == "-c" || arg == "--collapse-whitespace") {
                collapse_whitespace = true;
            } else if (arg[0] == '-' && arg.length() > 1 && arg != "-") {
                std::cerr << "Unknown option: " << arg << std::endl;
                print_usage(argv[0]);
                return 1;
            } else {
                if (input_path.empty()) {
                    input_path = arg;
                } else if (output_path.empty()) {
                    output_path = arg;
                } else {
                    std::cerr << "Too many arguments" << std::endl;
                    print_usage(argv[0]);
                    return 1;
                }
            }
        }

        if (input_path.empty()) {
            std::cerr << "Error: Input file required" << std::endl;
            print_usage(argv[0]);
            return 1;
        }

        
        std::string final_output_path;
        if (in_place) {
            if (input_path == "-") {
                std::cerr << "Error: Cannot use --in-place with stdin" << std::endl;
                return 1;
            }
            final_output_path = input_path;
        } else if (!output_path.empty()) {
            final_output_path = output_path;
        } else {
            std::cerr << "Error: Output path required (use '-' for stdout)" << std::endl;
            print_usage(argv[0]);
            return 1;
        }

        bool preserve_whitespace = !collapse_whitespace;

        
        std::string xml_content;
        if (input_path == "-") {
            std::stringstream buffer;
            buffer << std::cin.rdbuf();
            xml_content = buffer.str();
        } else {
            std::ifstream file(input_path);
            if (!file) {
                std::cerr << "Error: Cannot open input file: " << input_path << std::endl;
                return 1;
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            xml_content = buffer.str();
        }

        
        if (final_output_path == "-") {
            xml2abx::XmlToAbxConverter::convert_from_string(xml_content, std::cout, preserve_whitespace);
        } else {
            std::ofstream output_file(final_output_path, std::ios::binary);
            if (!output_file) {
                std::cerr << "Error: Cannot create output file: " << final_output_path << std::endl;
                return 1;
            }
            xml2abx::XmlToAbxConverter::convert_from_string(xml_content, output_file, preserve_whitespace);
        }

        return 0;

    } catch (const xml2abx::ConversionError& e) {
        std::cerr << "Conversion error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
