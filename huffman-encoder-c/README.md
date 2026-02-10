# Huffman Encoder in C

This is code for a Huffman encoder as solution for [Build Your Own Compression Tool](https://codingchallenges.fyi/challenges/challenge-huffman).

## Compiling

Just run make in this directory:

```sh
make
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

Some potential improvements:

1. Use heap for node storage: Replace Variable Array Declaration (VLA) `HuffmanNode* nodes[node_count]` which can overflow for larger datasets with malloc.
2. Implement proper error handling: Replace exit() calls with error returns
3. Add const correctness: Mark read-only parameters as const, like `freq[]`
4. Add comprehensive tests: Both implementations lack thorough testing
5. Add input validation: Check for empty files, invalid characters
6. Optimize for smaller or single-character files: Handle edge case efficiently
7. Add compression ratio reporting: Show space savings achieved
8. Consider endianness: Make format portable across different architectures
