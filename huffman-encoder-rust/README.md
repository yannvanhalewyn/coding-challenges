# Huffman Encoder in Rust

This is code for a Huffman encoder as solution for [Build Your Own Compression Tool](https://codingchallenges.fyi/challenges/challenge-huffman).

## Compiling

Just run make in this directory:

```sh
make compile
```

It will create a `./huffman` binary. You can run this using arguments:

*Encoding*

```sh
./huffman encode test.txt -o test.txt.encoded
```

*Decoding*

```sh
./huffman decode test.txt.encoded -o test.txt.decoded
```

## Building and running directly:

In dev it's useful to have a repeatable make command. You can specify args= to the make command like so:

```sh
make run args="encode test.txt -o test.txt.encoded"
make run args="decode test.txt.encoded -o test.txt.decoded"
```

# Notes

Since this implementation is for learning purposes, no further improvements have been made to the algorithm, such as improve the storage efficiency of the header. Smaller files get larger because of the header size, and more would be ways to improve this program.

1. Use `BinaryHeap`: Replace manual sorting with efficient priority queue
2. Eliminate panics: Convert all `panic!` calls to proper `Result` returns
3. Optimize I/O: Read larger chunks instead of single bytes
4. Simplify `BitReader` type: Consider owning the reader instead of borrowing
5. Match C file format: Change to `"HUFF"` in header and make both implementation use matching header formats
6. Add comprehensive tests: Both implementations lack thorough testing
7. Add input validation: Check for empty files, invalid characters
8. Optimize for single-character files: Handle edge case efficiently
9. Add compression ratio reporting: Show space savings achieved
10. Consider endianness: Make format portable across different architectures
