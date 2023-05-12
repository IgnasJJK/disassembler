# Disassembler for x8086

## Building

Run the `build.bat` command in a shell that has the Microsoft `cl` compiler environment initialized.

## Running

The executable is in `build\`. The output is written to standard output.

Command line usage:
```sh
main.exe [-e] <filename>
```

- `-e`: Emulate the disassembled instructions.

## Testing

The `tests` directory contains listings as they're provided in [Computer Enhance!](https://computerenhance.com).

Use the following command to test the disassembler with any file in the tests directory. _Do not include the extension (.asm)_.
```sh
test.bat <listing name>
```

The `test` command will assemble the listing and disassemble it using this program.
Afterwards, it will assemble the disassembled code and compare the binaries.

- `temp\assembly`: Assembled original code
- `temp\disassembly.asm`: Disassembled code
- `temp\disassembly`: Reassembled code
- `temp\disassembly-exec.asm`: Disassembled code with emulation output.

## Sources

- [Computer, Enhance!](https://computerenhance.com)
- [Computer, Enhance! Repository](https://github.com/cmuratori/computer_enhance)
- Intel. _The 8086 Family User's Manual_. 1979.