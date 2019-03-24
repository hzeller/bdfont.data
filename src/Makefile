CXXFLAGS=-Wall -Wextra -W -O3 -Wno-unused-parameter -I../client-lib
OBJECTS=generate-compiled-font.o bdf-font.o

bdfont-data-gen: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(OBJECTS) bdfont-data-gen
