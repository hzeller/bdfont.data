OBJECTS=generate-compiled-font.o bdf-font.o

generate-compiled-font: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) generate-compiled-font
