use android_xml_converter::*;
use byteorder::{BigEndian, WriteBytesExt};
use ahash::AHashMap;
use quick_xml::Reader;
use quick_xml::events::Event;
use std::env;
use std::fs::File;
use std::io::{self, BufRead, BufWriter, Read, Write};

// ============================================================================
// Fast Data Output Writer
// ============================================================================

pub struct FastDataOutput<W: Write> {
    writer: W,
    string_pool: AHashMap<String, u16>,
    interned_strings: Vec<String>,
}

impl<W: Write> FastDataOutput<W> {
    pub fn new(writer: W) -> Self {
        Self {
            writer,
            string_pool: AHashMap::new(),
            interned_strings: Vec::with_capacity(INITIAL_STRING_POOL_CAPACITY),
        }
    }

    pub fn write_byte(&mut self, value: u8) -> Result<()> {
        self.writer.write_u8(value)?;
        Ok(())
    }

    pub fn write_short(&mut self, value: u16) -> Result<()> {
        self.writer.write_u16::<BigEndian>(value)?;
        Ok(())
    }

    pub fn write_int(&mut self, value: i32) -> Result<()> {
        self.writer.write_i32::<BigEndian>(value)?;
        Ok(())
    }

    pub fn write_long(&mut self, value: i64) -> Result<()> {
        self.writer.write_i64::<BigEndian>(value)?;
        Ok(())
    }

    pub fn write_float(&mut self, value: f32) -> Result<()> {
        self.writer.write_f32::<BigEndian>(value)?;
        Ok(())
    }

    pub fn write_double(&mut self, value: f64) -> Result<()> {
        self.writer.write_f64::<BigEndian>(value)?;
        Ok(())
    }

    pub fn write_utf(&mut self, s: &str) -> Result<()> {
        let bytes = s.as_bytes();
        if bytes.len() > MAX_UNSIGNED_SHORT as usize {
            return Err(ConversionError::StringTooLong(
                bytes.len(),
                MAX_UNSIGNED_SHORT as usize,
            ));
        }
        self.write_short(bytes.len() as u16)?;
        self.writer.write_all(bytes)?;
        Ok(())
    }

    pub fn write_interned_utf(&mut self, s: &str) -> Result<()> {
        if let Some(&index) = self.string_pool.get(s) {
            self.write_short(index)?;
        } else {
            self.write_short(INTERNED_STRING_NEW_MARKER)?;
            self.write_utf(s)?;
            let index = self.interned_strings.len() as u16;
            self.string_pool.insert(s.to_string(), index);
            self.interned_strings.push(s.to_string());
        }
        Ok(())
    }

    pub fn write_bytes(&mut self, data: &[u8]) -> Result<()> {
        self.writer.write_all(data)?;
        Ok(())
    }

    pub fn flush(&mut self) -> Result<()> {
        self.writer.flush()?;
        Ok(())
    }
}

// ============================================================================
// Binary XML Serializer
// ============================================================================

pub struct BinaryXmlSerializer<W: Write> {
    output: FastDataOutput<W>,
    tag_count: usize,
    tag_names: Vec<String>,
    preserve_whitespace: bool,
}

impl<W: Write> BinaryXmlSerializer<W> {
    pub fn new(writer: W) -> Result<Self> {
        Self::with_options(writer, true)
    }

    pub fn with_options(writer: W, preserve_whitespace: bool) -> Result<Self> {
        let mut output = FastDataOutput::new(writer);
        output.write_bytes(&PROTOCOL_MAGIC_VERSION_0)?;
        Ok(Self {
            output,
            tag_count: 0,
            tag_names: Vec::with_capacity(8),
            preserve_whitespace,
        })
    }

    fn write_token(&mut self, token: u8, text: Option<&str>) -> Result<()> {
        if let Some(text) = text {
            self.output.write_byte(token | TYPE_STRING)?;
            self.output.write_utf(text)?;
        } else {
            self.output.write_byte(token | TYPE_NULL)?;
        }
        Ok(())
    }

    pub fn start_document(&mut self) -> Result<()> {
        self.output.write_byte(START_DOCUMENT | TYPE_NULL)
    }

