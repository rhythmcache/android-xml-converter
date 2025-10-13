//! # Android Binary XML (ABX) Converter
//!
//! This library provides Rust bindings for converting between Android Binary XML (ABX) format
//! and standard XML. ABX is a compact binary representation of XML that uses:
//! - String interning to reduce duplicate string storage
//! - Type-aware encoding for attributes (integers, floats, booleans, etc.)
//! - Binary serialization for reduced file size and faster parsing
//!
//! ## Format Overview
//!
//! The ABX format uses a token-based binary protocol with:
//! - **Magic header**: `0x41 0x42 0x58 0x00` ("ABX\0")
//! - **Token byte structure**: Command (lower 4 bits) + Type (upper 4 bits)
//! - **String interning**: Frequently used strings are assigned indices
//! - **Type preservation**: Attributes maintain their semantic types (int, float, bool, etc.)
//!
//! ## Quick Start
//!
//! ### Converting XML to ABX
//!
//! ```rust,no_run
//! use android_xml_converter::{convert_xml_file_to_abx_file, Options};
//!
//! // Convert with default options
//! convert_xml_file_to_abx_file("input.xml", "output.abx", None)?;
//!
//! // Convert with whitespace collapsing
//! convert_xml_file_to_abx_file(
//!     "input.xml",
//!     "output.abx",
//!     Some(Options::new().collapse_whitespaces(true))
//! )?;
//! # Ok::<(), android_xml_converter::AbxException>(())
//! ```
//!
//! ### Converting ABX to XML
//!
//! ```rust,no_run
//! use android_xml_converter::convert_abx_file_to_xml_file;
//!
//! // Convert ABX back to XML
//! convert_abx_file_to_xml_file("input.abx", "output.xml")?;
//! # Ok::<(), android_xml_converter::AbxException>(())
//! ```
//!
//! ### Working with Strings and Buffers
//!
//! ```rust,no_run
//! use android_xml_converter::{convert_xml_string_to_buffer, convert_abx_buffer_to_string};
//!
//! // XML string → ABX buffer
//! let xml = r#"<manifest package="com.example.app" versionCode="1"/>"#;
//! let abx_data = convert_xml_string_to_buffer(xml, None)?;
//!
//! // ABX buffer → XML string
//! let xml_output = convert_abx_buffer_to_string(&abx_data)?;
//! # Ok::<(), android_xml_converter::AbxException>(())
//! ```
//!
//! ## Manual Document Construction
//!
//! For fine-grained control, use the `Serializer` API:
//!
//! ```rust,no_run
//! use android_xml_converter::Serializer;
//!
//! let mut ser = Serializer::create_buffer()?;
//! ser.start_document()?;
//! 
//! ser.start_tag("manifest")?;
//! ser.attribute_string("package", "com.example.app")?;
//! ser.attribute_int("versionCode", 1)?;
//! ser.attribute_int_hex("versionCodeMajor", 0xFF)?;
//! ser.attribute_bool("debuggable", false)?;
//! ser.end_tag("manifest")?;
//! 
//! ser.end_document()?;
//! let abx_data = ser.get_buffer();
//! # Ok::<(), android_xml_converter::AbxException>(())
//! ```
//!
//! ## Important:
//!
//! **ABX → XML (Deserialization)**: compatible with Android system files.
//!
//! **XML → ABX (Serialization)**: May produce different output than Android's official
//! `BinaryXmlSerializer`. This library uses **automatic type inference** (guessing whether
//! a value is int, float, boolean, etc. from the text), while Android uses **explicit type
//! methods** chosen by the programmer. This means:
//! - The output is valid ABX format
//! - Round-tripping (XML → ABX → XML) works correctly
//! - But the binary output may differ from what Android would generate
//! - Android system components may reject files if they expect specific types
//!
//! **Recommendation**: Use this library for reading/analyzing Android ABX files. For generating
//! ABX files that Android will consume, use the low-level `Serializer` API with explicit type
//! methods, or test thoroughly with actual Android components.
//!
//! ## Type Inference During Conversion
//!
//! When converting XML to ABX, the library automatically infers attribute types:
//! - `"true"` / `"false"` → Boolean
//! - `"0xFF"` → Hex integer
//! - `"42"` → Integer (if < 15 digits)
//! - `"3.14"` → Float
//! - `"3.14e5"` → Double
//! - Everything else → String
//!
//! ## Thread Safety
//!
//! - `Serializer` and `Deserializer` instances are **not thread-safe**
//! - Each thread should create its own instances
//! - The underlying C library uses thread-local error storage
//!
//! ## Performance Considerations
//!
//! - String interning reduces file size for documents with repeated strings
//! - Binary encoding is faster to parse than text XML
//! - Type-aware attributes avoid string-to-number conversions during parsing
//! - Typical size reduction: 20-40% compared to compressed XML
//!
//! ## Error Handling
//!
//! All fallible operations return `Result<T, AbxException>`. Error types include:
//! - File I/O errors
//! - Parse failures (invalid XML or corrupted ABX)
//! - Tag mismatches (mismatched start/end tags)
//! - Memory allocation failures
//!
//! ## Limitations
//!
//! - XML namespaces are preserved but not specially handled
//! - Maximum string length: 65,535 bytes (uint16 limit)
//! - Maximum attribute byte arrays: 65,535 bytes
//! - Comments and processing instructions are preserved

use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::fmt;
use std::ptr;

// ============================================================================
// FFI Bindings
// ============================================================================

