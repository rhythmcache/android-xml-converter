use android_xml_converter::*;
use base64::Engine;
use faster_hex::hex_string;
use std::env;
use std::fs::File;
use std::io::{self, BufReader, BufWriter, Cursor, Read, Write};

// ============================================================================
// Data Input Reader
// ============================================================================

pub struct DataInput<R: Read> {
    reader: R,
    interned_strings: Vec<String>,
    peeked_byte: Option<u8>,
}

impl<R: Read> DataInput<R> {
    pub fn new(reader: R) -> Self {
    Self {
        reader,
        interned_strings: Vec::with_capacity(INITIAL_STRING_POOL_CAPACITY),
        peeked_byte: None,
    }
}

    pub fn read_byte(&mut self) -> Result<u8> {
        if let Some(byte) = self.peeked_byte.take() {
            return Ok(byte);
        }
        let mut buf = [0u8; 1];
        self.reader
            .read_exact(&mut buf)
            .map_err(|_| ConversionError::ReadError("byte".to_string()))?;
        Ok(buf[0])
    }

    pub fn peek_byte(&mut self) -> Result<u8> {
        if let Some(byte) = self.peeked_byte {
            return Ok(byte);
        }
        let byte = self.read_byte()?;
        self.peeked_byte = Some(byte);
        Ok(byte)
    }

    pub fn read_short(&mut self) -> Result<u16> {
        let mut buf = [0u8; 2];
        if let Some(byte) = self.peeked_byte.take() {
            buf[0] = byte;
            self.reader
                .read_exact(&mut buf[1..])
                .map_err(|_| ConversionError::ReadError("short".to_string()))?;
        } else {
            self.reader
                .read_exact(&mut buf)
                .map_err(|_| ConversionError::ReadError("short".to_string()))?;
        }
        Ok(u16::from_be_bytes(buf))
    }

    pub fn read_int(&mut self) -> Result<i32> {
        let mut buf = [0u8; 4];
        let start_idx = if let Some(byte) = self.peeked_byte.take() {
            buf[0] = byte;
            1
        } else {
            0
        };
        self.reader
            .read_exact(&mut buf[start_idx..])
            .map_err(|_| ConversionError::ReadError("int".to_string()))?;
        Ok(i32::from_be_bytes(buf))
    }

    pub fn read_long(&mut self) -> Result<i64> {
        let mut buf = [0u8; 8];
        let start_idx = if let Some(byte) = self.peeked_byte.take() {
            buf[0] = byte;
            1
        } else {
            0
        };
        self.reader
            .read_exact(&mut buf[start_idx..])
            .map_err(|_| ConversionError::ReadError("long".to_string()))?;
        Ok(i64::from_be_bytes(buf))
    }

    pub fn read_float(&mut self) -> Result<f32> {
        let int_value = self.read_int()? as u32;
        Ok(f32::from_bits(int_value))
    }

    pub fn read_double(&mut self) -> Result<f64> {
        let int_value = self.read_long()? as u64;
        Ok(f64::from_bits(int_value))
    }

    pub fn read_utf(&mut self) -> Result<String> {
        let length = self.read_short()?;
        let mut buffer = vec![0u8; length as usize];
        self.reader
            .read_exact(&mut buffer)
            .map_err(|_| ConversionError::ReadError("UTF string".to_string()))?;
        String::from_utf8(buffer)
            .map_err(|_| ConversionError::ReadError("UTF string (invalid UTF-8)".to_string()))
    }

    pub fn read_interned_utf(&mut self) -> Result<String> {
        let index = self.read_short()?;
        if index == INTERNED_STRING_NEW_MARKER {
            let string = self.read_utf()?;
            self.interned_strings.push(string.clone());
            Ok(string)
        } else {
            self.interned_strings
                .get(index as usize)
                .cloned()
                .ok_or(ConversionError::InvalidInternedStringIndex(index))
        }
    }

    pub fn read_bytes(&mut self, length: u16) -> Result<Vec<u8>> {
        let mut data = vec![0u8; length as usize];
        self.reader
            .read_exact(&mut data)
            .map_err(|_| ConversionError::ReadError("bytes".to_string()))?;
        Ok(data)
    }
}

// ============================================================================
// Binary XML Deserializer
// ============================================================================

