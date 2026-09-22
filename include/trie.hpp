// trie.hpp — Las dos estructuras de arbol del proyecto.
//
// Ambos arboles guardan UN CARACTER por nodo, tal como exige el enunciado.
// Los hijos se representan como lista enlazada de hermanos (primerHijo /
// hermano) en lugar de un arreglo de 36 punteros por nodo: con ~5 millones de
// nodos, el arreglo costaria >700 MB mientras que la lista enlazada cuesta
// 16 bytes por nodo. El alfabeto es [a-z0-9] (36 simbolos), asi que recorrer
// la lista de hermanos es O(36) en el peor caso y ~O(3) en la practica.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "binario.hpp"

namespace pf {

// ---------------------------------------------------------------------------
// TriePrefijos: diccionario del vocabulario.
// Cada palabra distinta del corpus se inserta una sola vez y recibe un id.
// Resuelve (a) busqueda exacta de palabra y (b) busqueda por prefijo, ambas en
// O(|patron|) sin tablas hash.
// ---------------------------------------------------------------------------
class TriePrefijos {
public:
  TriePrefijos();

  // Inserta la palabra si no existe y devuelve su id de termino.
  int insertar(const std::string &palabra);

  // Devuelve el id del termino, o -1 si no esta en el vocabulario. O(|palabra|)
  int buscar(const std::string &palabra) const;

  // Ids de todos los terminos que empiezan con el prefijo dado.
  std::vector<int> conPrefijo(const std::string &prefijo, size_t maxIds) const;

  // Reconstruye la cadena asociada a un id de termino.
  const std::string &termino(int id) const { return terminos_[id]; }

  size_t numTerminos() const { return terminos_.size(); }
  size_t numNodos() const { return nodos_.size(); }
  size_t bytesAproximados() const;

  void escribir(EscritorBinario &e) const;
  bool leer(LectorBinario &l);

private:
  struct Nodo {
    int32_t primerHijo = -1;
    int32_t hermano = -1;
    int32_t idTermino = -1; // >= 0 si aqui termina una palabra
    uint8_t c = 0;
  };
  std::vector<Nodo> nodos_;
  std::vector<std::string> terminos_;

  int32_t descender(const std::string &s) const; // -1 si el camino no existe
};

// ---------------------------------------------------------------------------
// TrieSufijos: trie generalizado de sufijos SOBRE EL VOCABULARIO.
//
// Para cada palabra distinta se insertan todos sus sufijos y se marca el nodo
// final con el id del termino. Entonces:
//
//     terminos que contienen el patron P
//        = terminos marcados en el subarbol al que se llega descendiendo P
//
// porque P aparece en T en la posicion i si y solo si el sufijo T[i..] empieza
// con P. La busqueda es O(|P| + tamano del subarbol).
//
// Se construye sobre el VOCABULARIO (143 535 palabras distintas, 1.07 M de
// caracteres) y no sobre el texto completo (13.3 M de tokens, 75 MB): esa es la
// decision que hace viable la busqueda por sub-cadena en memoria.
// ---------------------------------------------------------------------------
class TrieSufijos {
public:
  TrieSufijos();

  // Inserta los |t| sufijos de t, todos marcados con idTermino.
  void insertarTermino(const std::string &t, int idTermino);

  // Ids de los terminos del vocabulario que contienen 'patron' como sub-cadena.
  // 'maxIds' acota el recorrido para patrones muy cortos y frecuentes.
  std::vector<int> terminosQueContienen(const std::string &patron,
                                        size_t maxIds) const;

  size_t numNodos() const { return nodos_.size(); }
  size_t bytesAproximados() const;

  void escribir(EscritorBinario &e) const;
  bool leer(LectorBinario &l);

private:
  struct Nodo {
    int32_t primerHijo = -1;
    int32_t hermano = -1;
    int32_t cabezaLista = -1; // lista enlazada de ids de termino
    uint8_t c = 0;
  };
  struct Entrada {
    int32_t idTermino;
    int32_t siguiente;
  };
  std::vector<Nodo> nodos_;
  std::vector<Entrada> entradas_;
};

} // namespace pf
