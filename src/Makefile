PREFIX?=/usr/local
CXXFLAGS=-Wall -Wextra -W -O3 -Wno-unused-parameter -I../client-lib
OBJECTS=generate-compiled-font.o bdf-font.o

bdfont-data-gen: $(OBJECTS)
	$(CXX) -o $@ $^ $(LDFLAGS)

install: bdfont-data-gen
	install $^ $(PREFIX)/bin

generate-compiled-font.o: generate-compiled-font.cc font-support-str.inc

font-support-str.inc: ../client-lib/font-support.h ../client-lib/font-support.c
	./make-inc.sh $^ > $@

clean:
	rm -f $(OBJECTS) font-support-str.inc bdfont-data-gen