    pub fn end_document(&mut self) -> Result<()> {
        self.output.write_byte(END_DOCUMENT | TYPE_NULL)?;
        self.output.flush()
    }

    pub fn start_tag(&mut self, name: &str) -> Result<()> {
        if self.tag_count == self.tag_names.len() {
            let new_size = self.tag_count + std::cmp::max(1, self.tag_count / 2);
            self.tag_names.resize(new_size, String::new());
        }
        self.tag_names[self.tag_count] = name.to_string();
        self.tag_count += 1;

        self.output.write_byte(START_TAG | TYPE_STRING_INTERNED)?;
        self.output.write_interned_utf(name)
    }

    pub fn end_tag(&mut self, name: &str) -> Result<()> {
        self.tag_count -= 1;
        self.output.write_byte(END_TAG | TYPE_STRING_INTERNED)?;
        self.output.write_interned_utf(name)
    }

    pub fn attribute(&mut self, name: &str, value: &str) -> Result<()> {
        self.output.write_byte(ATTRIBUTE | TYPE_STRING)?;
        self.output.write_interned_utf(name)?;
        self.output.write_utf(value)
    }

    pub fn attribute_interned(&mut self, name: &str, value: &str) -> Result<()> {
        self.output.write_byte(ATTRIBUTE | TYPE_STRING_INTERNED)?;
        self.output.write_interned_utf(name)?;
        self.output.write_interned_utf(value)
    }

    pub fn attribute_bytes_hex(&mut self, name: &str, value: &[u8]) -> Result<()> {
        if value.len() > MAX_UNSIGNED_SHORT as usize {
            return Err(ConversionError::BinaryDataTooLong(
                value.len(),
                MAX_UNSIGNED_SHORT as usize,
            ));
        }
        self.output.write_byte(ATTRIBUTE | TYPE_BYTES_HEX)?;
        self.output.write_interned_utf(name)?;
        self.output.write_short(value.len() as u16)?;
        self.output.write_bytes(value)
    }

    pub fn attribute_bytes_base64(&mut self, name: &str, value: &[u8]) -> Result<()> {
        if value.len() > MAX_UNSIGNED_SHORT as usize {
            return Err(ConversionError::BinaryDataTooLong(
                value.len(),
                MAX_UNSIGNED_SHORT as usize,
            ));
        }
        self.output.write_byte(ATTRIBUTE | TYPE_BYTES_BASE64)?;
        self.output.write_interned_utf(name)?;
        self.output.write_short(value.len() as u16)?;
        self.output.write_bytes(value)
    }

    pub fn attribute_int(&mut self, name: &str, value: i32) -> Result<()> {
        self.output.write_byte(ATTRIBUTE | TYPE_INT)?;
        self.output.write_interned_utf(name)?;
        self.output.write_int(value)
    }

    pub fn attribute_int_hex(&mut self, name: &str, value: i32) -> Result<()> {
        self.output.write_byte(ATTRIBUTE | TYPE_INT_HEX)?;
        self.output.write_interned_utf(name)?;
        self.output.write_int(value)
    }

    pub fn attribute_long(&mut self, name: &str, value: i64) -> Result<()> {
        self.output.write_byte(ATTRIBUTE | TYPE_LONG)?;
        self.output.write_interned_utf(name)?;
        self.output.write_long(value)
    }

    pub fn attribute_long_hex(&mut self, name: &str, value: i64) -> Result<()> {
        self.output.write_byte(ATTRIBUTE | TYPE_LONG_HEX)?;
        self.output.write_interned_utf(name)?;
        self.output.write_long(value)
    }

    pub fn attribute_float(&mut self, name: &str, value: f32) -> Result<()> {
        self.output.write_byte(ATTRIBUTE | TYPE_FLOAT)?;
        self.output.write_interned_utf(name)?;
        self.output.write_float(value)
    }

    pub fn attribute_double(&mut self, name: &str, value: f64) -> Result<()> {
        self.output.write_byte(ATTRIBUTE | TYPE_DOUBLE)?;
        self.output.write_interned_utf(name)?;
        self.output.write_double(value)
    }

