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
