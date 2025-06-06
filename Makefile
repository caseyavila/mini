JAVA=java
GENERATED=generated
GRAMMAR=Mini.g4

RUNTIME=antlr/runtime
CC=g++
CCARGS=-c -I $(RUNTIME)/include/ -I $(GENERATED) -std=c++20 -g
LDARGS=-g
LIBS=$(RUNTIME)/lib/libantlr4-runtime.a

GEN_SRC=generated/MiniLexer.cpp \
        generated/MiniParser.cpp \
        generated/MiniBaseVisitor.cpp \
        generated/MiniVisitor.cpp
GEN_OBJ=$(patsubst generated/%.cpp, build/%.o, $(GEN_SRC))

CPP_SRC=$(wildcard src/*.cpp) 
CPP_OBJ=$(patsubst src/%.cpp, build/%.o, $(CPP_SRC))

.PHONY: all clean

all: mini

$(GENERATED): $(GRAMMAR)
	@mkdir -p $(GENERATED)
	$(JAVA) -jar antlr/antlr-4.13.2-complete.jar -Dlanguage=Cpp -no-listener -visitor -o $(GENERATED) $(GRAMMAR)

build/%.o: src/%.cpp $(GENERATED)
	@mkdir -p build
	$(CC) $(CCARGS) $< -o $@

build/%.o: generated/%.cpp $(GENERATED)
	@mkdir -p build
	$(CC) $(CCARGS) $< -o $@

mini: $(CPP_OBJ) $(GEN_OBJ)
	$(CC) $(LDARGS) $(CPP_OBJ) $(GEN_OBJ) $(LIBS) -o mini

clean:
	@rm -rf build
	@rm -rf $(GENERATED)
	@rm -f mini