    pub fn attribute_boolean(&mut self, name: &str, value: bool) -> Result<()> {
        let token = if value {
            ATTRIBUTE | TYPE_BOOLEAN_TRUE
        } else {
            ATTRIBUTE | TYPE_BOOLEAN_FALSE
        };
        self.output.write_byte(token)?;
        self.output.write_interned_utf(name)
    }

    pub fn text(&mut self, text: &str) -> Result<()> {
        self.write_token(TEXT, Some(text))
    }

    pub fn cdsect(&mut self, text: &str) -> Result<()> {
        self.write_token(CDSECT, Some(text))
    }

    pub fn comment(&mut self, text: &str) -> Result<()> {
        self.write_token(COMMENT, Some(text))
    }

    pub fn processing_instruction(&mut self, target: &str, data: Option<&str>) -> Result<()> {
        let full_pi = if let Some(data) = data {
            if data.is_empty() {
                target.to_string()
            } else {
                format!("{} {}", target, data)
            }
        } else {
            target.to_string()
        };
        self.write_token(PROCESSING_INSTRUCTION, Some(&full_pi))
    }

    pub fn docdecl(&mut self, text: &str) -> Result<()> {
        self.write_token(DOCDECL, Some(text))
    }

    pub fn ignorable_whitespace(&mut self, text: &str) -> Result<()> {
        self.write_token(IGNORABLE_WHITESPACE, Some(text))
    }

    pub fn entity_ref(&mut self, text: &str) -> Result<()> {
        self.write_token(ENTITY_REF, Some(text))
    }
}

// ============================================================================
// Converter API
// ============================================================================

pub struct XmlToAbxConverter;

impl XmlToAbxConverter {
    pub fn convert_from_string<W: Write>(xml: &str, writer: W) -> Result<()> {
        Self::convert_from_string_with_options(xml, writer, true)
    }

    pub fn convert_from_string_with_options<W: Write>(
        xml: &str,
        writer: W,
        preserve_whitespace: bool,
    ) -> Result<()> {
        let mut reader = Reader::from_str(xml);
        reader.config_mut().trim_text(!preserve_whitespace);
        Self::convert_reader_with_options(reader, writer, preserve_whitespace)
    }

    pub fn convert_from_file<W: Write>(input_path: &str, writer: W) -> Result<()> {
        Self::convert_from_file_with_options(input_path, writer, true)
    }

    pub fn convert_from_file_with_options<W: Write>(
        input_path: &str,
        writer: W,
        preserve_whitespace: bool,
    ) -> Result<()> {
        let mut reader = Reader::from_file(input_path)?;
        reader.config_mut().trim_text(!preserve_whitespace);
        Self::convert_reader_with_options(reader, writer, preserve_whitespace)
    }

    pub fn convert_from_reader<R: BufRead, W: Write>(input: R, writer: W) -> Result<()> {
        Self::convert_from_reader_with_options(input, writer, true)
    }

    pub fn convert_from_reader_with_options<R: BufRead, W: Write>(
        input: R,
        writer: W,
        preserve_whitespace: bool,
    ) -> Result<()> {
        let mut reader = Reader::from_reader(input);
        reader.config_mut().trim_text(!preserve_whitespace);
        Self::convert_reader_with_options(reader, writer, preserve_whitespace)
    }

