#!/usr/bin/env python3
"""
Compares Rust vs C++ implementations
"""
import subprocess
import sys
import os
import json
import platform
import psutil
from pathlib import Path
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from datetime import datetime

if sys.platform == 'win32':
    import ctypes
    kernel32 = ctypes.windll.kernel32
    kernel32.SetConsoleMode(kernel32.GetStdHandle(-11), 7)

class Colors:
    GREEN = '\033[92m'
    RED = '\033[91m'
    YELLOW = '\033[93m'
    BLUE = '\033[94m'
    CYAN = '\033[96m'
    BOLD = '\033[1m'
    END = '\033[0m'

def print_header(text):
    print(f"\n{Colors.BOLD}{Colors.BLUE}{'='*70}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.BLUE}{text:^70}{Colors.END}")
    print(f"{Colors.BOLD}{Colors.BLUE}{'='*70}{Colors.END}\n")

def get_system_info():
    """get system specifications."""
    print_header("System Specifications")
    
    info = {
        'os': platform.system(),
        'os_version': platform.version(),
        'architecture': platform.machine(),
        'processor': platform.processor() or 'Unknown',
        'cpu_cores_physical': psutil.cpu_count(logical=False),
        'cpu_cores_logical': psutil.cpu_count(logical=True),
        'ram_total_gb': round(psutil.virtual_memory().total / (1024**3), 2),
        'ram_available_gb': round(psutil.virtual_memory().available / (1024**3), 2),
    }
    
    # get disk info
    disk = psutil.disk_usage('/')
    info['disk_total_gb'] = round(disk.total / (1024**3), 2)
    info['disk_free_gb'] = round(disk.free / (1024**3), 2)
    print(f"{Colors.CYAN}OS:{Colors.END} {info['os']} {info['architecture']}")
    print(f"{Colors.CYAN}Processor:{Colors.END} {info['processor']}")
    print(f"{Colors.CYAN}CPU Cores:{Colors.END} {info['cpu_cores_physical']} physical, {info['cpu_cores_logical']} logical")
    print(f"{Colors.CYAN}RAM:{Colors.END} {info['ram_total_gb']} GB total, {info['ram_available_gb']} GB available")
    print(f"{Colors.CYAN}Disk:{Colors.END} {info['disk_total_gb']} GB total, {info['disk_free_gb']} GB free")
    
    return info

def check_binaries():
    """Check if both implementations exist."""
    is_windows = os.name == 'nt'
    ext = '.exe' if is_windows else ''
    
    binaries = {
        'rust_xml2abx': f"../target/release/xml2abx{ext}",
        'rust_abx2xml': f"../target/release/abx2xml{ext}",
        'cpp_xml2abx': f"../builddir/xml2abx{ext}",
        'cpp_abx2xml': f"../builddir/abx2xml{ext}",
    }
    
    for name, path in binaries.items():
        if not os.path.exists(path):
            print(f"{Colors.RED}✗ Missing: {name} at {path}{Colors.END}")
            return None
    
    print(f"{Colors.GREEN}✓ All binaries found{Colors.END}")
    return binaries

def generate_test_file(size_mb=1):
    """Generate a test XML file."""
    filename = f"bench_test_{size_mb}mb.xml"
    
    python_cmd = "python3" if subprocess.run("python3 --version", shell=True, capture_output=True).returncode == 0 else "python"
    
    result = subprocess.run(
        f"{python_cmd} gen_xml.py random 1",
        shell=True,
        capture_output=True
    )
    
    if result.returncode == 0 and os.path.exists("test_1.xml"):
        if os.path.exists(filename):
            os.remove(filename)
        os.rename("test_1.xml", filename)
        return filename
    
    return None

def run_hyperfine(name, commands, warmup=3, runs=10):
    """Run hyperfine benchmark and return results."""
    print(f"\n{Colors.CYAN}Running: {name}{Colors.END}")
    
    cmd = [
        'hyperfine',
        '--warmup', str(warmup),
        '--runs', str(runs),
        '--export-json', 'temp_results.json',
    ]
    
    for command in commands:
        cmd.append(command)
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    
    if result.returncode != 0:
        print(f"{Colors.RED}Error running hyperfine{Colors.END}")
        return None
    
    # Parse results
    with open('temp_results.json', 'r') as f:
        data = json.load(f)
    
    os.remove('temp_results.json')
    return data['results']