/// Error codes returned by the underlying C library.
///
/// These represent all possible error conditions that can occur during
/// ABX conversion operations. Each error code maps to a specific failure mode.
#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum AbxError {
    /// Operation completed successfully
    Ok = 0,
    /// A null pointer was passed where a valid pointer was expected
    NullPointer = -1,
    /// An invalid handle was provided to a function
    InvalidHandle = -2,
    /// The specified file could not be found or opened
    FileNotFound = -3,
    /// Parsing the input data failed (malformed XML or corrupted ABX)
    ParseFailed = -4,
    /// Writing output data failed (disk full, permissions, etc.)
    WriteFailed = -5,
    /// The input format is invalid or corrupted
    InvalidFormat = -6,
    /// The provided buffer is too small to hold the result
    BufferTooSmall = -7,
    /// XML tag mismatch (closing tag doesn't match opening tag)
    TagMismatch = -8,
    /// Memory allocation failed
    OutOfMemory = -9,
    /// An unknown error occurred
    Unknown = -100,
}

/// Options for controlling the XML to ABX conversion process.
///
/// This struct is passed to the C library and controls various aspects
/// of the conversion behavior.
#[repr(C)]
pub struct AbxOptions {
    /// Whether to collapse consecutive whitespace characters into a single space.
    /// Set to 1 to enable, 0 to disable.
    pub collapse_whitespaces: i32,
}

// Callback type for warnings during conversion
#[allow(dead_code)]
type WarningCallback = unsafe extern "C" fn(*const i8, *const i8, *mut std::os::raw::c_void);

#[link(name = "abx")]
unsafe extern "C" {
    fn abx_get_last_error() -> *const i8;

    // Serializer
    fn abx_serializer_create_file(
        filepath: *const i8,
        error: *mut AbxError,
    ) -> *mut std::os::raw::c_void;
    fn abx_serializer_create_buffer(error: *mut AbxError) -> *mut std::os::raw::c_void;
    fn abx_serializer_start_document(serializer: *mut std::os::raw::c_void) -> AbxError;
    fn abx_serializer_end_document(serializer: *mut std::os::raw::c_void) -> AbxError;
    fn abx_serializer_start_tag(serializer: *mut std::os::raw::c_void, name: *const i8)
    -> AbxError;
    fn abx_serializer_end_tag(serializer: *mut std::os::raw::c_void, name: *const i8) -> AbxError;
    fn abx_serializer_attribute_string(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        value: *const i8,
    ) -> AbxError;
    fn abx_serializer_attribute_int(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        value: i32,
    ) -> AbxError;
    fn abx_serializer_attribute_int_hex(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        value: i32,
    ) -> AbxError;
    fn abx_serializer_attribute_long(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        value: i64,
    ) -> AbxError;
    fn abx_serializer_attribute_long_hex(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        value: i64,
    ) -> AbxError;
    fn abx_serializer_attribute_float(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        value: f32,
    ) -> AbxError;
    fn abx_serializer_attribute_double(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        value: f64,
    ) -> AbxError;
    fn abx_serializer_attribute_bool(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        value: i32,
    ) -> AbxError;
    fn abx_serializer_attribute_bytes_hex(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        data: *const u8,
        length: usize,
    ) -> AbxError;
    fn abx_serializer_attribute_bytes_base64(
        serializer: *mut std::os::raw::c_void,
        name: *const i8,
        data: *const u8,
        length: usize,
    ) -> AbxError;
    fn abx_serializer_text(serializer: *mut std::os::raw::c_void, text: *const i8) -> AbxError;
    fn abx_serializer_cdata(serializer: *mut std::os::raw::c_void, text: *const i8) -> AbxError;
    fn abx_serializer_comment(serializer: *mut std::os::raw::c_void, text: *const i8) -> AbxError;
    fn abx_serializer_get_buffer(
        serializer: *mut std::os::raw::c_void,
        out_buffer: *mut u8,
        buffer_size: usize,
    ) -> usize;
    fn abx_serializer_free(serializer: *mut std::os::raw::c_void);

    // Deserializer
    fn abx_deserializer_create_file(
        filepath: *const i8,
        error: *mut AbxError,
    ) -> *mut std::os::raw::c_void;
    fn abx_deserializer_create_buffer(
        data: *const u8,
        length: usize,
        error: *mut AbxError,
    ) -> *mut std::os::raw::c_void;
    fn abx_deserializer_to_file(
        deserializer: *mut std::os::raw::c_void,
        output_path: *const i8,
    ) -> AbxError;
    fn abx_deserializer_to_string(
        deserializer: *mut std::os::raw::c_void,
        out_buffer: *mut i8,
        buffer_size: usize,
    ) -> usize;
    fn abx_deserializer_free(deserializer: *mut std::os::raw::c_void);

    // High-level functions
    fn abx_convert_xml_file_to_abx_file(
        xml_path: *const i8,
        abx_path: *const i8,
        options: *const AbxOptions,
    ) -> AbxError;
    fn abx_convert_xml_string_to_abx_file(
        xml_string: *const i8,
        abx_path: *const i8,
        options: *const AbxOptions,
    ) -> AbxError;
    fn abx_convert_xml_file_to_buffer(
        xml_path: *const i8,
        out_buffer: *mut u8,
        buffer_size: usize,
        options: *const AbxOptions,
        error: *mut AbxError,
    ) -> usize;
    fn abx_convert_xml_string_to_buffer(
        xml_string: *const i8,
        out_buffer: *mut u8,
        buffer_size: usize,
        options: *const AbxOptions,
        error: *mut AbxError,
    ) -> usize;
    fn abx_convert_abx_file_to_xml_file(abx_path: *const i8, xml_path: *const i8) -> AbxError;
    fn abx_convert_abx_buffer_to_xml_file(
        abx_data: *const u8,
        length: usize,
        xml_path: *const i8,
    ) -> AbxError;
    fn abx_convert_abx_file_to_string(
        abx_path: *const i8,
        out_buffer: *mut i8,
        buffer_size: usize,
        error: *mut AbxError,
    ) -> usize;
    fn abx_convert_abx_buffer_to_string(
        abx_data: *const u8,
        length: usize,
        out_buffer: *mut i8,
        buffer_size: usize,
        error: *mut AbxError,
    ) -> usize;

    // Utilities
    fn abx_base64_encode(data: *const u8, length: usize, out: *mut i8, out_size: usize) -> usize;
    fn abx_base64_decode(encoded: *const i8, out: *mut u8, out_size: usize) -> usize;
    fn abx_hex_encode(data: *const u8, length: usize, out: *mut i8, out_size: usize) -> usize;
    fn abx_hex_decode(hex: *const i8, out: *mut u8, out_size: usize) -> usize;
}

