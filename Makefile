# Makefile de respaldo (si no hay CMake instalado).
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Iinclude
FUENTES   = src/normalizador.cpp src/csv.cpp src/catalogo.cpp src/trie.cpp \
            src/indice.cpp src/buscador.cpp src/recomendador.cpp \
            src/estado_usuario.cpp src/main.cpp
OBJETOS   = $(FUENTES:.cpp=.o)

all: streaming

streaming: $(OBJETOS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJETOS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

pruebas: tests/pruebas.cpp src/normalizador.o src/csv.o src/catalogo.o \
         src/trie.o src/indice.o src/buscador.o src/recomendador.o \
         src/estado_usuario.o
	$(CXX) $(CXXFLAGS) -o pruebas $^

clean:
	rm -f $(OBJETOS) streaming pruebas tests/*.o

.PHONY: all clean pruebas
