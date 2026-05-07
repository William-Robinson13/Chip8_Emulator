CFLAGS = -lmingw32 -lSDL2main -lSDL2
all: 
	g++ -Isrc/Include -Lsrc/lib -o chip8 chip8.cpp $(CFLAGS)

debug:
	g++ -Isrc/Include -Lsrc/lib -o chip8 chip8.cpp $(CFLAGS) -DDEBUG -g

queso: 
	g++ -Isrc/Include -Lsrc/lib -o chip8 chip8_queso.cpp $(CFLAGS) -DDEBUG -g