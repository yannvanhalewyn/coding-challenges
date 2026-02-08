name := huffman
output := $(name)

build: $(output)

run: $(output)
	./$< $(args)

$(output): $(name).c
	clang $< -o $@
