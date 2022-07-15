hw2: main.cpp hw2_output.c
	gcc -c -o hw2_output.o hw2_output.c
	g++ -std=c++11 -o hw2 main.cpp hw2_output.o -lpthread