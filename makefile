TARGET=lfqueue

nlock: nlqueue.h nlqueue.cpp main.cpp
	g++ -std=c++11 -fpermissive -lpthread -g2 -O2 *.cpp -o $(TARGET)

clean:
	rm -rf $(TARGET)
