// ============================================================================
// IMPROVED abx2xml.rs
// ============================================================================

use android_xml_converter::convert_abx_buffer_to_string;
use std::fs;
use std::io::{self, Read, Write};
use std::process;

struct Args {
    input: String,
    output: Option<String>,
    in_place: bool,
}

fn print_help() {
    eprintln!("Converts Android Binary XML (ABX) to human-readable XML.");
    eprintln!();
    eprintln!("Usage: abx2xml [OPTIONS] <input> [output]");
    eprintln!();
    eprintln!("Arguments:");
    eprintln!("  <input>   Input file path (use '-' for stdin)");
    eprintln!("  [output]  Output file path (use '-' for stdout, defaults to stdout)");
    eprintln!();
    eprintln!("Options:");
    eprintln!("  -i, --in-place  Overwrite input file with converted output");
    eprintln!("  -h, --help      Print this help message");
}

fn parse_args() -> Option<Args> {
    let args: Vec<String> = std::env::args().collect();
    let mut input = None;
    let mut output = None;
    let mut in_place = false;
    let mut i = 1;

    while i < args.len() {
        match args[i].as_str() {
            "-h" | "--help" => {
                print_help();
                return None;
            }
            "-i" | "--in-place" => {
                in_place = true;
            }
            arg if arg.starts_with('-') && arg != "-" => {
                eprintln!("Error: Unknown option '{}'", arg);
                print_help();
                process::exit(1);
            }
            arg => {
                if input.is_none() {
                    input = Some(arg.to_string());
                } else if output.is_none() {
                    output = Some(arg.to_string());
                } else {
                    eprintln!("Error: Too many arguments");
                    print_help();
                    process::exit(1);
                }
            }
        }
        i += 1;
    }

    if input.is_none() {
        eprintln!("Error: Missing required argument <input>");
        print_help();
        process::exit(1);
    }

    Some(Args {
        input: input.unwrap(),
        output,
        in_place,
    })
}

fn read_input(input: &str) -> io::Result<Vec<u8>> {
    if input == "-" {
        let mut buffer = Vec::new();
        io::stdin().read_to_end(&mut buffer)?;
        Ok(buffer)
    } else {
        fs::read(input)
    }
}

fn write_output(output: Option<&str>, content: &[u8]) -> io::Result<()> {
    match output {
        Some(path) if path == "-" => io::stdout().write_all(content),
        None => io::stdout().write_all(content),
        Some(path) => fs::write(path, content).map(|_| ()),
    }
}

fn main() {
    let args = match parse_args() {
        Some(a) => a,
        None => return,
    };

    if args.in_place && args.input == "-" {
        eprintln!("Error: Cannot use --in-place with stdin");
        process::exit(1);
    }

    if args.in_place && args.output.is_some() {
        eprintln!("Error: Cannot specify output file with --in-place");
        process::exit(1);
    }

    let buffer = match read_input(&args.input) {
        Ok(b) => b,
        Err(e) => {
            eprintln!("Error reading input: {}", e);
            process::exit(1);
        }
    };

    let xml_content = match convert_abx_buffer_to_string(&buffer) {
        Ok(content) => content,
        Err(e) => {
            eprintln!("Error converting ABX to XML: {}", e);
            process::exit(1);
        }
    };

    let output_path = if args.in_place {
        Some(args.input.as_str())
    } else {
        args.output.as_deref()
    };

    if let Err(e) = write_output(output_path, xml_content.as_bytes()) {
        eprintln!("Error writing output: {}", e);
        process::exit(1);
    }
}