// ============================================================================
// Error Handling
// ============================================================================

/// Exception type for ABX conversion errors.
///
/// Contains both an error code and a human-readable message retrieved
/// from the underlying C library's thread-local error storage.
#[derive(Debug)]
pub struct AbxException {
    code: AbxError,
    message: String,
}

impl AbxException {
    fn from_error(code: AbxError) -> Self {
        let message = unsafe {
            let err_ptr = abx_get_last_error();
            if err_ptr.is_null() {
                format!("ABX error: {:?}", code)
            } else {
                CStr::from_ptr(err_ptr as *const c_char)
                    .to_string_lossy()
                    .into_owned()
            }
        };

        AbxException { code, message }
    }

    /// Get the error code
    pub fn error_code(&self) -> AbxError {
        self.code
    }

    /// Get the error message
    pub fn message(&self) -> &str {
        &self.message
    }
}

impl fmt::Display for AbxException {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.message)
    }
}

impl std::error::Error for AbxException {}

/// Result type for ABX operations.
pub type Result<T> = std::result::Result<T, AbxException>;

fn check_error(code: AbxError) -> Result<()> {
    if code == AbxError::Ok {
        Ok(())
    } else {
        Err(AbxException::from_error(code))
    }
}

// ============================================================================
// Options
// ============================================================================

/// User-provided callback function for handling warnings.
///
/// Currently not used by the C API but reserved for future use.
/// The callback receives a category and message string.
pub type WarningHandler = Box<dyn FnMut(&str, &str) + Send>;

/// Options for XML to ABX conversion.
///
/// # Examples
///
/// ```
/// use android_xml_converter::Options;
///
/// // Default options
/// let opts = Options::new();
///
/// // With whitespace collapsing
/// let opts = Options::new().collapse_whitespaces(true);
/// ```
#[derive(Debug, Clone, Default)]
pub struct Options {
    /// Whether to collapse consecutive whitespace characters in text content.
    ///
    /// When enabled, sequences of whitespace (spaces, tabs, newlines) are
    /// replaced with a single space. This reduces file size but may affect
    /// formatting-sensitive content.
    pub collapse_whitespaces: bool,
}

impl Options {
    /// Create new options with default values.
    pub fn new() -> Self {
        Self::default()
    }

    /// Set whether to collapse whitespace characters.
    ///
    /// # Arguments
    ///
    /// * `value` - `true` to collapse whitespace, `false` to preserve it
    pub fn collapse_whitespaces(mut self, value: bool) -> Self {
        self.collapse_whitespaces = value;
        self
    }

    fn to_c(&self) -> AbxOptions {
        AbxOptions {
            collapse_whitespaces: if self.collapse_whitespaces { 1 } else { 0 },
        }
    }
}

// The C API has a warning callback in the header, but it's not actually
// used in the conversion functions. If it gets added to the C API, we can expose
// it through Options like this:
//
// pub struct OptionsWithCallback {
//     pub options: Options,
//     pub warning_handler: Option<WarningHandler>,
// }

// ============================================================================
// Serializer
// ============================================================================

/// A streaming serializer for manually constructing ABX documents.
///
/// The serializer provides fine-grained control over ABX document creation,
/// allowing you to build documents element by element with type-specific
/// attribute methods.
///
/// # Lifecycle
///
/// 1. Create with `create_file()` or `create_buffer()`
/// 2. Call `start_document()`
/// 3. Add elements with `start_tag()`, attributes, text, and `end_tag()`
/// 4. Call `end_document()`
/// 5. For buffer serializers, retrieve data with `get_buffer()`
///
/// # Examples
///
/// ```rust,no_run
/// use android_xml_converter::Serializer;
///
/// let mut ser = Serializer::create_buffer()?;
/// ser.start_document()?;
/// ser.start_tag("person")?;
/// ser.attribute_string("name", "Alice")?;
/// ser.attribute_int("age", 30)?;
/// ser.text("Hello, world!")?;
/// ser.end_tag("person")?;
/// ser.end_document()?;
///
/// let data = ser.get_buffer();
/// # Ok::<(), android_xml_converter::AbxException>(())
/// ```
///
/// # Thread Safety
///
/// Not thread-safe. Each thread should create its own serializer instance.
pub struct Serializer {
    handle: *mut std::os::raw::c_void,
}

