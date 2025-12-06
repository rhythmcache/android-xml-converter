# Tests Directory

This directory contains some tests

## Files

### Test Scripts

- **`test.py`** - test runner
  - Checks dependencies (Python, Cargo, Meson)
  - Compiles both Rust and C++ implementations
  - Runs roundtrip conversion tests
  - Reports test results

- **`gen_xml.py`** - Random XML generator
  - Generates test XML files of varying sizes (1-2 MB default)

- **`diff.py`** - Semantic XML comparator
  - Compares two XML files for semantic differences
  - Ignores formatting (whitespace, indentation, etc.)
  - Reports actual structural/content differences only

- **`benchmark.py`**
   - Benches both the versions

## Test Logic

The test follows a **roundtrip conversion** approach:

### Roundtrip Test Flow

```
1. Generate Random XML
   └─> Creates a test XML file (~1-2 MB) with random structure and content

2. XML -> ABX Conversion
   └─> Converts the XML file to Android Binary XML (ABX) format
   └─> Using: xml2abx test_input.xml temp.abx

3. ABX -> XML Conversion
   └─> Converts the ABX file back to XML format
   └─> Using: abx2xml temp.abx output.xml

4. Compare XMLs
   └─> Semantically compares original XML with roundtrip output
   └─> Using: diff.py test_input.xml output.xml
```   

- Expected Results

```cmd
============================================================
Android XML Converter - Automated Test Suite
============================================================

ℹ Generating test XML: test_input.xml
✓ Generated test_input.xml (3.81 MB)

============================================================
Compiling Rust Implementation
============================================================

ℹ Running: cargo build --release
✓ Rust compilation successful

============================================================
Testing Rust Roundtrip
============================================================

ℹ Converting XML to ABX: ../target/release/xml2abx
✓ XML to ABX conversion successful
ℹ Converting ABX back to XML: ../target/release/abx2xml
✓ ABX to XML conversion successful
ℹ Comparing original and roundtrip XML files
✓ Rust roundtrip test PASSED

============================================================
Compiling C++ Implementation
============================================================

ℹ Setting up meson build directory
ℹ Running: meson compile -C builddir
✓ C++ compilation successful

============================================================
Testing C++ Roundtrip
============================================================

ℹ Converting XML to ABX: ../builddir/xml2abx
✓ XML to ABX conversion successful
ℹ Converting ABX back to XML: ../builddir/abx2xml
✓ ABX to XML conversion successful
ℹ Comparing original and roundtrip XML files
✓ C++ roundtrip test PASSED

============================================================
Test Summary
============================================================

Rust Compilation: ✓ PASSED
Rust Roundtrip:   ✓ PASSED
C++ Compilation:  ✓ PASSED
C++ Roundtrip:    ✓ PASSED

Time elapsed: 57.53s

✓ All tests PASSED! ✓
```

## Usage

### Run Test

```bash
python test.py
```

## Requirements

- **Python**
- **Working C++ and Rust compilers**
- **Cargo**

- **Meson**