    fn convert_reader_with_options<R: BufRead, W: Write>(
        mut reader: Reader<R>,
        writer: W,
        preserve_whitespace: bool,
    ) -> Result<()> {
        let mut serializer = BinaryXmlSerializer::with_options(writer, preserve_whitespace)?;
        let mut buf = Vec::with_capacity(INITIAL_EVENT_BUFFER_CAPACITY);
        let mut tag_stack = Vec::with_capacity(16);

        serializer.start_document()?;

        loop {
            match reader.read_event_into(&mut buf)? {
                Event::Start(e) => {
                    let name_bytes = e.name();
                    let name = std::str::from_utf8(name_bytes.as_ref())?;

                    if name.contains(':') {
                        show_warning(
                            "Namespaces and prefixes",
                            Some(&format!("Found prefixed element: {}", name)),
                        );
                    }

                    serializer.start_tag(name)?;
                    tag_stack.push(name.to_string());

                    for attr in e.attributes() {
                        let attr = attr?;
                        let attr_name = std::str::from_utf8(attr.key.as_ref())?;
                        let attr_value = std::str::from_utf8(&attr.value)?;

                        if attr_name.starts_with("xmlns") || attr_name.contains(':') {
                            show_warning(
                                "Namespaces and prefixes",
                                Some(&format!(
                                    "Found namespace declaration or prefixed attribute: {}",
                                    attr_name
                                )),
                            );
                        }

                        Self::write_attribute(&mut serializer, attr_name, attr_value)?;
                    }
                }
                Event::End(e) => {
                    let name_bytes = e.name();
                    let name = std::str::from_utf8(name_bytes.as_ref())?;
                    serializer.end_tag(name)?;
                    tag_stack.pop();
                }
                Event::Empty(e) => {
                    let name_bytes = e.name();
                    let name = std::str::from_utf8(name_bytes.as_ref())?;

                    if name.contains(':') {
                        show_warning(
                            "Namespaces and prefixes",
                            Some(&format!("Found prefixed element: {}", name)),
                        );
                    }

                    serializer.start_tag(name)?;

                    for attr in e.attributes() {
                        let attr = attr?;
                        let attr_name = std::str::from_utf8(attr.key.as_ref())?;
                        let attr_value = std::str::from_utf8(&attr.value)?;

                        if attr_name.starts_with("xmlns") || attr_name.contains(':') {
                            show_warning(
                                "Namespaces and prefixes",
                                Some(&format!(
                                    "Found namespace declaration or prefixed attribute: {}",
                                    attr_name
                                )),
                            );
                        }

                        Self::write_attribute(&mut serializer, attr_name, attr_value)?;
                    }

                    serializer.end_tag(name)?;
                }
                Event::Text(e) => {
                    let text = std::str::from_utf8(&e)?;
                    if type_detection::is_whitespace_only(text) {
                        if serializer.preserve_whitespace {
                            serializer.ignorable_whitespace(text)?;
                        }
                    } else {
                        serializer.text(text)?;
                    }
                }
                Event::CData(e) => {
                    let text = std::str::from_utf8(&e)?;
                    serializer.cdsect(text)?;
                }
                Event::Comment(e) => {
                    let text = std::str::from_utf8(&e)?;
                    serializer.comment(text)?;
                }
                Event::PI(e) => {
                    let target = std::str::from_utf8(e.target())?;
                    let raw = e.content();
                    let data = if raw.is_empty() {
                        None
                    } else {
                        Some(std::str::from_utf8(raw)?)
                    };

                    if target == "xml"
                        && let Some(content) = data
                        && content.contains("encoding")
                        && !content.to_lowercase().contains("utf-8")
                    {
                        show_warning(
                            "Non-UTF-8 encoding",
                            Some(&format!("Found in declaration: {}", content)),
                        );
                    }

                    serializer.processing_instruction(target, data)?;
                }
                Event::Decl(decl) => {
                    if let Some(enc_result) = decl.encoding() {
                        let enc_bytes = enc_result?;
                        let enc = std::str::from_utf8(enc_bytes.as_ref())?;
                        if !enc.to_lowercase().contains("utf-8") {
                            show_warning(
                                "Non-UTF-8 encoding",
                                Some(&format!("Found encoding: {}", enc)),
                            );
                        }
                    }
                }
                Event::DocType(e) => {
                    let text = std::str::from_utf8(&e)?;
                    serializer.docdecl(text)?;
                }
                Event::GeneralRef(e) => {
                    let text = std::str::from_utf8(&e)?;
                    serializer.entity_ref(text)?;
                }
                Event::Eof => break,
            }
            buf.clear();
        }

        serializer.end_document()?;
        Ok(())
    }

    fn write_attribute<W: Write>(
        serializer: &mut BinaryXmlSerializer<W>,
        name: &str,
        value: &str,
    ) -> Result<()> {
        use type_detection::*;

        if is_boolean(value) {
            serializer.attribute_boolean(name, value == "true")?;
        } else if value.len() < 50 && !value.contains(' ') {
            serializer.attribute_interned(name, value)?;
        } else {
            serializer.attribute(name, value)?;
        }
        Ok(())
    }
}