pub struct BinaryXmlDeserializer<R: Read, W: Write> {
    input: DataInput<R>,
    output: W,
}

impl<R: Read, W: Write> BinaryXmlDeserializer<R, W> {
    pub fn new(mut reader: R, output: W) -> Result<Self> {
        let mut magic = [0u8; 4];
        reader
            .read_exact(&mut magic)
            .map_err(|_| ConversionError::ReadError("magic header".to_string()))?;

        if magic != PROTOCOL_MAGIC_VERSION_0 {
            return Err(ConversionError::InvalidMagicHeader {
                expected: PROTOCOL_MAGIC_VERSION_0,
                actual: magic,
            });
        }

        Ok(Self {
            input: DataInput::new(reader),
            output,
        })
    }

    pub fn deserialize(&mut self) -> Result<()> {
        self.output
            .write_all(b"<?xml version=\"1.0\" encoding=\"UTF-8\"?>")?;

        loop {
            match self.process_token() {
                Ok(should_continue) => {
                    if !should_continue {
                        break;
                    }
                }
                Err(ConversionError::ReadError(_)) => {
                    break;
                }
                Err(e) => {
                    eprintln!("Warning: Error parsing token: {}", e);
                    break;
                }
            }
        }

        Ok(())
    }
    fn process_token(&mut self) -> Result<bool> {
        let token = self.input.read_byte()?;
        let command = token & 0x0F;
        let type_info = token & 0xF0;

        match command {
            START_DOCUMENT => Ok(true),
            END_DOCUMENT => Ok(false),
            START_TAG => {
                let tag_name = self.input.read_interned_utf()?;
                self.output.write_all(b"<")?;
                self.output.write_all(tag_name.as_bytes())?;

                while let Ok(next_token) = self.input.peek_byte() {
                    if (next_token & 0x0F) != ATTRIBUTE {
                        break;
                    }

                    let _ = self.input.read_byte()?;
                    self.process_attribute(next_token)?;
                }

                self.output.write_all(b">")?;
                Ok(true)
            }
            END_TAG => {
                let tag_name = self.input.read_interned_utf()?;
                self.output.write_all(b"</")?;
                self.output.write_all(tag_name.as_bytes())?;
                self.output.write_all(b">")?;
                Ok(true)
            }
            TEXT => {
                if type_info == TYPE_STRING {
                    let text = self.input.read_utf()?;
                    if !text.is_empty() {
                        let encoded = encode_xml_entities(&text);
                        self.output.write_all(encoded.as_bytes())?;
                    }
                }
                Ok(true)
            }
            CDSECT => {
                if type_info == TYPE_STRING {
                    let text = self.input.read_utf()?;
                    self.output.write_all(b"<![CDATA[")?;
                    self.output.write_all(text.as_bytes())?;
                    self.output.write_all(b"]]>")?;
                }
                Ok(true)
            }
            COMMENT => {
                if type_info == TYPE_STRING {
                    let text = self.input.read_utf()?;
                    self.output.write_all(b"<!--")?;
                    self.output.write_all(text.as_bytes())?;
                    self.output.write_all(b"-->")?;
                }
                Ok(true)
            }
            PROCESSING_INSTRUCTION => {
                if type_info == TYPE_STRING {
                    let text = self.input.read_utf()?;
                    self.output.write_all(b"<?")?;
                    self.output.write_all(text.as_bytes())?;
                    self.output.write_all(b"?>")?;
                }
                Ok(true)
            }
            DOCDECL => {
                if type_info == TYPE_STRING {
                    let text = self.input.read_utf()?;
                    self.output.write_all(b"<!DOCTYPE ")?;
                    self.output.write_all(text.as_bytes())?;
                    self.output.write_all(b">")?;
                }
                Ok(true)
            }
            ENTITY_REF => {
                if type_info == TYPE_STRING {
                    let text = self.input.read_utf()?;
                    self.output.write_all(b"&")?;
                    self.output.write_all(text.as_bytes())?;
                    self.output.write_all(b";")?;
                }
                Ok(true)
            }
            IGNORABLE_WHITESPACE => {
                if type_info == TYPE_STRING {
                    let text = self.input.read_utf()?;
                    self.output.write_all(text.as_bytes())?;
                }
                Ok(true)
            }
            _ => {
                eprintln!("Warning: Unknown token: {}", command);
                Ok(true)
            }
        }
    }