impl Serializer {
    /// Create a serializer that writes to a file.
    ///
    /// The file will be created or truncated if it exists.
    ///
    /// # Arguments
    ///
    /// * `filepath` - Path to the output file
    ///
    /// # Errors
    ///
    /// Returns an error if the file cannot be created or opened for writing.
    pub fn create_file(filepath: &str) -> Result<Self> {
        let path = CString::new(filepath).map_err(|_| AbxException {
            code: AbxError::InvalidFormat,
            message: "Invalid filepath".to_string(),
        })?;

        let mut error = AbxError::Ok;
        let handle = unsafe { abx_serializer_create_file(path.as_ptr() as *const i8, &mut error) };

        if handle.is_null() {
            Err(AbxException::from_error(error))
        } else {
            Ok(Serializer { handle })
        }
    }

    /// Create a serializer that writes to an in-memory buffer.
    ///
    /// Use `get_buffer()` to retrieve the serialized data after calling
    /// `end_document()`.
    ///
    /// # Errors
    ///
    /// Returns an error if memory allocation fails.
    pub fn create_buffer() -> Result<Self> {
        let mut error = AbxError::Ok;
        let handle = unsafe { abx_serializer_create_buffer(&mut error) };

        if handle.is_null() {
            Err(AbxException::from_error(error))
        } else {
            Ok(Serializer { handle })
        }
    }

    /// Start the XML document.
    ///
    /// Must be called before any other operations. Writes the ABX magic header.
    pub fn start_document(&mut self) -> Result<()> {
        let code = unsafe { abx_serializer_start_document(self.handle) };
        check_error(code)
    }

    /// End the XML document and flush all data.
    ///
    /// Must be called after all elements have been written. After this,
    /// no more elements can be added.
    pub fn end_document(&mut self) -> Result<()> {
        let code = unsafe { abx_serializer_end_document(self.handle) };
        check_error(code)
    }

    /// Start an XML element with the given tag name.
    ///
    /// Must be matched with a corresponding `end_tag()` call with the same name.
    ///
    /// # Arguments
    ///
    /// * `name` - The tag name (e.g., "manifest", "application")
    ///
    /// # Errors
    ///
    /// Returns an error if the tag name contains null bytes.
    pub fn start_tag(&mut self, name: &str) -> Result<()> {
        let c_name = CString::new(name).map_err(|_| AbxException {
            code: AbxError::InvalidFormat,
            message: "Invalid tag name".to_string(),
        })?;

        let code = unsafe { abx_serializer_start_tag(self.handle, c_name.as_ptr() as *const i8) };
        check_error(code)
    }

    /// End an XML element with the given tag name.
    ///
    /// The tag name must match the most recent unclosed `start_tag()` call.
    ///
    /// # Arguments
    ///
    /// * `name` - The tag name (must match the opening tag)
    ///
    /// # Errors
    ///
    /// Returns `TagMismatch` if the tag name doesn't match the opening tag.
    pub fn end_tag(&mut self, name: &str) -> Result<()> {
        let c_name = CString::new(name).map_err(|_| AbxException {
            code: AbxError::InvalidFormat,
            message: "Invalid tag name".to_string(),
        })?;

        let code = unsafe { abx_serializer_end_tag(self.handle, c_name.as_ptr() as *const i8) };
        check_error(code)
    }

    /// Add a string attribute to the current element.
    ///
    /// Must be called after `start_tag()` and before any text content or child elements.
    ///
    /// # Arguments
    ///
    /// * `name` - Attribute name
    /// * `value` - Attribute value
    pub fn attribute_string(&mut self, name: &str, value: &str) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let c_value = CString::new(value).unwrap();

