# abx2xml
`This` is just a cpp version of [ccl_abx](https://github.com/cclgroupltd/android-bits/tree/main/ccl_abx)
Python module for reading Android Binary XML (ABX) files and converting them back to XML for processing. 
## Command line usage
To convert an ABX file at the command line:

`abx2xml file_path_here.xml`

The converted data will be outputted to `STDOUT`, so if you want to save the file you can redirect the output:

`abx2xml file_path_here.xml > processed_file_path.xml`


### Credit
[@android-bits](https://github.com/cclgroupltd/android-bits/tree/main/ccl_abx)