    fn process_attribute(&mut self, token: u8) -> Result<()> {
        let type_info = token & 0xF0;
        let name = self.input.read_interned_utf()?;

        self.output.write_all(b" ")?;
        self.output.write_all(name.as_bytes())?;
        self.output.write_all(b"=\"")?;

        match type_info {
            TYPE_STRING => {
                let value = self.input.read_utf()?;
                let encoded = encode_xml_entities(&value);
                self.output.write_all(encoded.as_bytes())?;
            }
            TYPE_STRING_INTERNED => {
                let value = self.input.read_interned_utf()?;
                let encoded = encode_xml_entities(&value);
                self.output.write_all(encoded.as_bytes())?;
            }
            TYPE_INT => {
                let value = self.input.read_int()?;
                write!(self.output, "{}", value)?;
            }
            TYPE_INT_HEX => {
                let value = self.input.read_int()?;
                if value == -1 {
                    write!(self.output, "{}", value)?;
                } else {
                    write!(self.output, "{:x}", value as u32)?;
                }
            }
            TYPE_LONG => {
                let value = self.input.read_long()?;
                write!(self.output, "{}", value)?;
            }
            TYPE_LONG_HEX => {
                let value = self.input.read_long()?;
                if value == -1 {
                    write!(self.output, "{}", value)?;
                } else {
                    write!(self.output, "{:x}", value as u64)?;
                }
            }
            TYPE_FLOAT => {
                let value = self.input.read_float()?;
                if value.fract() == 0.0 && value.is_finite() {
                    write!(self.output, "{:.1}", value)?;
                } else {
                    write!(self.output, "{}", value)?;
                }
            }
            TYPE_DOUBLE => {
                let value = self.input.read_double()?;
                if value.fract() == 0.0 && value.is_finite() {
                    write!(self.output, "{:.1}", value)?;
                } else {
                    write!(self.output, "{}", value)?;
                }
            }
            TYPE_BOOLEAN_TRUE => {
                self.output.write_all(b"true")?;
            }
            TYPE_BOOLEAN_FALSE => {
                self.output.write_all(b"false")?;
            }
            TYPE_BYTES_HEX => {
                let length = self.input.read_short()?;
                let bytes = self.input.read_bytes(length)?;
                let hex = hex_string(&bytes);
                self.output.write_all(hex.as_bytes())?;
            }
            TYPE_BYTES_BASE64 => {
                let length = self.input.read_short()?;
                let bytes = self.input.read_bytes(length)?;
                let encoded = base64::engine::general_purpose::STANDARD.encode(&bytes);
                self.output.write_all(encoded.as_bytes())?;
            }
            _ => {
                return Err(ConversionError::UnknownAttributeType(type_info));
            }
        }

        self.output.write_all(b"\"")?;
        Ok(())
    }
}

// ============================================================================
// Converter API
// ============================================================================

pub struct AbxToXmlConverter;

impl AbxToXmlConverter {
    pub fn convert<R: Read, W: Write>(reader: R, writer: W) -> Result<()> {
        let mut deserializer = BinaryXmlDeserializer::new(reader, writer)?;
        deserializer.deserialize()
    }

    pub fn convert_file(input_path: &str, output_path: &str) -> Result<()> {
        if input_path == output_path {
            return Self::convert_file_in_place(input_path);
        }

        let input_file = File::open(input_path)?;
        let reader = BufReader::new(input_file);
        let output_file = File::create(output_path)?;
        let writer = BufWriter::new(output_file);
        Self::convert(reader, writer)
    }

    pub fn convert_stdin_stdout() -> Result<()> {
        let stdin = io::stdin();
        let reader = stdin.lock();
        let stdout = io::stdout();
        let writer = BufWriter::new(stdout.lock());
        Self::convert(reader, writer)
    }

    pub fn convert_stdin_to_file(output_path: &str) -> Result<()> {
        let stdin = io::stdin();
        let reader = stdin.lock();
        let output_file = File::create(output_path)?;
        let writer = BufWriter::new(output_file);
        Self::convert(reader, writer)
    }

