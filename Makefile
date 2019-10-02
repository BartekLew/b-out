CPP=g++ -Wall -pedantic -std=c++11 -g
HEADS=-I/usr/include/SDL2
LIBS=-lSDL2 -lSDL2_net

b-out: b-out.cpp net.o
	${CPP} $^ -o $@ ${HEADS} ${LIBS}

net.o: net.cpp
	${CPP} -c $^ -o $@ ${HEADS}
