# android-xml-converter
This is just an experimental code to convert between android xml formats (abx and xml). functionality is not guaranteed.

#### android's default `abx2xml` and `xml2abx`

- `abx2xml` and `xml2abx` binaries found generally in /system/bin/ of android devices is just a shell script that acts as a wrapper for executing abx.jar. It depends on Java and app_process, making it reliant on Android’s runtime environment. Since it invokes Java code, it cannot run independently in environments where Java isn’t available and also the overhead of launching a Java process adds extra execution time.

## Standalone `abx2xml` and `xml2abx`
- This  `abx2xml` and `xml2abx` binary performs the same function—converting between ABX and XML but in a fully standalone manner. Unlike default android binaries ,this binary does not require Java, or abx.jar to function.  


### Command line usage

- Similar to default abx2xml and xml2abx

- `abx2xml [-i] input [output]`

- `xml2abx [-i] [--collapse-whitespace] input [output]`


## Building

#### Build Steps (C/C++ via Meson)

```bash
meson setup builddir
meson compile -C builddir
```
---

## Rust Implementations

There are also Rust versions of these converters, implementing the same functionality:

- [abx2xml-rs](https://github.com/rhythmcache/abx2xml-rs) – `abx2xml`.
- [xml2abx-rs](https://github.com/rhythmcache/xml2abx-rs) – `xml2abx`.

These Rust versions can be installed via Cargo:

```bash
cargo install abx2xml
cargo install xml2abx
```
---

### Sources
[BinaryXmlPullParser.java](https://cs.android.com/android/platform/superproject/main/+/main:/frameworks/libs/modules-utils/java/com/android/modules/utils/BinaryXmlPullParser.java)
[BinaryXmlSerializer.java](https://cs.android.com/android/platform/superproject/main/+/main:/frameworks/libs/modules-utils/java/com/android/modules/utils/BinaryXmlSerializer.java)

---