        let code = unsafe {
            abx_serializer_attribute_string(
                self.handle,
                c_name.as_ptr() as *const i8,
                c_value.as_ptr() as *const i8,
            )
        };
        check_error(code)
    }

    /// Add a 32-bit integer attribute (decimal representation).
    pub fn attribute_int(&mut self, name: &str, value: i32) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_int(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    /// Add a 32-bit integer attribute (hexadecimal representation).
    ///
    /// When deserialized back to XML, the value will be formatted as "0xHHHHHHHH".
    pub fn attribute_int_hex(&mut self, name: &str, value: i32) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_int_hex(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    /// Add a 64-bit integer attribute (decimal representation).
    pub fn attribute_long(&mut self, name: &str, value: i64) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_long(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    /// Add a 64-bit integer attribute (hexadecimal representation).
    pub fn attribute_long_hex(&mut self, name: &str, value: i64) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_long_hex(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    /// Add a 32-bit floating-point attribute.
    pub fn attribute_float(&mut self, name: &str, value: f32) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_float(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    /// Add a 64-bit floating-point attribute.
    pub fn attribute_double(&mut self, name: &str, value: f64) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_double(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    /// Add a boolean attribute.
    ///
    /// Will be serialized as "true" or "false" in XML.
    pub fn attribute_bool(&mut self, name: &str, value: bool) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_bool(
                self.handle,
                c_name.as_ptr() as *const i8,
                if value { 1 } else { 0 },
            )
        };
        check_error(code)
    }

    /// Add a binary attribute encoded as hexadecimal.
    ///
    /// The data will be encoded as an uppercase hexadecimal string in XML.
    ///
    /// # Arguments
    ///
    /// * `name` - Attribute name
    /// * `data` - Binary data (max 65,535 bytes)
    ///
    /// # Errors
    ///
    /// Returns an error if `data.len() > 65535`.
    pub fn attribute_bytes_hex(&mut self, name: &str, data: &[u8]) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_bytes_hex(
                self.handle,
                c_name.as_ptr() as *const i8,
                data.as_ptr(),
                data.len(),
            )
        };
        check_error(code)
    }

    /// Add a binary attribute encoded as Base64.
    ///
    /// The data will be encoded as a Base64 string in XML.
    ///
    /// # Arguments
    ///
    /// * `name` - Attribute name
    /// * `data` - Binary data (max 65,535 bytes)
    ///
    /// # Errors
    ///
    /// Returns an error if `data.len() > 65535`.
    pub fn attribute_bytes_base64(&mut self, name: &str, data: &[u8]) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_bytes_base64(
                self.handle,
                c_name.as_ptr() as *const i8,
                data.as_ptr(),
                data.len(),
            )
        };
        check_error(code)
    }

    /// Add text content to the current element.
    ///
    /// XML entities (&, <, >, ", ') will be automatically escaped.
    pub fn text(&mut self, text: &str) -> Result<()> {
        let c_text = CString::new(text).unwrap();
        let code = unsafe { abx_serializer_text(self.handle, c_text.as_ptr() as *const i8) };
        check_error(code)
    }

    /// Add a CDATA section to the current element.
    ///
    /// Content within CDATA sections is not XML-escaped.
    pub fn cdata(&mut self, text: &str) -> Result<()> {
        let c_text = CString::new(text).unwrap();
        let code = unsafe { abx_serializer_cdata(self.handle, c_text.as_ptr() as *const i8) };
        check_error(code)
    }

    /// Add an XML comment.
    ///
    /// Comments are preserved in the ABX format and will be present when
    /// deserializing back to XML.
    pub fn comment(&mut self, text: &str) -> Result<()> {
        let c_text = CString::new(text).unwrap();
        let code = unsafe { abx_serializer_comment(self.handle, c_text.as_ptr() as *const i8) };
        check_error(code)
    }

    /// Get the serialized ABX data from a buffer-based serializer.
    ///
    /// Only valid for serializers created with `create_buffer()`.
    /// Must be called after `end_document()`.
    ///
    /// # Returns
    ///
    /// A `Vec<u8>` containing the complete ABX document.
    ///
    /// # Panics
    ///
    /// Panics if called on a file-based serializer.
    pub fn get_buffer(&self) -> Vec<u8> {
        let size = unsafe { abx_serializer_get_buffer(self.handle, ptr::null_mut(), 0) };
        let mut buffer = vec![0u8; size];
        unsafe { abx_serializer_get_buffer(self.handle, buffer.as_mut_ptr(), size) };
        buffer
    }
}

impl Drop for Serializer {
    fn drop(&mut self) {
        unsafe { abx_serializer_free(self.handle) };
    }
}

// ============================================================================
// Deserializer
// ============================================================================

/// A deserializer for converting ABX format back to XML.
///
/// The deserializer reads ABX binary data and converts it to XML text format,
/// preserving all structure, attributes, and content.
///
/// # Examples
///
/// ```rust,no_run
/// use android_xml_converter::Deserializer;
///
/// // From file
/// let deser = Deserializer::from_file("input.abx")?;
/// deser.to_file("output.xml")?;
///
/// // From buffer
/// let abx_data = vec![0x41, 0x42, 0x58, 0x00, /* ... */];
/// let deser = Deserializer::from_buffer(&abx_data)?;
/// let xml_string = deser.to_string()?;
/// # Ok::<(), android_xml_converter::AbxException>(())
/// ```
///
/// # Thread Safety
///
/// Not thread-safe. Each thread should create its own deserializer instance.
pub struct Deserializer {
    handle: *mut std::os::raw::c_void,
}

impl Deserializer {
    /// Create a deserializer from an ABX file.
    ///
    /// # Arguments
    ///
    /// * `filepath` - Path to the ABX file
    ///
    /// # Errors
    ///
    /// Returns an error if:
    /// - The file cannot be opened
    /// - The file is not a valid ABX file (wrong magic header)
    pub fn from_file(filepath: &str) -> Result<Self> {
        let path = CString::new(filepath).map_err(|_| AbxException {
            code: AbxError::InvalidFormat,
            message: "Invalid filepath".to_string(),
        })?;

        let mut error = AbxError::Ok;
        let handle =
            unsafe { abx_deserializer_create_file(path.as_ptr() as *const i8, &mut error) };

        if handle.is_null() {
            Err(AbxException::from_error(error))
        } else {
            Ok(Deserializer { handle })
        }
    }

    /// Create a deserializer from an ABX buffer in memory.
    ///
    /// # Arguments
    ///
    /// * `data` - Byte slice containing ABX data
    ///
    /// # Errors
    ///
    /// Returns an error if the buffer is not a valid ABX format.
    pub fn from_buffer(data: &[u8]) -> Result<Self> {
        let mut error = AbxError::Ok;
        let handle =
            unsafe { abx_deserializer_create_buffer(data.as_ptr(), data.len(), &mut error) };

        if handle.is_null() {
            Err(AbxException::from_error(error))
        } else {
            Ok(Deserializer { handle })
        }
    }

    /// Deserialize the ABX data to an XML file.
    ///
    /// # Arguments
    ///
    /// * `output_path` - Path where the XML file should be written
    ///
    /// # Errors
    ///
    /// Returns an error if:
    /// - The output file cannot be created or written
    /// - The ABX data is corrupted or invalid
    pub fn to_file(&self, output_path: &str) -> Result<()> {
        let path = CString::new(output_path).unwrap();
        let code = unsafe { abx_deserializer_to_file(self.handle, path.as_ptr() as *const i8) };
        check_error(code)
    }

