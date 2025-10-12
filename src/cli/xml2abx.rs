use android_xml_converter::{Options, convert_xml_string_to_buffer};
use std::fs;
use std::io::{self, Read, Write};
use std::process;

struct Args {
    input: String,
    output: Option<String>,
    in_place: bool,
    collapse_whitespaces: bool,
}

fn print_help() {
    eprintln!("Converts human-readable XML to Android Binary XML (ABX).");
    eprintln!();
    eprintln!("Usage: xml2abx [OPTIONS] <input> [output]");
    eprintln!();
    eprintln!("Arguments:");
    eprintln!("  <input>   Input file path (use '-' for stdin)");
    eprintln!("  [output]  Output file path (use '-' for stdout, defaults to stdout)");
    eprintln!();
    eprintln!("Options:");
    eprintln!("  -i, --in-place              Overwrite input file with converted output");
    eprintln!("  -c, --collapse-whitespaces  Remove unnecessary whitespace");
    eprintln!("  -h, --help                  Print this help message");
}

fn parse_args() -> Option<Args> {
    let args: Vec<String> = std::env::args().collect();
    let mut input = None;
    let mut output = None;
    let mut in_place = false;
    let mut collapse_whitespaces = false;
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
            "-c" | "--collapse-whitespaces" => {
                collapse_whitespaces = true;
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
        collapse_whitespaces,
    })
}

fn read_input_string(input: &str) -> io::Result<String> {
    if input == "-" {
        let mut buffer = String::new();
        io::stdin().read_to_string(&mut buffer)?;
        Ok(buffer)
    } else {
        fs::read_to_string(input)
    }
}

fn write_output_xml2abx(output: Option<&str>, content: &[u8]) -> io::Result<()> {
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

    let xml_content = match read_input_string(&args.input) {
        Ok(content) => content,
        Err(e) => {
            eprintln!("Error reading input: {}", e);
            process::exit(1);
        }
    };

    let options = Options::new().collapse_whitespaces(args.collapse_whitespaces);
    let abx_buffer = match convert_xml_string_to_buffer(&xml_content, Some(options)) {
        Ok(buffer) => buffer,
        Err(e) => {
            eprintln!("Error converting XML to ABX: {}", e);
            process::exit(1);
        }
    };

    let output_path = if args.in_place {
        Some(args.input.as_str())
    } else {
        args.output.as_deref()
    };

    if let Err(e) = write_output_xml2abx(output_path, &abx_buffer) {
        eprintln!("Error writing output: {}", e);
        process::exit(1);
    }
}