// ============================================================================
// CLI
// ============================================================================

fn print_help(bin_name: &str) {
    eprintln!("Usage: {} [options] <input.xml> [output.abx]", bin_name);
    eprintln!("Options:");
    eprintln!("  -i, --in-place            Overwrite input file with output");
    eprintln!("  -c, --collapse-whitespace Collapse whitespace");
    eprintln!("  -h, --help                Show this help");
    eprintln!();
    eprintln!("Use '-' for stdin/stdout");
}

fn main() -> std::result::Result<(), Box<dyn std::error::Error>> {
    let mut args = env::args();
    let bin_name = args
        .next()
        .as_ref()
        .and_then(|p| std::path::Path::new(p).file_name())
        .and_then(|n| n.to_str())
        .unwrap_or("xml2abx")
        .to_string();

    let args: Vec<String> = args.collect();

    if args.is_empty() || args.iter().any(|a| a == "-h" || a == "--help") {
        print_help(&bin_name);
        std::process::exit(if args.is_empty() { 1 } else { 0 });
    }

    let mut in_place = false;
    let mut collapse_whitespace = false;
    let mut input_path = None;
    let mut output_path = None;
    let mut after_double_dash = false;

    for arg in &args {
        if !after_double_dash && arg == "--" {
            after_double_dash = true;
        } else if !after_double_dash && (arg == "-i" || arg == "--in-place") {
            in_place = true;
        } else if !after_double_dash && (arg == "-c" || arg == "--collapse-whitespace") {
            collapse_whitespace = true;
        } else if input_path.is_none() {
            input_path = Some(arg.as_str());
        } else if output_path.is_none() {
            output_path = Some(arg.as_str());
        } else {
            eprintln!("Error: Unexpected argument: {}", arg);
            std::process::exit(1);
        }
    }

    let input_path = match input_path {
        Some(path) => path,
        None => {
            eprintln!("Error: Missing required argument: INPUT");
            std::process::exit(1);
        }
    };

    // preserve_whitespace is the inverse of collapse_whitespace
    let preserve_whitespace = !collapse_whitespace;

    let final_output_path = if in_place {
        if input_path == "-" {
            eprintln!("Error: Cannot overwrite stdin, output path is required");
            std::process::exit(1);
        }
        Some(input_path)
    } else if let Some(output) = output_path {
        Some(output)
    } else {
        eprintln!("Error: Output path is required (use '-' for stdout or specify a file)");
        std::process::exit(1);
    };

    let result = if input_path == "-" {
        let mut xml_content = String::new();
        io::stdin().read_to_string(&mut xml_content)?;

        if let Some(output_path) = final_output_path {
            if output_path == "-" {
                XmlToAbxConverter::convert_from_string_with_options(
                    &xml_content,
                    io::stdout(),
                    preserve_whitespace,
                )
            } else {
                let file = File::create(output_path)?;
                let writer = BufWriter::new(file);
                XmlToAbxConverter::convert_from_string_with_options(
                    &xml_content,
                    writer,
                    preserve_whitespace,
                )
            }
        } else {
            eprintln!("Error: Output path is required");
            std::process::exit(1);
        }
    } else {
        // for in-place editing, we need to read the file completely first
        let xml_content = std::fs::read_to_string(input_path)?;

        if let Some(output_path) = final_output_path {
            if output_path == "-" {
                XmlToAbxConverter::convert_from_string_with_options(
                    &xml_content,
                    io::stdout(),
                    preserve_whitespace,
                )
            } else {
                let file = File::create(output_path)?;
                let writer = BufWriter::new(file);
                XmlToAbxConverter::convert_from_string_with_options(
                    &xml_content,
                    writer,
                    preserve_whitespace,
                )
            }
        } else {
            eprintln!("Error: Output path is required");
            std::process::exit(1);
        }
    };

    match result {
        Ok(_) => Ok(()),
        Err(e) => {
            eprintln!("Error: {}", e);
            std::process::exit(1);
        }
    }
}