    /// Deserialize the ABX data to an XML string.
    ///
    /// # Returns
    ///
    /// A `String` containing the complete XML document.
    ///
    /// # Errors
    ///
    /// Returns an error if the ABX data is corrupted or invalid.
    pub fn to_string(&self) -> Result<String> {
        let size = unsafe { abx_deserializer_to_string(self.handle, ptr::null_mut(), 0) };
        let mut buffer = vec![0i8; size];
        unsafe { abx_deserializer_to_string(self.handle, buffer.as_mut_ptr(), size) };

        let c_str = unsafe { CStr::from_ptr(buffer.as_ptr() as *const c_char) };
        Ok(c_str.to_string_lossy().into_owned())
    }
}

impl Drop for Deserializer {
    fn drop(&mut self) {
        unsafe { abx_deserializer_free(self.handle) };
    }
}

// ============================================================================
// High-Level Conversion Functions
// ============================================================================

/// Convert an XML file directly to an ABX file.
///
/// This is the simplest way to convert XML to ABX format.
///
/// # Arguments
///
/// * `xml_path` - Path to the input XML file
/// * `abx_path` - Path where the ABX file should be written
/// * `options` - Optional conversion options (pass `None` for defaults)
///
/// # Examples
///
/// ```rust,no_run
/// use android_xml_converter::{convert_xml_file_to_abx_file, Options};
///
/// // With default options
/// convert_xml_file_to_abx_file("input.xml", "output.abx", None)?;
///
/// // With custom options
/// let opts = Options::new().collapse_whitespaces(true);
/// convert_xml_file_to_abx_file("input.xml", "output.abx", Some(opts))?;
/// # Ok::<(), android_xml_converter::AbxException>(())
/// ```
///
/// # Errors
///
/// Returns an error if:
/// - The XML file cannot be read or parsed
/// - The ABX file cannot be created or written
/// - The XML is malformed
pub fn convert_xml_file_to_abx_file(
    xml_path: &str,
    abx_path: &str,
    options: Option<Options>,
) -> Result<()> {
    let c_xml_path = CString::new(xml_path).unwrap();
    let c_abx_path = CString::new(abx_path).unwrap();

    let opts = options.unwrap_or_default().to_c();
    let code = unsafe {
        abx_convert_xml_file_to_abx_file(
            c_xml_path.as_ptr() as *const i8,
            c_abx_path.as_ptr() as *const i8,
            &opts,
        )
    };
    check_error(code)
}

/// Convert an XML string directly to an ABX file.
///
/// # Arguments
///
/// * `xml_string` - XML content as a string
/// * `abx_path` - Path where the ABX file should be written
/// * `options` - Optional conversion options
///
/// # Examples
///
/// ```rust,no_run
/// use android_xml_converter::convert_xml_string_to_abx_file;
///
/// let xml = r#"<root attr="value">Content</root>"#;
/// convert_xml_string_to_abx_file(xml, "output.abx", None)?;
/// # Ok::<(), android_xml_converter::AbxException>(())
/// ```
pub fn convert_xml_string_to_abx_file(
    xml_string: &str,
    abx_path: &str,
    options: Option<Options>,
) -> Result<()> {
    let c_xml_string = CString::new(xml_string).unwrap();
    let c_abx_path = CString::new(abx_path).unwrap();

    let opts = options.unwrap_or_default().to_c();
    let code = unsafe {
        abx_convert_xml_string_to_abx_file(
            c_xml_string.as_ptr() as *const i8,
            c_abx_path.as_ptr() as *const i8,
            &opts,
        )
    };
    check_error(code)
}

/// Convert an XML file to an ABX buffer in memory.
///
/// Useful when you need to process the ABX data further or send it over a network.
///
/// # Arguments
///
/// * `xml_path` - Path to the input XML file
/// * `options` - Optional conversion options
///
/// # Returns
///
/// A `Vec<u8>` containing the complete ABX document.
///
/// # Examples
///
/// ```rust,no_run
/// use android_xml_converter::convert_xml_file_to_buffer;
///
/// let abx_data = convert_xml_file_to_buffer("input.xml", None)?;
/// // Now you can send abx_data over the network, write it to a database, etc.
/// # Ok::<(), android_xml_converter::AbxException>(())
/// ```
pub fn convert_xml_file_to_buffer(xml_path: &str, options: Option<Options>) -> Result<Vec<u8>> {
    let c_xml_path = CString::new(xml_path).unwrap();
    let opts = options.unwrap_or_default().to_c();
    let mut error = AbxError::Ok;

    let size = unsafe {
        abx_convert_xml_file_to_buffer(
            c_xml_path.as_ptr() as *const i8,
            ptr::null_mut(),
            0,
            &opts,
            &mut error,
        )
    };
    check_error(error)?;

    let mut buffer = vec![0u8; size];
    unsafe {
        abx_convert_xml_file_to_buffer(
            c_xml_path.as_ptr() as *const i8,
            buffer.as_mut_ptr(),
            size,
            &opts,
            &mut error,
        )
    };
    check_error(error)?;

    Ok(buffer)
}

