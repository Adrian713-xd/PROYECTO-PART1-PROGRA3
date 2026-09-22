// indice.hpp — Indice invertido con campos, construido sobre los tries.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "binario.hpp"
#include "catalogo.hpp"
#include "trie.hpp"

namespace pf {

// Mascara de campos donde aparece un termino dentro de un documento.
enum Campo : uint8_t {
  C_TITULO = 1,
  C_SINOPSIS = 2,
  C_DIRECTOR = 4,
  C_REPARTO = 8,
  C_GENERO = 16,
  C_ORIGEN = 32,
  C_CUALQUIERA = 63
};

// Un termino del perfil de una pelicula con su peso tf-idf. Es un POD propio
// y no un std::pair porque std::pair no es trivialmente copiable en libstdc++
// y por lo tanto no se puede volcar en bloque al cache binario.
struct TerminoPeso {
  int32_t termino;
  float peso;
};

// Una entrada de la lista de publicaciones (posting list).
struct Publicacion {
  int32_t doc;
  uint16_t tfTitulo;
  uint16_t tfSinopsis;
  uint16_t tfTag; // director + reparto + genero + origen
  uint8_t campos;
  uint8_t relleno = 0;
};

struct EstadisticasIndice {
  size_t terminos = 0;
  size_t publicaciones = 0;
  size_t nodosPrefijos = 0;
  size_t nodosSufijos = 0;
  size_t megabytes = 0;
  double segundos = 0.0;
};

class Indice {
public:
  EstadisticasIndice construir(const Catalogo &catalogo);

  // --- Cache binario -------------------------------------------------------
  // Construir los arboles cuesta ~4 s y parsear el CSV otros 1-4 s. El cache
  // guarda el catalogo ya limpio y las estructuras ya construidas, de modo que
  // el arranque siguiente sea una lectura secuencial de disco.
  // La validez se comprueba contra el tamano del CSV de origen: si el archivo
  // cambia, el cache se descarta y se reconstruye.
  bool guardarCache(const std::string &ruta, const Catalogo &catalogo,
                    uint64_t huellaCsv) const;
  bool cargarCache(const std::string &ruta, Catalogo &catalogo,
                   uint64_t huellaCsv, EstadisticasIndice &est);

  const TriePrefijos &vocabulario() const { return vocab_; }
  const TrieSufijos &sufijos() const { return sufijos_; }

  const std::vector<Publicacion> &publicaciones(int idTermino) const {
    return publicaciones_[idTermino];
  }
  int frecuenciaDocumental(int idTermino) const {
    return static_cast<int>(publicaciones_[idTermino].size());
  }
  double idf(int idTermino) const;
  // idf a partir de una frecuencia documental arbitraria (para expansiones).
  double idfDesdeDf(double df) const;

  size_t numDocumentos() const { return numDocs_; }
  double longitudPromedio() const { return longitudPromedio_; }
  uint32_t longitud(int doc) const { return longitudDoc_[doc]; }

  // Vector de terminos representativos de un documento (para recomendaciones).
  const std::vector<TerminoPeso> &perfil(int doc) const { return perfiles_[doc]; }
  float normaPerfil(int doc) const { return normas_[doc]; }

private:
  TriePrefijos vocab_;
  TrieSufijos sufijos_;
  std::vector<std::vector<Publicacion>> publicaciones_;
  std::vector<uint32_t> longitudDoc_;
  std::vector<std::vector<TerminoPeso>> perfiles_;
  std::vector<float> normas_;
  size_t numDocs_ = 0;
  double longitudPromedio_ = 1.0;

  void rellenarEstadisticas(EstadisticasIndice &est) const;

  void construirPerfiles();
};

} // namespace pf
