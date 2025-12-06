import subprocess
import sys
import os
import time
from pathlib import Path

if os.name == 'nt':
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)

class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    BOLD = '\033[1m'
    END = '\033[0m'

def print_header(text):
    print(f"\n{Colors.BOLD}{Colors.BLUE}{'='*60}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.BLUE}{text}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.BLUE}{'='*60}{Colors.END}\n")

def print_success(text):
    print(f"{Colors.GREEN}✓ {text}{Colors.END}")

def print_error(text):
    print(f"{Colors.RED}✗ {text}{Colors.END}")

def print_info(text):
    print(f"{Colors.YELLOW}ℹ {text}{Colors.END}")

def run_command(cmd, cwd=None, check=True):
    """Run a command and return success status."""
    try:
        result = subprocess.run(
            cmd,
            cwd=cwd,
            capture_output=True,
            text=True,
            shell=True,
            check=check
        )
        return True, result.stdout, result.stderr
    except subprocess.CalledProcessError as e:
        return False, e.stdout, e.stderr

def check_command_exists(cmd):
    """Check if a command exists in PATH."""
    try:
        result = subprocess.run(
            f"{'where' if os.name == 'nt' else 'which'} {cmd}",
            shell=True,
            capture_output=True,
            text=True
        )
        return result.returncode == 0
    except:
        return False

def get_python_command():
    """Get the available Python command."""
    if check_command_exists("python3"):
        return "python3"
    elif check_command_exists("python"):
        return "python"
    else:
        return None

def check_dependencies():
    """Check if all required dependencies are available."""
    print_header("Checking Dependencies")
    
    missing = []
    
    python_cmd = get_python_command()
    if python_cmd:
        print_success(f"Python: {python_cmd}")
    else:
        print_error("Python: Not found")
        missing.append("python/python3")
    
    
    if check_command_exists("cargo"):
        print_success("Cargo: Found")
    else:
        print_error("Cargo: Not found")
        missing.append("cargo")
    
    
    if check_command_exists("meson"):
        print_success("Meson: Found")
    else:
        print_error("Meson: Not found")
        missing.append("meson")
    
    if missing:
        print_error(f"\nMissing dependencies: {', '.join(missing)}")
        print_info("Please install missing dependencies and try again")
        return False, None
    
    return True, python_cmd

def compile_rust():
    """Compile Rust version."""
    print_header("Compiling Rust Implementation")
    print_info("Running: cargo build --release")
    
    success, stdout, stderr = run_command("cargo build --release", cwd="..")
    
    if success:
        print_success("Rust compilation successful")
        return True
    else:
        print_error("Rust compilation failed")
        print(stderr)
        return False

def compile_cpp():
    """Compile C++ version using meson."""
    print_header("Compiling C++ Implementation")
    
    
    if not os.path.exists("../builddir"):
        print_info("Setting up meson build directory")
        success, stdout_setup, stderr_setup = run_command("meson setup builddir", cwd="..")
        if not success:
            print_error("Meson setup failed")
            if stderr_setup:
                print(f"\nStderr:\n{stderr_setup}")
            if stdout_setup:
                print(f"\nStdout:\n{stdout_setup}")
            return False
    
    print_info("Running: meson compile -C builddir")
    success, stdout, stderr = run_command("meson compile -C builddir", cwd="..")
    
    if success:
        print_success("C++ compilation successful")
        return True
    else:
        print_error("C++ compilation failed")
        if stderr:
            print(f"\nStderr:\n{stderr}")
        if stdout:
            print(f"\nStdout:\n{stdout}")
        return False

def generate_test_xml(filename, python_cmd, size_mb=1):
    """Generate a test XML file."""
    print_info(f"Generating test XML: {filename}")
    
    success, stdout, stderr = run_command(f"{python_cmd} gen_xml.py random 1")
    
    if not success:
        print_error(f"Failed to run gen_xml.py: {stderr}")
        return False
    
    if success and os.path.exists("test_1.xml"):
        
        if os.path.exists(filename):
            os.remove(filename)
        os.rename("test_1.xml", filename)
        
        size = os.path.getsize(filename) / (1024 * 1024)
        print_success(f"Generated {filename} ({size:.2f} MB)")
        return True
    else:
        print_error(f"Failed to generate {filename}")
        if stderr:
            print(f"Error: {stderr}")
        if stdout:
            print(f"Output: {stdout}")
        return False

