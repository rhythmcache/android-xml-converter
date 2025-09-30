# android-xml-converter
This is just an experimental code to convert between android xml formats (abx and xml). functionality is not guaranteed.

#### android's default `abx2xml` and `xml2abx`

- `abx2xml` and `xml2abx` binaries found generally in /system/bin/ of android devices is just a shell script that acts as a wrapper for executing abx.jar. It depends on Java and app_process, making it reliant on Android’s runtime environment. Since it invokes Java code, it cannot run independently in environments where Java isn’t available and also the overhead of launching a Java process adds extra execution time.

## Standalone `abx2xml` and `xml2abx`
- This  `abx2xml` and `xml2abx` binary performs the same function—converting between ABX and XML but in a fully standalone manner. Unlike default android binaries ,this binary does not require Java, or abx.jar to function.  


### Command line usage

- Similar to default abx2xml and xml2abx

- `abx2xml [-i] input [output]`

- `xml2abx [-i] [--collapse-whitespaces] input [output]`


### Sources
[BinaryXmlPullParser.java](https://cs.android.com/android/platform/superproject/main/+/main:/frameworks/libs/modules-utils/java/com/android/modules/utils/BinaryXmlPullParser.java)
[BinaryXmlSerializer.java](https://cs.android.com/android/platform/superproject/main/+/main:/frameworks/libs/modules-utils/java/com/android/modules/utils/BinaryXmlSerializer.java)

---

