name := huffman
output := ./out/$(name)

run: $(output)
	$< $(args)

$(output): $(name).c
	clang $< -o $@
