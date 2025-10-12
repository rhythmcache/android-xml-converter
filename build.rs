use std::env;
use std::fs;
use std::path::PathBuf;

fn main() {
    let out_dir = PathBuf::from(env::var("OUT_DIR").unwrap());
    let target = env::var("TARGET").unwrap();

    println!("cargo:rerun-if-changed=src/abx.cc");
    println!("cargo:rerun-if-changed=src/abx.hpp");
    println!("cargo:rerun-if-changed=src/abx.h");
    println!("cargo:rerun-if-changed=src/abx_c.cc");
    println!("cargo:rerun-if-env-changed=LIBABX_PATH");

    // try pkg-config
    if try_pkg_config() {
        println!("cargo:warning=Found libabx via pkg-config");
        return;
    }

    // see LIBABX_PATH env variable
    if let Ok(libabx_path) = env::var("LIBABX_PATH") {
        if try_env_path(&libabx_path) {
            println!("cargo:warning=Found libabx at LIBABX_PATH: {}", libabx_path);
            return;
        }
    }

    // build from source
    println!("cargo:warning=Building libabx from source");
    build_from_source(&out_dir, &target);
}

fn try_pkg_config() -> bool {
    if let Ok(lib) = pkg_config::Config::new().statik(true).probe("libabx") {
        for path in &lib.link_paths {
            println!("cargo:rustc-link-search=native={}", path.display());
        }
        for lib_name in &lib.libs {
            println!("cargo:rustc-link-lib=static={}", lib_name);
        }
        return true;
    }
    false
}

fn try_env_path(path: &str) -> bool {
    let lib_path = PathBuf::from(path);

    // check for static library
    let static_lib = lib_path.join("libabx.a");
    if static_lib.exists() {
        println!("cargo:rustc-link-search=native={}", lib_path.display());
        println!("cargo:rustc-link-lib=static=abx");
        link_cxx_stdlib();
        return true;
    }

    // check for shared library
    let shared_lib = lib_path.join("libabx.so");
    if shared_lib.exists() {
        println!("cargo:rustc-link-search=native={}", lib_path.display());
        println!("cargo:rustc-link-lib=dylib=abx");
        link_cxx_stdlib();
        return true;
    }

    // Windows DLL
    let dll_lib = lib_path.join("abx.lib");
    if dll_lib.exists() {
        println!("cargo:rustc-link-search=native={}", lib_path.display());
        println!("cargo:rustc-link-lib=static=abx");
        link_cxx_stdlib();
        return true;
    }

    false
}

fn build_from_source(out_dir: &PathBuf, target: &str) {
    let pugixml_dir = out_dir.join("pugixml");
    fs::create_dir_all(&pugixml_dir).expect("Failed to create pugixml directory");

    // download pugixml files
    let pugixml_files = [
        (
            "pugiconfig.hpp",
            "https://raw.githubusercontent.com/zeux/pugixml/master/src/pugiconfig.hpp",
        ),
        (
            "pugixml.cpp",
            "https://raw.githubusercontent.com/zeux/pugixml/master/src/pugixml.cpp",
        ),
        (
            "pugixml.hpp",
            "https://raw.githubusercontent.com/zeux/pugixml/master/src/pugixml.hpp",
        ),
    ];

    for (filename, url) in &pugixml_files {
        let dest = pugixml_dir.join(filename);
        if !dest.exists() {
            println!("cargo:warning=Downloading {}...", filename);
            download_file(url, &dest);
        }
    }

    let mut build = cc::Build::new();

    build
        .cpp(true)
        .flag_if_supported("-std=c++17")
        .flag_if_supported("/std:c++17")
        .include("src")
        .include(&pugixml_dir)
        .file("src/abx.cc")
        .file("src/abx_c.cc")
        .file(pugixml_dir.join("pugixml.cpp"))
        .warnings(false); // suppress warnings from pugixml

    if target.contains("windows") {
        build.flag_if_supported("/EHsc"); // enable C++ exceptions on MSVC
    }

    build.compile("abx");

    println!("cargo:rustc-link-lib=static=abx");
    link_cxx_stdlib();
}

fn download_file(url: &str, dest: &PathBuf) {
    // use curl
    let status = std::process::Command::new("curl")
        .args(&["-fsSL", "-o", dest.to_str().unwrap(), url])
        .status();

    if status.is_ok() && status.unwrap().success() {
        return;
    }

    // fallback to wget
    let status = std::process::Command::new("wget")
        .args(&["-q", "-O", dest.to_str().unwrap(), url])
        .status();

    if status.is_ok() && status.unwrap().success() {
        return;
    }

    panic!(
        "Failed to download {}. Please install curl or wget, or manually download to {}",
        url,
        dest.display()
    );
}

fn link_cxx_stdlib() {
    let target = env::var("TARGET").unwrap();

    if target.contains("apple") {
        println!("cargo:rustc-link-lib=c++");
    } else if target.contains("linux") || target.contains("android") {
        println!("cargo:rustc-link-lib=stdc++");
    } else if target.contains("windows") {
        if target.contains("gnu") {
            println!("cargo:rustc-link-lib=stdc++");
        }
    }
}
