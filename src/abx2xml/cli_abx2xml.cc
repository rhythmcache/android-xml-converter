#include "abx2xml.hpp"
#include <iostream>
#include <string_view>
#include <vector>

void print_usage(std::string_view program_name) {
    std::cerr << "Usage: " << program_name << " [OPTIONS] <input> [output]\n\n";
    std::cerr << "Converts Android Binary XML (ABX) to human-readable XML.\n\n";
    std::cerr << "Arguments:\n";
    std::cerr << "  input              Input file path (use '-' for stdin)\n";
    std::cerr << "  output             Output file path (use '-' for stdout)\n";
    std::cerr << "                     If not specified, defaults to stdout or in-place\n\n";
    std::cerr << "Options:\n";
    std::cerr << "  -i, --in-place     Overwrite input file with converted output\n";
    std::cerr << "  -h, --help         Show this help message\n";
}

int main(int argc, char* argv[]) {
    try {
        bool in_place = false;
        std::string input_path;
        std::string output_path;
        
        
        std::vector<std::string_view> args(argv + 1, argv + argc);
        
        for (auto it = args.begin(); it != args.end(); ++it) {
            std::string_view arg = *it;
            
            if (arg == "-h" || arg == "--help") {
                print_usage(argv[0]);
                return 0;
            } else if (arg == "-i" || arg == "--in-place") {
                in_place = true;
            } else if (input_path.empty()) {
                input_path = arg;
            } else if (output_path.empty()) {
                output_path = arg;
            } else {
                std::cerr << "Error: Too many arguments\n\n";
                print_usage(argv[0]);
                return 1;
            }
        }
        
        
        if (input_path.empty()) {
            std::cerr << "Error: Input file required\n\n";
            print_usage(argv[0]);
            return 1;
        }
        
        if (in_place && input_path == "-") {
            std::cerr << "Error: Cannot use -i option with stdin input\n";
            return 1;
        }
        
        
        if (output_path.empty()) {
            output_path = in_place ? input_path : "-";
        }
        
        
        using namespace std::string_literals;
        
        if (input_path == "-" && output_path == "-") {
            abx2xml::AbxToXmlConverter::convert_stdin_stdout();
        } else if (input_path == "-") {
            abx2xml::AbxToXmlConverter::convert_stdin_to_file(output_path);
        } else if (output_path == "-") {
            abx2xml::AbxToXmlConverter::convert_file_to_stdout(input_path);
        } else {
            abx2xml::AbxToXmlConverter::convert_file(input_path, output_path);
        }
        
        return 0;
        
    } catch (const abx2xml::AbxError& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << '\n';
        return 1;
    }
}