def test_roundtrip(impl_name, xml2abx_cmd, abx2xml_cmd, test_file, python_cmd):
    """Test roundtrip conversion for an implementation."""
    print_header(f"Testing {impl_name} Roundtrip")
    
    input_xml = test_file
    intermediate_abx = "temp.abx"
    output_xml = "output.xml"
    
    
    xml2abx_abs = os.path.abspath(xml2abx_cmd)
    abx2xml_abs = os.path.abspath(abx2xml_cmd)
    
    
    print_info(f"Converting XML to ABX: {xml2abx_cmd}")
    success, _, stderr = run_command(f'"{xml2abx_abs}" {input_xml} {intermediate_abx}')
    
    if not success:
        print_error("XML to ABX conversion failed")
        print(stderr)
        return False
    
    if not os.path.exists(intermediate_abx):
        print_error(f"ABX file not created: {intermediate_abx}")
        return False
    
    print_success("XML to ABX conversion successful")
    
    
    print_info(f"Converting ABX back to XML: {abx2xml_cmd}")
    
    success, _, stderr = run_command(f'"{abx2xml_abs}" {intermediate_abx} {output_xml}')
    
    if not success:
        print_error("ABX to XML conversion failed")
        print(stderr)
        return False
    
    if not os.path.exists(output_xml):
        print_error(f"Output XML file not created: {output_xml}")
        return False
    
    print_success("ABX to XML conversion successful")
    
    
    print_info("Comparing original and roundtrip XML files")
    success, stdout, stderr = run_command(f"{python_cmd} diff.py {input_xml} {output_xml}", check=False)
    
    if "semantically identical" in stdout.lower():
        print_success(f"{impl_name} roundtrip test PASSED")
        return True
    else:
        print_error(f"{impl_name} roundtrip test FAILED")
        print("Differences found:")
        print(stdout)
        return False

def cleanup_test_files():
    """Clean up temporary test files."""
    temp_files = ["temp.abx", "output.xml", "test_input.xml"]
    
    for f in temp_files:
        if os.path.exists(f):
            os.remove(f)

def main():
    start_time = time.time()
    results = {}
    
    print_header("Android XML Converter Tests")
    
    
    if not os.path.exists("gen_xml.py") or not os.path.exists("diff.py"):
        print_error("Please run this script from the tests/ directory")
        sys.exit(1)
    
    
    if not os.path.exists("../Cargo.toml"):
        print_error("Cannot find project root (../Cargo.toml not found)")
        sys.exit(1)
    
    deps_ok, python_cmd = check_dependencies()
    if not deps_ok:
        sys.exit(1)
    
    test_file = "test_input.xml"
    if not generate_test_xml(test_file, python_cmd):
        print_error("Failed to generate test XML")
        sys.exit(1)
    
    
    results['rust_compile'] = compile_rust()
    if not results['rust_compile']:
        print_error("Skipping Rust tests due to compilation failure")
        results['rust_test'] = False
    else:
        
        rust_xml2abx = "../target/release/xml2abx.exe" if os.name == 'nt' else "../target/release/xml2abx"
        rust_abx2xml = "../target/release/abx2xml.exe" if os.name == 'nt' else "../target/release/abx2xml"
        results['rust_test'] = test_roundtrip("Rust", rust_xml2abx, rust_abx2xml, test_file, python_cmd)
    
    
    results['cpp_compile'] = compile_cpp()
    if not results['cpp_compile']:
        print_error("Skipping C++ tests due to compilation failure")
        results['cpp_test'] = False
    else:
        
        cpp_xml2abx = "../builddir/xml2abx.exe" if os.name == 'nt' else "../builddir/xml2abx"
        cpp_abx2xml = "../builddir/abx2xml.exe" if os.name == 'nt' else "../builddir/abx2xml"
        results['cpp_test'] = test_roundtrip("C++", cpp_xml2abx, cpp_abx2xml, test_file, python_cmd)
    
    
    cleanup_test_files()
    
    
    elapsed = time.time() - start_time
    print_header("Test Summary")
    
    print(f"Rust Compilation: ", end="")
    print_success("PASSED") if results['rust_compile'] else print_error("FAILED")
    
    print(f"Rust Roundtrip:   ", end="")
    print_success("PASSED") if results['rust_test'] else print_error("FAILED")
    
    print(f"C++ Compilation:  ", end="")
    print_success("PASSED") if results['cpp_compile'] else print_error("FAILED")
    
    print(f"C++ Roundtrip:    ", end="")
    print_success("PASSED") if results['cpp_test'] else print_error("FAILED")
    
    print(f"\n{Colors.BOLD}Time elapsed: {elapsed:.2f}s{Colors.END}\n")
    
    
    all_passed = all(results.values())
    if all_passed:
        print_success("All tests PASSED! ")
        sys.exit(0)
    else:
        print_error("Some tests FAILED! ")
        sys.exit(1)

if __name__ == "__main__":
    main()