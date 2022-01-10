LDFLAGS:=-lSDL2
kchip8: main.o
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm kchip8 main.o
