# Multithreaded _n-gram_ Counter

As a precursor of further natural language processing, this tool counts the appearance of n-grams in all files in a given directory recursively. User defines `n`. This tool implements the famous _MapReduce_ pattern for key-value storage and enables multi-threading functionalities using the C++ `<future>` library. Use of locks is heavily minimized by improving memory locality.

To compile: 
```bash
make
```
To use:
```bash
./ngc++ -n=<#gram> -t=<#threads> <dir>
```
