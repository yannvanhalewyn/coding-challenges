name := bitshifting
out := ./out/$(name)

run: $(out)
	$< $(args)

$(out): $(name).c
	mkdir -p $(dir $(out))
	clang $< -o $@

clean:
	rm -f $(name)
