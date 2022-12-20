# Makefile
all: client.c serverM.cpp serverC.cpp serverEE.cpp serverCS.cpp
	gcc -o client client.c
	g++ -std=c++11 -o serverM serverM.cpp
	g++ -std=c++11 -o serverC serverC.cpp
	g++ -std=c++11 -o serverEE serverEE.cpp
	g++ -std=c++11 -o serverCS serverCS.cpp

clean: 
	rm -rf *.o *.out client serverM serverC serverCS serverEE

.PHONY: all clean