def create_benchmark_chart(results, system_info, output_file='benchmark_results.png'):
    
    plt.style.use('seaborn-v0_8-whitegrid')
    
    fig, ax = plt.subplots(figsize=(14, 8))
    
    rust_xml2abx_mean = results['xml2abx']['rust']['mean'] * 1000  # convert to ms
    cpp_xml2abx_mean = results['xml2abx']['cpp']['mean'] * 1000
    rust_xml2abx_std = results['xml2abx']['rust']['stddev'] * 1000
    cpp_xml2abx_std = results['xml2abx']['cpp']['stddev'] * 1000
    
    rust_abx2xml_mean = results['abx2xml']['rust']['mean'] * 1000
    cpp_abx2xml_mean = results['abx2xml']['cpp']['mean'] * 1000
    rust_abx2xml_std = results['abx2xml']['rust']['stddev'] * 1000
    cpp_abx2xml_std = results['abx2xml']['cpp']['stddev'] * 1000
    
    # setup bar chart
  #  x = [0, 1, 3.5, 4.5]
    x = [0, 1, 2, 3]
    heights = [rust_xml2abx_mean, cpp_xml2abx_mean, rust_abx2xml_mean, cpp_abx2xml_mean]
    errors = [rust_xml2abx_std, cpp_xml2abx_std, rust_abx2xml_std, cpp_abx2xml_std]
    colors = ['#E74C3C', '#3498DB', '#E74C3C', '#3498DB']  # rust=red, C++=blue
    labels = ['Rust\nxml2abx', 'C++\nxml2abx', 'Rust\nabx2xml', 'C++\nabx2xml']
    
    bars = ax.bar(x, heights, width=0.8, color=colors, alpha=0.85, 
                  edgecolor='black', linewidth=2)
    
    ax.set_ylabel('Time (milliseconds)', fontsize=14, fontweight='bold', labelpad=10)
    ax.set_title('XML Converter Performance Benchmark', fontsize=18, fontweight='bold', pad=20)
    ax.set_xticks(x)
    ax.set_xticklabels(labels, fontsize=13, fontweight='bold')
    ax.grid(axis='y', alpha=0.3, linewidth=1)
    ax.set_axisbelow(True)
    
    for bar, height in zip(bars, heights):
        ax.text(bar.get_x() + bar.get_width()/2, height + max(heights)*0.02,
                f'{height:.2f}ms', ha='center', va='bottom', 
                fontweight='bold', fontsize=11,
                bbox=dict(boxstyle='round,pad=0.3', facecolor='white', 
                         edgecolor='black', linewidth=1, alpha=0.9))
    
    # calculate winners and speedups
    speedup_xml2abx = max(rust_xml2abx_mean, cpp_xml2abx_mean) / min(rust_xml2abx_mean, cpp_xml2abx_mean)
    winner_xml2abx = 'Rust' if rust_xml2abx_mean < cpp_xml2abx_mean else 'C++'
    
    speedup_abx2xml = max(rust_abx2xml_mean, cpp_abx2xml_mean) / min(rust_abx2xml_mean, cpp_abx2xml_mean)
    winner_abx2xml = 'Rust' if rust_abx2xml_mean < cpp_abx2xml_mean else 'C++'
    
    y_pos = max(rust_xml2abx_mean, cpp_xml2abx_mean) * 0.85
    ax.annotate(f'{winner_xml2abx}\n{speedup_xml2abx:.2f}x faster', 
                xy=(0.5, y_pos), fontsize=11, fontweight='bold',
                ha='center', va='center',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='#FFD700', alpha=0.8, edgecolor='black', linewidth=1.5))
    
    y_pos2 = max(rust_abx2xml_mean, cpp_abx2xml_mean) * 0.85
    ax.annotate(f'{winner_abx2xml}\n{speedup_abx2xml:.2f}x faster', 
                xy=(2.5, y_pos2), fontsize=11, fontweight='bold',
                ha='center', va='center',
                bbox=dict(boxstyle='round,pad=0.5', facecolor='#FFD700', alpha=0.8, edgecolor='black', linewidth=1.5))

    rust_patch = mpatches.Patch(color='#E74C3C', label='Rust', alpha=0.85)
    cpp_patch = mpatches.Patch(color='#3498DB', label='C++', alpha=0.85)
    # ax.legend(handles=[rust_patch, cpp_patch], loc='upper right', fontsize=13, 
              #frameon=True, shadow=True, fancybox=True)
    
    specs_text = (f"System Specifications: OS: {system_info['os']} {system_info['os_version']} | "
                  f"Architecture: {system_info['architecture']} | Processor: {system_info['processor']} | "
                  f"CPU Cores: {system_info['cpu_cores_physical']} Physical, {system_info['cpu_cores_logical']} Logical | "
                  f"RAM: {system_info['ram_total_gb']} GB Total, {system_info['ram_available_gb']} GB Available | "
                  f"Disk: {system_info['disk_total_gb']} GB Total, {system_info['disk_free_gb']} GB Free | "
                  f"Benchmark Date: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    fig.text(0.5, 0.01, specs_text, ha='center', va='bottom',
             fontsize=7.5, color='#555555', style='italic', wrap=True)
    
    plt.tight_layout(rect=[0, 0.03, 1, 0.98])
    
    plt.savefig(output_file, dpi=300, bbox_inches='tight', facecolor='white')
    print(f"\n{Colors.GREEN}✓ Chart saved: {output_file}{Colors.END}")
    
    return output_file