/// Convert an XML string to an ABX buffer in memory.
///
/// # Arguments
///
/// * `xml_string` - XML content as a string
/// * `options` - Optional conversion options
///
/// # Returns
///
/// A `Vec<u8>` containing the complete ABX document.
///
/// # Examples
///
/// ```rust,no_run
/// use android_xml_converter::convert_xml_string_to_buffer;
///
/// let xml = r#"<manifest package="com.example"/>"#;
/// let abx_data = convert_xml_string_to_buffer(xml, None)?;
/// assert_eq!(&abx_data[0..4], b"ABX\0"); // Magic header
/// # Ok::<(), android_xml_converter::AbxException>(())
/// ```
pub fn convert_xml_string_to_buffer(xml_string: &str, options: Option<Options>) -> Result<Vec<u8>> {
    let c_xml_string = CString::new(xml_string).unwrap();
    let opts = options.unwrap_or_default().to_c();
    let mut error = AbxError::Ok;

    let size = unsafe {
        abx_convert_xml_string_to_buffer(
            c_xml_string.as_ptr() as *const i8,
            ptr::null_mut(),
            0,
            &opts,
            &mut error,
        )
    };
    check_error(error)?;

    let mut buffer = vec![0u8; size];
    unsafe {
        abx_convert_xml_string_to_buffer(
            c_xml_string.as_ptr() as *const i8,
            buffer.as_mut_ptr(),
            size,
            &opts,
            &mut error,
        )
    };
    check_error(error)?;

    Ok(buffer)
}

/// Convert an ABX file directly to an XML file.
///
/// This is the simplest way to convert ABX back to XML format.
///
/// # Arguments
///
/// * `abx_path` - Path to the input ABX file
/// * `xml_path` - Path where the XML file should be written
///
/// # Examples
///
/// ```rust,no_run
/// use android_xml_converter::convert_abx_file_to_xml_file;
///
/// convert_abx_file_to_xml_file("input.abx", "output.xml")?;
/// # Ok::<(), android_xml_converter::AbxException>(())
/// ```
///
/// # Errors
///
/// Returns an error if:
/// - The ABX file cannot be read
/// - The ABX format is invalid or corrupted
/// - The XML file cannot be created or written
pub fn convert_abx_file_to_xml_file(abx_path: &str, xml_path: &str) -> Result<()> {
    let c_abx_path = CString::new(abx_path).unwrap();
    let c_xml_path = CString::new(xml_path).unwrap();

    let code = unsafe {
        abx_convert_abx_file_to_xml_file(
            c_abx_path.as_ptr() as *const i8,
            c_xml_path.as_ptr() as *const i8,
        )
    };
    check_error(code)
}

/// Convert an ABX buffer to an XML file.
///
/// # Arguments
///
/// * `abx_data` - Byte slice containing ABX data
/// * `xml_path` - Path where the XML file should be written
pub fn convert_abx_buffer_to_xml_file(abx_data: &[u8], xml_path: &str) -> Result<()> {
    let c_xml_path = CString::new(xml_path).unwrap();
    let code = unsafe {
        abx_convert_abx_buffer_to_xml_file(
            abx_data.as_ptr(),
            abx_data.len(),
            c_xml_path.as_ptr() as *const i8,
        )
    };
    check_error(code)
}

/// Convert an ABX file to an XML string.
///
/// # Arguments
///
/// * `abx_path` - Path to the input ABX file
///
/// # Returns
///
/// A `String` containing the complete XML document.
///
/// # Examples
///
/// ```rust,no_run
/// use android_xml_converter::convert_abx_file_to_string;
///
/// let xml_string = convert_abx_file_to_string("input.abx")?;
/// println!("XML content: {}", xml_string);
/// # Ok::<(), android_xml_converter::AbxException>(())
/// ```
pub fn convert_abx_file_to_string(abx_path: &str) -> Result<String> {
    let c_abx_path = CString::new(abx_path).unwrap();
    let mut error = AbxError::Ok;

    let size = unsafe {
        abx_convert_abx_file_to_string(
            c_abx_path.as_ptr() as *const i8,
            ptr::null_mut(),
            0,
            &mut error,
        )
    };
    check_error(error)?;

    let mut buffer = vec![0i8; size];
    unsafe {
        abx_convert_abx_file_to_string(
            c_abx_path.as_ptr() as *const i8,
            buffer.as_mut_ptr(),
            size,
            &mut error,
        )
    };
    check_error(error)?;

    let c_str = unsafe { CStr::from_ptr(buffer.as_ptr() as *const c_char) };
    Ok(c_str.to_string_lossy().into_owned())
}

/// Convert an ABX buffer to an XML string.
///
/// # Arguments
///
/// * `abx_data` - Byte slice containing ABX data
///
/// # Returns
///
/// A `String` containing the complete XML document.
///
/// # Examples
///
/// ```rust,no_run
/// use android_xml_converter::convert_abx_buffer_to_string;
///
/// let abx_data = vec![0x41, 0x42, 0x58, 0x00, /* ... */];
/// let xml_string = convert_abx_buffer_to_string(&abx_data)?;
/// # Ok::<(), android_xml_converter::AbxException>(())
/// ```
pub fn convert_abx_buffer_to_string(abx_data: &[u8]) -> Result<String> {
    let mut error = AbxError::Ok;

    let size = unsafe {
        abx_convert_abx_buffer_to_string(
            abx_data.as_ptr(),
            abx_data.len(),
            ptr::null_mut(),
            0,
            &mut error,
        )
    };
    check_error(error)?;

    let mut buffer = vec![0i8; size];
    unsafe {
        abx_convert_abx_buffer_to_string(
            abx_data.as_ptr(),
            abx_data.len(),
            buffer.as_mut_ptr(),
            size,
            &mut error,
        )
    };
    check_error(error)?;

    let c_str = unsafe { CStr::from_ptr(buffer.as_ptr() as *const c_char) };
    Ok(c_str.to_string_lossy().into_owned())
}

