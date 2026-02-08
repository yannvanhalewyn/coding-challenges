out := huffman
run:
	cargo run $(args)

compile: $(out)

$(out): $(wildcard src/*.rs)
	rustc src/main.rs -o $(out)
