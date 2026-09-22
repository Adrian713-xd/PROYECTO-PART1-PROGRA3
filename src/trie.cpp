#include "trie.hpp"

#include <algorithm>

namespace pf {

// ===========================================================================
// TriePrefijos
// ===========================================================================

TriePrefijos::TriePrefijos() { nodos_.emplace_back(); /* raiz */ }

int TriePrefijos::insertar(const std::string &palabra) {
  int32_t actual = 0;
  for (unsigned char c : palabra) {
    int32_t hijo = nodos_[actual].primerHijo;
    int32_t anterior = -1;
    while (hijo != -1 && nodos_[hijo].c != c) {
      anterior = hijo;
      hijo = nodos_[hijo].hermano;
    }
    if (hijo == -1) {
      nodos_.emplace_back();
      hijo = static_cast<int32_t>(nodos_.size()) - 1;
      nodos_[hijo].c = c;
      if (anterior == -1) {
        nodos_[actual].primerHijo = hijo;
      } else {
        nodos_[anterior].hermano = hijo;
      }
    }
    actual = hijo;
  }
  if (nodos_[actual].idTermino == -1) {
    nodos_[actual].idTermino = static_cast<int32_t>(terminos_.size());
    terminos_.push_back(palabra);
  }
  return nodos_[actual].idTermino;
}

int32_t TriePrefijos::descender(const std::string &s) const {
  int32_t actual = 0;
  for (unsigned char c : s) {
    int32_t hijo = nodos_[actual].primerHijo;
    while (hijo != -1 && nodos_[hijo].c != c) hijo = nodos_[hijo].hermano;
    if (hijo == -1) return -1;
    actual = hijo;
  }
  return actual;
}

int TriePrefijos::buscar(const std::string &palabra) const {
  int32_t n = descender(palabra);
  return n == -1 ? -1 : nodos_[n].idTermino;
}

std::vector<int> TriePrefijos::conPrefijo(const std::string &prefijo,
                                          size_t maxIds) const {
  std::vector<int> salida;
  int32_t raizSub = descender(prefijo);
  if (raizSub == -1) return salida;

  std::vector<int32_t> pila{raizSub};
  while (!pila.empty() && salida.size() < maxIds) {
    int32_t n = pila.back();
    pila.pop_back();
    if (nodos_[n].idTermino >= 0) salida.push_back(nodos_[n].idTermino);
    for (int32_t h = nodos_[n].primerHijo; h != -1; h = nodos_[h].hermano) {
      pila.push_back(h);
    }
  }
  return salida;
}

void TriePrefijos::escribir(EscritorBinario &e) const {
  e.vectorPod(nodos_);
  e.vectorCadenas(terminos_);
}

bool TriePrefijos::leer(LectorBinario &l) {
  return l.vectorPod(nodos_) && l.vectorCadenas(terminos_);
}

size_t TriePrefijos::bytesAproximados() const {
  size_t b = nodos_.capacity() * sizeof(Nodo);
  for (const auto &t : terminos_) b += t.capacity() + sizeof(std::string);
  return b;
}

// ===========================================================================
// TrieSufijos
// ===========================================================================

TrieSufijos::TrieSufijos() { nodos_.emplace_back(); /* raiz */ }

void TrieSufijos::insertarTermino(const std::string &t, int idTermino) {
  const size_t n = t.size();
  for (size_t inicio = 0; inicio < n; ++inicio) {
    int32_t actual = 0;
    for (size_t i = inicio; i < n; ++i) {
      const unsigned char c = static_cast<unsigned char>(t[i]);
      int32_t hijo = nodos_[actual].primerHijo;
      int32_t anterior = -1;
      while (hijo != -1 && nodos_[hijo].c != c) {
        anterior = hijo;
        hijo = nodos_[hijo].hermano;
      }
      if (hijo == -1) {
        nodos_.emplace_back();
        hijo = static_cast<int32_t>(nodos_.size()) - 1;
        nodos_[hijo].c = c;
        if (anterior == -1) {
          nodos_[actual].primerHijo = hijo;
        } else {
          nodos_[anterior].hermano = hijo;
        }
      }
      actual = hijo;
    }
    // Marcar el final del sufijo con el id del termino que lo origino.
    entradas_.push_back({static_cast<int32_t>(idTermino), nodos_[actual].cabezaLista});
    nodos_[actual].cabezaLista = static_cast<int32_t>(entradas_.size()) - 1;
  }
}

std::vector<int> TrieSufijos::terminosQueContienen(const std::string &patron,
                                                   size_t maxIds) const {
  std::vector<int> salida;
  if (patron.empty()) return salida;

  int32_t actual = 0;
  for (unsigned char c : patron) {
    int32_t hijo = nodos_[actual].primerHijo;
    while (hijo != -1 && nodos_[hijo].c != c) hijo = nodos_[hijo].hermano;
    if (hijo == -1) return salida; // el patron no aparece en ninguna palabra
    actual = hijo;
  }

  // Recorrido en profundidad del subarbol: cada marca es un termino que
  // contiene el patron. Un mismo termino puede aparecer varias veces (el
  // patron ocurre en varias posiciones), asi que se deduplica al final.
  std::vector<int32_t> pila{actual};
  while (!pila.empty() && salida.size() < maxIds) {
    int32_t n = pila.back();
    pila.pop_back();
    for (int32_t e = nodos_[n].cabezaLista; e != -1; e = entradas_[e].siguiente) {
      salida.push_back(entradas_[e].idTermino);
    }
    for (int32_t h = nodos_[n].primerHijo; h != -1; h = nodos_[h].hermano) {
      pila.push_back(h);
    }
  }
  std::sort(salida.begin(), salida.end());
  salida.erase(std::unique(salida.begin(), salida.end()), salida.end());
  return salida;
}

void TrieSufijos::escribir(EscritorBinario &e) const {
  e.vectorPod(nodos_);
  e.vectorPod(entradas_);
}

bool TrieSufijos::leer(LectorBinario &l) {
  return l.vectorPod(nodos_) && l.vectorPod(entradas_);
}

size_t TrieSufijos::bytesAproximados() const {
  return nodos_.capacity() * sizeof(Nodo) + entradas_.capacity() * sizeof(Entrada);
}

} // namespace pf
