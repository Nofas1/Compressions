# First Project: Compression

A C++17 file-compression program built around the project's original
`Deflate`, `LZ77`, `Builder`, and `Archivator` classes.

## Algorithms

- **Deflate** writes and reads a zlib-wrapped DEFLATE stream. The implementation
  is written in this project: compression uses LZ77 matches and canonical fixed
  Huffman codes, and decompression handles stored, fixed-Huffman, and
  dynamic-Huffman DEFLATE blocks. It does not link to zlib.
- **LZ77** is a separate sliding-window codec with a project-specific binary
  format.

LZMA is not implemented and is reported as unavailable instead of producing
data mislabeled as LZMA.

## Build and test

The project has no third-party build dependencies:

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

Requires CMake 3.16 or later and a C++17 compiler.
With MinGW, the C++ runtime is linked statically so another program's
`libstdc++-6.dll` earlier in `PATH` cannot prevent the app or tests from
starting.

## Command line

```powershell
.\build\archivator.exe compress deflate input.bin input.deflate
.\build\archivator.exe decompress deflate input.deflate restored.bin
.\build\archivator.exe compress lz77 input.bin input.lz77
.\build\archivator.exe decompress lz77 input.lz77 restored.bin
```

The program reads and writes binary files, including empty files. DEFLATE
output includes the zlib header and Adler-32 checksum, which are also verified
when decompressing. The DEFLATE implementation itself does not use zlib.
