name := huffman
output := $(name)

run: $(output)
	./$< $(args)

$(output): $(name).c
	clang $< -o $@
