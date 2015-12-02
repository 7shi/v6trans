TARGET = parser

CXX = g++ -std=c++11

all: $(TARGET)

parser: parser.o parsecpp.o
	$(CXX) -o $@ $^

parser.o: parser.cpp parsecpp.h
	$(CXX) -c $<

parsecpp.o: parsecpp.cpp parsecpp.h
	$(CXX) -c $<

clean:
	rm -f $(TARGET) *.exe *.o
