CPP=g++ -Wall -pedantic -std=c++11
HEADS=-I/usr/include/SDL2
LIBS=-lSDL2

b-out: b-out.cpp
	${CPP} $^ -o $@ ${HEADS} ${LIBS}
