use std::io;
use thiserror::Error;

#[derive(Error, Debug)]
pub enum ConversionError {
    #[error("IO error: {0}")]
    Io(#[from] io::Error),

    #[error(
        "Invalid ABX file format - magic header mismatch. Expected: {expected:02X?}, got: {actual:02X?}"
    )]
    InvalidMagicHeader { expected: [u8; 4], actual: [u8; 4] },

    #[error("Failed to read {0} from stream")]
    ReadError(String),

    #[error("Invalid interned string index: {0}")]
    InvalidInternedStringIndex(u16),

    #[error("Unknown attribute type: {0}")]
    UnknownAttributeType(u8),

    #[error("Parse error: {0}")]
    ParseError(String),

    #[error("XML parsing failed: {0}")]
    XmlParsing(String),

    #[error("String too long: {0} bytes (max: {1})")]
    StringTooLong(usize, usize),

    #[error("Binary data too long: {0} bytes (max: {1})")]
    BinaryDataTooLong(usize, usize),

    #[error("Invalid hex string")]
    InvalidHex,

    #[error("Invalid base64 string")]
    InvalidBase64,

    #[error("UTF-8 conversion error: {0}")]
    Utf8Error(#[from] std::str::Utf8Error),
}

// convert quick_xml errors
impl From<quick_xml::Error> for ConversionError {
    fn from(err: quick_xml::Error) -> Self {
        ConversionError::XmlParsing(err.to_string())
    }
}

impl From<quick_xml::events::attributes::AttrError> for ConversionError {
    fn from(err: quick_xml::events::attributes::AttrError) -> Self {
        ConversionError::XmlParsing(err.to_string())
    }
}

pub type Result<T> = std::result::Result<T, ConversionError>;

// ============================================================================
// Protocol Constants
// ============================================================================

/// Magic header for ABX format version 0
pub const PROTOCOL_MAGIC_VERSION_0: [u8; 4] = [0x41, 0x42, 0x58, 0x00];

// Token types (lower 4 bits)
pub const START_DOCUMENT: u8 = 0;
pub const END_DOCUMENT: u8 = 1;
pub const START_TAG: u8 = 2;
pub const END_TAG: u8 = 3;
pub const TEXT: u8 = 4;
pub const CDSECT: u8 = 5;
pub const ENTITY_REF: u8 = 6;
pub const IGNORABLE_WHITESPACE: u8 = 7;
pub const PROCESSING_INSTRUCTION: u8 = 8;
pub const COMMENT: u8 = 9;
pub const DOCDECL: u8 = 10;
pub const ATTRIBUTE: u8 = 15;

// Type information (upper 4 bits)
pub const TYPE_NULL: u8 = 1 << 4;
pub const TYPE_STRING: u8 = 2 << 4;
pub const TYPE_STRING_INTERNED: u8 = 3 << 4;
pub const TYPE_BYTES_HEX: u8 = 4 << 4;
pub const TYPE_BYTES_BASE64: u8 = 5 << 4;
pub const TYPE_INT: u8 = 6 << 4;
pub const TYPE_INT_HEX: u8 = 7 << 4;
pub const TYPE_LONG: u8 = 8 << 4;
pub const TYPE_LONG_HEX: u8 = 9 << 4;
pub const TYPE_FLOAT: u8 = 10 << 4;
pub const TYPE_DOUBLE: u8 = 11 << 4;
pub const TYPE_BOOLEAN_TRUE: u8 = 12 << 4;
pub const TYPE_BOOLEAN_FALSE: u8 = 13 << 4;

// ============================================================================
// Shared Utilities
// ============================================================================

/// Maximum value for a u16 (used for length limits)
pub const MAX_UNSIGNED_SHORT: u16 = 65535;

/// Special marker indicating a new interned string follows
pub const INTERNED_STRING_NEW_MARKER: u16 = 0xFFFF;

/// Initial capacity for string pool
pub const INITIAL_STRING_POOL_CAPACITY: usize = 64;

/// Initial capacity for XML event buffer
pub const INITIAL_EVENT_BUFFER_CAPACITY: usize = 8192;

#[inline]
pub fn encode_xml_entities(text: &str) -> std::borrow::Cow<'_, str> {
    // Fast path: check if escaping is needed
    if !text
        .bytes()
        .any(|b| matches!(b, b'&' | b'<' | b'>' | b'"' | b'\''))
    {
        return std::borrow::Cow::Borrowed(text);
    }

    let mut result = String::with_capacity(text.len() + 16);
    for ch in text.chars() {
        match ch {
            '&' => result.push_str("&amp;"),
            '<' => result.push_str("&lt;"),
            '>' => result.push_str("&gt;"),
            '"' => result.push_str("&quot;"),
            '\'' => result.push_str("&apos;"),
            _ => result.push(ch),
        }
    }
    std::borrow::Cow::Owned(result)
}

/// Shows a warning message for unsupported XML features
#[inline]
pub fn show_warning(feature: &str, details: Option<&str>) {
    eprintln!("WARNING: {} is not supported and might be lost.", feature);
    if let Some(details) = details {
        eprintln!("  {}", details);
    }
}

// ============================================================================
// Type Detection Utilities
// ============================================================================

pub mod type_detection {
    /// checks if a string represents a boolean value
    #[inline]
    pub fn is_boolean(s: &str) -> bool {
        matches!(s, "true" | "false")
    }

    /// Checks if a string contains only whitespace
    #[inline]
    pub fn is_whitespace_only(s: &str) -> bool {
        s.bytes().all(|b| matches!(b, b' ' | b'\t' | b'\n' | b'\r'))
    }
}