    pub fn convert_file_to_stdout(input_path: &str) -> Result<()> {
        let input_file = File::open(input_path)?;
        let reader = BufReader::new(input_file);
        let writer = io::stdout();
        Self::convert(reader, writer)
    }

    fn convert_file_in_place(file_path: &str) -> Result<()> {
        let input_file = File::open(file_path)?;
        let mut reader = BufReader::new(input_file);
        let mut file_data = Vec::new();
        reader.read_to_end(&mut file_data)?;

        let cursor = Cursor::new(file_data);
        let mut output_data = Vec::new();
        {
            let writer = Cursor::new(&mut output_data);
            Self::convert(cursor, writer)?;
        }

        let output_file = File::create(file_path)?;
        let mut writer = BufWriter::new(output_file);
        writer.write_all(&output_data)?;
        writer.flush()?;
        Ok(())
    }

    pub fn convert_bytes(abx_data: &[u8]) -> Result<String> {
        let cursor = Cursor::new(abx_data);
        let mut output_data = Vec::new();
        {
            let writer = Cursor::new(&mut output_data);
            Self::convert(cursor, writer)?;
        }
        String::from_utf8(output_data)
            .map_err(|_| ConversionError::ParseError("Invalid UTF-8 in output".to_string()))
    }

    pub fn convert_vec(abx_data: Vec<u8>) -> Result<String> {
        Self::convert_bytes(&abx_data)
    }
}

// ============================================================================
// CLI
// ============================================================================

struct Cli;

impl Cli {
    fn print_help(program_name: &str) {
        eprintln!("Usage: {} [OPTIONS] <input> [output]", program_name);
        eprintln!();
        eprintln!("Converts Android Binary XML (ABX) to human-readable XML.");
        eprintln!();
        eprintln!("Arguments:");
        eprintln!("  input              Input file path (use '-' for stdin)");
        eprintln!("  output             Output file path (use '-' for stdout)");
        eprintln!("                     If not specified, defaults to stdout or in-place");
        eprintln!();
        eprintln!("Options:");
        eprintln!("  -i, --in-place     Overwrite input file with converted output");
        eprintln!("  -h, --help         Show this help message");
    }

    fn run() -> Result<()> {
        let mut args = env::args();
        let bin_name = args
            .next()
            .as_ref()
            .and_then(|p| std::path::Path::new(p).file_name())
            .and_then(|n| n.to_str())
            .unwrap_or("abx2xml")
            .to_string();

        let args: Vec<String> = args.collect();

        if args.is_empty() || args.iter().any(|a| a == "-h" || a == "--help") {
            Self::print_help(&bin_name);
            std::process::exit(if args.is_empty() { 1 } else { 0 });
        }

        let mut in_place = false;
        let mut input_path = None;
        let mut output_path = None;
        let mut after_double_dash = false;

        for arg in &args {
            if !after_double_dash && arg == "--" {
                after_double_dash = true;
            } else if !after_double_dash && (arg == "-i" || arg == "--in-place") {
                in_place = true;
            } else if input_path.is_none() {
                input_path = Some(arg.as_str());
            } else if output_path.is_none() {
                output_path = Some(arg.as_str());
            } else {
                return Err(ConversionError::ParseError(format!(
                    "Unexpected argument: {}",
                    arg
                )));
            }
        }

        let input_path = input_path.ok_or_else(|| {
            ConversionError::ParseError("Missing required argument: INPUT".to_string())
        })?;

        if in_place && input_path == "-" {
            return Err(ConversionError::ParseError(
                "Cannot use -i option with stdin input".to_string(),
            ));
        }

        let output_path = match output_path {
            Some(path) => path,
            None => {
                if in_place {
                    input_path
                } else {
                    "-"
                }
            }
        };

        match (input_path, output_path) {
            ("-", "-") => AbxToXmlConverter::convert_stdin_stdout(),
            ("-", output) => AbxToXmlConverter::convert_stdin_to_file(output),
            (input, "-") => AbxToXmlConverter::convert_file_to_stdout(input),
            (input, output) => AbxToXmlConverter::convert_file(input, output),
        }
    }
}

fn main() {
    if let Err(e) = Cli::run() {
        eprintln!("Error: {}", e);
        std::process::exit(1);
    }
}
