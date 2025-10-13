use std::ffi::{CStr, CString};
use std::os::raw::c_char;
use std::fmt;
use std::ptr;

// ============================================================================
// FFI Bindings
// ============================================================================

#[repr(C)]
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum AbxError {
    Ok = 0,
    NullPointer = -1,
    InvalidHandle = -2,
    FileNotFound = -3,
    ParseFailed = -4,
    WriteFailed = -5,
    InvalidFormat = -6,
    BufferTooSmall = -7,
    TagMismatch = -8,
    OutOfMemory = -9,
    Unknown = -100,
}

#[repr(C)]
pub struct AbxOptions {
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

/// User-provided callback function for handling warnings
pub type WarningHandler = Box<dyn FnMut(&str, &str) + Send>;

#[derive(Debug, Clone, Default)]
pub struct Options {
    pub collapse_whitespaces: bool,
}

impl Options {
    pub fn new() -> Self {
        Self::default()
    }

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

pub struct Serializer {
    handle: *mut std::os::raw::c_void,
}

impl Serializer {
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

    pub fn create_buffer() -> Result<Self> {
        let mut error = AbxError::Ok;
        let handle = unsafe { abx_serializer_create_buffer(&mut error) };

        if handle.is_null() {
            Err(AbxException::from_error(error))
        } else {
            Ok(Serializer { handle })
        }
    }

    pub fn start_document(&mut self) -> Result<()> {
        let code = unsafe { abx_serializer_start_document(self.handle) };
        check_error(code)
    }

    pub fn end_document(&mut self) -> Result<()> {
        let code = unsafe { abx_serializer_end_document(self.handle) };
        check_error(code)
    }

    pub fn start_tag(&mut self, name: &str) -> Result<()> {
        let c_name = CString::new(name).map_err(|_| AbxException {
            code: AbxError::InvalidFormat,
            message: "Invalid tag name".to_string(),
        })?;

        let code = unsafe { abx_serializer_start_tag(self.handle, c_name.as_ptr() as *const i8) };
        check_error(code)
    }

    pub fn end_tag(&mut self, name: &str) -> Result<()> {
        let c_name = CString::new(name).map_err(|_| AbxException {
            code: AbxError::InvalidFormat,
            message: "Invalid tag name".to_string(),
        })?;

        let code = unsafe { abx_serializer_end_tag(self.handle, c_name.as_ptr() as *const i8) };
        check_error(code)
    }

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

    pub fn attribute_int(&mut self, name: &str, value: i32) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_int(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    pub fn attribute_int_hex(&mut self, name: &str, value: i32) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_int_hex(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    pub fn attribute_long(&mut self, name: &str, value: i64) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_long(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    pub fn attribute_long_hex(&mut self, name: &str, value: i64) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_long_hex(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    pub fn attribute_float(&mut self, name: &str, value: f32) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_float(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

    pub fn attribute_double(&mut self, name: &str, value: f64) -> Result<()> {
        let c_name = CString::new(name).unwrap();
        let code = unsafe {
            abx_serializer_attribute_double(self.handle, c_name.as_ptr() as *const i8, value)
        };
        check_error(code)
    }

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

    pub fn text(&mut self, text: &str) -> Result<()> {
        let c_text = CString::new(text).unwrap();
        let code = unsafe { abx_serializer_text(self.handle, c_text.as_ptr() as *const i8) };
        check_error(code)
    }

    pub fn cdata(&mut self, text: &str) -> Result<()> {
        let c_text = CString::new(text).unwrap();
        let code = unsafe { abx_serializer_cdata(self.handle, c_text.as_ptr() as *const i8) };
        check_error(code)
    }

    pub fn comment(&mut self, text: &str) -> Result<()> {
        let c_text = CString::new(text).unwrap();
        let code = unsafe { abx_serializer_comment(self.handle, c_text.as_ptr() as *const i8) };
        check_error(code)
    }

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

pub struct Deserializer {
    handle: *mut std::os::raw::c_void,
}

impl Deserializer {
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

    pub fn to_file(&self, output_path: &str) -> Result<()> {
        let path = CString::new(output_path).unwrap();
        let code = unsafe { abx_deserializer_to_file(self.handle, path.as_ptr() as *const i8) };
        check_error(code)
    }

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

pub fn base64_encode(data: &[u8]) -> String {
    let size = unsafe { abx_base64_encode(data.as_ptr(), data.len(), ptr::null_mut(), 0) };
    let mut buffer = vec![0i8; size];
    unsafe { abx_base64_encode(data.as_ptr(), data.len(), buffer.as_mut_ptr(), size) };

    let c_str = unsafe { CStr::from_ptr(buffer.as_ptr() as *const c_char) };
    c_str.to_string_lossy().into_owned()
}

pub fn base64_decode(encoded: &str) -> Vec<u8> {
    let c_encoded = CString::new(encoded).unwrap();
    let size = unsafe { abx_base64_decode(c_encoded.as_ptr() as *const i8, ptr::null_mut(), 0) };
    let mut buffer = vec![0u8; size];
    unsafe { abx_base64_decode(c_encoded.as_ptr() as *const i8, buffer.as_mut_ptr(), size) };
    buffer
}

pub fn hex_encode(data: &[u8]) -> String {
    let size = unsafe { abx_hex_encode(data.as_ptr(), data.len(), ptr::null_mut(), 0) };
    let mut buffer = vec![0i8; size];
    unsafe { abx_hex_encode(data.as_ptr(), data.len(), buffer.as_mut_ptr(), size) };

    let c_str = unsafe { CStr::from_ptr(buffer.as_ptr() as *const c_char) };
    c_str.to_string_lossy().into_owned()
}

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
}