def main():
    if not os.path.exists("gen_xml.py"):
        print(f"{Colors.RED}Error: Run from tests/ directory{Colors.END}")
        sys.exit(1)
    
    system_info = get_system_info()
    
    print_header("Checking Binaries")
    binaries = check_binaries()
    if not binaries:
        sys.exit(1)


    print_header("Generating Test File")
    test_file = generate_test_file(1)
    if not test_file:
        print(f"{Colors.RED}Failed to generate test file{Colors.END}")
        sys.exit(1)
    
    file_size = os.path.getsize(test_file) / (1024 * 1024)
    print(f"{Colors.GREEN}✓ Generated {test_file} ({file_size:.2f} MB){Colors.END}")
    
    # bench xml2abx
    print_header("Benchmarking xml2abx")
    
    rust_cmd = f'{os.path.abspath(binaries["rust_xml2abx"])} {test_file} temp_rust.abx'
    cpp_cmd = f'{os.path.abspath(binaries["cpp_xml2abx"])} {test_file} temp_cpp.abx'
    
    xml2abx_results = run_hyperfine("xml2abx", [rust_cmd, cpp_cmd])
    
    if not xml2abx_results:
        sys.exit(1)
    
    # generate ABX for reverse test
    subprocess.run(rust_cmd, shell=True, capture_output=True)
    
    # bench abx2xml
    print_header("Benchmarking abx2xml")
    
    rust_cmd = f'{os.path.abspath(binaries["rust_abx2xml"])} temp_rust.abx temp_rust.xml'
    cpp_cmd = f'{os.path.abspath(binaries["cpp_abx2xml"])} temp_rust.abx temp_cpp.xml'
    
    abx2xml_results = run_hyperfine("abx2xml", [rust_cmd, cpp_cmd])
    
    if not abx2xml_results:
        sys.exit(1)
    
    # results for chart
    results = {
        'xml2abx': {
            'rust': xml2abx_results[0],
            'cpp': xml2abx_results[1]
        },
        'abx2xml': {
            'rust': abx2xml_results[0],
            'cpp': abx2xml_results[1]
        }
    }
    
    # create visualization
    print_header("Creating Benchmark Chart")
    chart_file = create_benchmark_chart(results, system_info)
    
    # print summary
    print_header("Summary")
    
    rust_xml2abx = results['xml2abx']['rust']['mean'] * 1000
    cpp_xml2abx = results['xml2abx']['cpp']['mean'] * 1000
    rust_abx2xml = results['abx2xml']['rust']['mean'] * 1000
    cpp_abx2xml = results['abx2xml']['cpp']['mean'] * 1000
    
    print(f"{Colors.CYAN}xml2abx:{Colors.END}")
    print(f"  Rust: {rust_xml2abx:.2f}ms")
    print(f"  C++:  {cpp_xml2abx:.2f}ms")
    print(f"  Winner: {Colors.GREEN if rust_xml2abx < cpp_xml2abx else Colors.YELLOW}"
          f"{'Rust' if rust_xml2abx < cpp_xml2abx else 'C++'} "
          f"({max(rust_xml2abx, cpp_xml2abx) / min(rust_xml2abx, cpp_xml2abx):.2f}x faster){Colors.END}")
    
    print(f"\n{Colors.CYAN}abx2xml:{Colors.END}")
    print(f"  Rust: {rust_abx2xml:.2f}ms")
    print(f"  C++:  {cpp_abx2xml:.2f}ms")
    print(f"  Winner: {Colors.GREEN if rust_abx2xml < cpp_abx2xml else Colors.YELLOW}"
          f"{'Rust' if rust_abx2xml < cpp_abx2xml else 'C++'} "
          f"({max(rust_abx2xml, cpp_abx2xml) / min(rust_abx2xml, cpp_abx2xml):.2f}x faster){Colors.END}")
    
    # cleanup
    for f in [test_file, "temp_rust.abx", "temp_cpp.abx", "temp_rust.xml", "temp_cpp.xml"]:
        if os.path.exists(f):
            os.remove(f)
    
    print(f"\n{Colors.BOLD}{Colors.GREEN}✓ Benchmark complete! Check {chart_file}{Colors.END}\n")

if __name__ == "__main__":
    main()