// ============================================================================
// Utility Functions
// ============================================================================

/// Encode binary data as a Base64 string.
///
/// # Arguments
///
/// * `data` - Binary data to encode
///
/// # Returns
///
/// A Base64-encoded string.
///
/// # Examples
///
/// ```
/// use android_xml_converter::base64_encode;
///
/// let data = b"Hello, World!";
/// let encoded = base64_encode(data);
/// assert_eq!(encoded, "SGVsbG8sIFdvcmxkIQ==");
/// ```
pub fn base64_encode(data: &[u8]) -> String {
    let size = unsafe { abx_base64_encode(data.as_ptr(), data.len(), ptr::null_mut(), 0) };
    let mut buffer = vec![0i8; size];
    unsafe { abx_base64_encode(data.as_ptr(), data.len(), buffer.as_mut_ptr(), size) };

    let c_str = unsafe { CStr::from_ptr(buffer.as_ptr() as *const c_char) };
    c_str.to_string_lossy().into_owned()
}

/// Decode a Base64 string to binary data.
///
/// # Arguments
///
/// * `encoded` - Base64-encoded string
///
/// # Returns
///
/// A `Vec<u8>` containing the decoded binary data.
///
/// # Examples
///
/// ```
/// use android_xml_converter::base64_decode;
///
/// let encoded = "SGVsbG8sIFdvcmxkIQ==";
/// let decoded = base64_decode(encoded);
/// assert_eq!(decoded, b"Hello, World!");
/// ```
pub fn base64_decode(encoded: &str) -> Vec<u8> {
    let c_encoded = CString::new(encoded).unwrap();
    let size = unsafe { abx_base64_decode(c_encoded.as_ptr() as *const i8, ptr::null_mut(), 0) };
    let mut buffer = vec![0u8; size];
    unsafe { abx_base64_decode(c_encoded.as_ptr() as *const i8, buffer.as_mut_ptr(), size) };
    buffer
}

/// Encode binary data as an uppercase hexadecimal string.
///
/// # Arguments
///
/// * `data` - Binary data to encode
///
/// # Returns
///
/// A hexadecimal string (uppercase, no "0x" prefix).
///
/// # Examples
///
/// ```
/// use android_xml_converter::hex_encode;
///
/// let data = b"Hello";
/// let encoded = hex_encode(data);
/// assert_eq!(encoded, "48656C6C6F");
/// ```
pub fn hex_encode(data: &[u8]) -> String {
    let size = unsafe { abx_hex_encode(data.as_ptr(), data.len(), ptr::null_mut(), 0) };
    let mut buffer = vec![0i8; size];
    unsafe { abx_hex_encode(data.as_ptr(), data.len(), buffer.as_mut_ptr(), size) };

    let c_str = unsafe { CStr::from_ptr(buffer.as_ptr() as *const c_char) };
    c_str.to_string_lossy().into_owned()
}

/// Decode a hexadecimal string to binary data.
///
/// # Arguments
///
/// * `hex` - Hexadecimal string (case-insensitive, no "0x" prefix)
///
/// # Returns
///
/// A `Vec<u8>` containing the decoded binary data.
///
/// # Examples
///
/// ```
/// use android_xml_converter::hex_decode;
///
/// let hex = "48656C6C6F";
/// let decoded = hex_decode(hex);
/// assert_eq!(decoded, b"Hello");
/// ```
///
/// # Panics
///
/// Panics if the hex string has an odd length or contains invalid characters.
pub fn hex_decode(hex: &str) -> Vec<u8> {
    let c_hex = CString::new(hex).unwrap();
    let size = unsafe { abx_hex_decode(c_hex.as_ptr() as *const i8, ptr::null_mut(), 0) };
    let mut buffer = vec![0u8; size];
    unsafe { abx_hex_decode(c_hex.as_ptr() as *const i8, buffer.as_mut_ptr(), size) };
    buffer
}

// ============================================================================
// Tests
// ============================================================================

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_serializer() {
        let mut ser = Serializer::create_buffer().unwrap();
        ser.start_document().unwrap();
        ser.start_tag("root").unwrap();
        ser.attribute_string("attr", "value").unwrap();
        ser.text("Hello, World!").unwrap();
        ser.end_tag("root").unwrap();
        ser.end_document().unwrap();

        let buffer = ser.get_buffer();
        assert!(!buffer.is_empty());
        assert_eq!(&buffer[0..4], b"ABX\0");
    }

    #[test]
    fn test_base64() {
        let data = b"Hello, World!";
        let encoded = base64_encode(data);
        let decoded = base64_decode(&encoded);
        assert_eq!(data, decoded.as_slice());
    }

    #[test]
    fn test_hex() {
        let data = b"Hello";
        let encoded = hex_encode(data);
        let decoded = hex_decode(&encoded);
        assert_eq!(data, decoded.as_slice());
    }

    #[test]
    fn test_round_trip() {
        let xml = r#"<root attr="value">Hello</root>"#;
        let abx_data = convert_xml_string_to_buffer(xml, None).unwrap();
        let xml_output = convert_abx_buffer_to_string(&abx_data).unwrap();
        assert!(xml_output.contains("root"));
        assert!(xml_output.contains("attr"));
        assert!(xml_output.contains("value"));
    }
}