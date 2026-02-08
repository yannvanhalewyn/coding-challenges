out := huffman
run:
	cargo run $(args)

compile: $(out)

$(out): $(wildcard src/*.rs)
	# Or `cargo build` / `cargo build --release`
	rustc src/main.rs -o $(out)
