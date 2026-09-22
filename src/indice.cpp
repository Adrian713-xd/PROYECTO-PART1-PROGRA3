#include "indice.hpp"

#include "normalizador.hpp"

#include "binario.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace pf {
namespace {
// Cuantos terminos se conservan por pelicula para el perfil de similitud.
constexpr size_t kTerminosPorPerfil = 40;

// Cabecera del cache. La version sube cuando cambia el formato o cualquier
// parametro que altere el contenido del indice.
constexpr uint32_t kMagiaCache = 0x55544543u; // "UTEC"
constexpr uint32_t kVersionCache = 3;
} // namespace

double Indice::idfDesdeDf(double dfEntrada) const {
  double df = dfEntrada;
  const double N = static_cast<double>(numDocs_);
  if (df < 0.0) df = 0.0;
  if (df > N - 1.0) df = N - 1.0;
  return std::log(1.0 + (N - df + 0.5) / (df + 0.5));
}

double Indice::idf(int idTermino) const {
  return idfDesdeDf(static_cast<double>(publicaciones_[idTermino].size()));
}

EstadisticasIndice Indice::construir(const Catalogo &catalogo) {
  using Reloj = std::chrono::steady_clock;
  const auto t0 = Reloj::now();

  numDocs_ = catalogo.tamano();
  longitudDoc_.assign(numDocs_, 0);
  publicaciones_.reserve(200000);

  // ultimoDoc_[t] evita usar una tabla hash por documento: si el ultimo
  // documento que toco el termino t es el actual, basta con actualizar la
  // ultima publicacion de su lista.
  std::vector<int32_t> ultimoDoc;
  ultimoDoc.reserve(200000);

  std::vector<std::string> tokens;
  tokens.reserve(2048);

  auto indexar = [&](int doc, const std::string &texto, Campo campo) {
    tokens.clear();
    tokenizarNormalizado(normalizar(texto), tokens);
    for (const std::string &tok : tokens) {
      if (tok.size() > 30) continue; // basura tipografica
      const int id = vocab_.insertar(tok);
      if (static_cast<size_t>(id) >= publicaciones_.size()) {
        publicaciones_.resize(id + 1);
        ultimoDoc.resize(id + 1, -1);
      }
      if (ultimoDoc[id] != doc) {
        ultimoDoc[id] = doc;
        publicaciones_[id].push_back({doc, 0, 0, 0, 0, 0});
      }
      Publicacion &p = publicaciones_[id].back();
      p.campos |= static_cast<uint8_t>(campo);
      switch (campo) {
      case C_TITULO:
        if (p.tfTitulo < 65000) ++p.tfTitulo;
        break;
      case C_SINOPSIS:
        if (p.tfSinopsis < 65000) ++p.tfSinopsis;
        break;
      default:
        if (p.tfTag < 65000) ++p.tfTag;
        break;
      }
    }
    if (campo == C_SINOPSIS || campo == C_TITULO) {
      longitudDoc_[doc] += static_cast<uint32_t>(tokens.size());
    }
  };

  for (size_t d = 0; d < numDocs_; ++d) {
    const Pelicula &p = catalogo[d];
    const int doc = static_cast<int>(d);
    indexar(doc, p.titulo, C_TITULO);
    indexar(doc, p.sinopsis, C_SINOPSIS);
    if (!p.director.empty()) indexar(doc, p.director, C_DIRECTOR);
    for (const auto &actor : p.reparto) indexar(doc, actor, C_REPARTO);
    for (const auto &g : p.generos) indexar(doc, g, C_GENERO);
    if (!p.origen.empty()) indexar(doc, p.origen, C_ORIGEN);
  }

  double suma = 0.0;
  for (uint32_t l : longitudDoc_) suma += l;
  longitudPromedio_ = numDocs_ ? suma / numDocs_ : 1.0;

  // El trie de sufijos se llena una sola vez, cuando el vocabulario ya es
  // definitivo: insertar sufijos de palabras repetidas seria trabajo perdido.
  for (size_t id = 0; id < vocab_.numTerminos(); ++id) {
    const std::string &t = vocab_.termino(static_cast<int>(id));
    if (t.size() <= 24) sufijos_.insertarTermino(t, static_cast<int>(id));
  }

  construirPerfiles();

  EstadisticasIndice est;
  rellenarEstadisticas(est);
  est.segundos = std::chrono::duration<double>(Reloj::now() - t0).count();
  return est;
}

void Indice::rellenarEstadisticas(EstadisticasIndice &est) const {
  est.terminos = vocab_.numTerminos();
  est.publicaciones = 0;
  for (const auto &lp : publicaciones_) est.publicaciones += lp.size();
  est.nodosPrefijos = vocab_.numNodos();
  est.nodosSufijos = sufijos_.numNodos();
  size_t bytes = vocab_.bytesAproximados() + sufijos_.bytesAproximados();
  bytes += est.publicaciones * sizeof(Publicacion);
  for (const auto &per : perfiles_) {
    bytes += per.capacity() * sizeof(TerminoPeso);
  }
  est.megabytes = bytes / (1024 * 1024);
}

// ---------------------------------------------------------------------------
// Cache binario
// ---------------------------------------------------------------------------
bool Indice::guardarCache(const std::string &ruta, const Catalogo &catalogo,
                          uint64_t huellaCsv) const {
  EscritorBinario e;
  e.reservar(220u * 1024u * 1024u);
  e.pod(kMagiaCache);
  e.pod(kVersionCache);
  e.pod(huellaCsv);
  e.pod<uint32_t>(static_cast<uint32_t>(sizeof(Publicacion)));

  catalogo.escribir(e);
  vocab_.escribir(e);
  sufijos_.escribir(e);

  e.pod<uint64_t>(publicaciones_.size());
  for (const auto &lista : publicaciones_) e.vectorPod(lista);
  e.vectorPod(longitudDoc_);
  e.pod<uint64_t>(perfiles_.size());
  for (const auto &per : perfiles_) e.vectorPod(per);
  e.vectorPod(normas_);
  e.pod<uint64_t>(static_cast<uint64_t>(numDocs_));
  e.pod(longitudPromedio_);
  return e.volcarA(ruta);
}

bool Indice::cargarCache(const std::string &ruta, Catalogo &catalogo,
                         uint64_t huellaCsv, EstadisticasIndice &est) {
  using Reloj = std::chrono::steady_clock;
  const auto t0 = Reloj::now();

  LectorBinario l;
  if (!l.abrir(ruta)) return false;

  uint32_t magia = 0, version = 0, tamPublicacion = 0;
  uint64_t huella = 0;
  if (!l.pod(magia) || !l.pod(version) || !l.pod(huella) || !l.pod(tamPublicacion))
    return false;
  if (magia != kMagiaCache || version != kVersionCache) return false;
  if (huella != huellaCsv) return false; // el CSV cambio
  if (tamPublicacion != sizeof(Publicacion)) return false; // otro compilador

  if (!catalogo.leer(l)) return false;
  if (!vocab_.leer(l) || !sufijos_.leer(l)) return false;

  uint64_t n = 0;
  if (!l.pod(n)) return false;
  publicaciones_.clear();
  publicaciones_.resize(static_cast<size_t>(n));
  for (uint64_t i = 0; i < n; ++i) {
    if (!l.vectorPod(publicaciones_[static_cast<size_t>(i)])) return false;
  }
  if (!l.vectorPod(longitudDoc_)) return false;

  if (!l.pod(n)) return false;
  perfiles_.clear();
  perfiles_.resize(static_cast<size_t>(n));
  for (uint64_t i = 0; i < n; ++i) {
    if (!l.vectorPod(perfiles_[static_cast<size_t>(i)])) return false;
  }
  if (!l.vectorPod(normas_)) return false;

  uint64_t docs = 0;
  if (!l.pod(docs) || !l.pod(longitudPromedio_)) return false;
  numDocs_ = static_cast<size_t>(docs);
  if (numDocs_ != catalogo.tamano() || longitudDoc_.size() != numDocs_) return false;

  l.liberar();
  rellenarEstadisticas(est);
  est.segundos = std::chrono::duration<double>(Reloj::now() - t0).count();
  return true;
}

void Indice::construirPerfiles() {
  perfiles_.assign(numDocs_, {});
  normas_.assign(numDocs_, 0.0f);

  // Acumular, por documento, los kTerminosPorPerfil terminos de mayor peso
  // tf-idf. Se recorre el indice invertido una vez y se usa un monticulo
  // acotado por documento.
  std::vector<std::vector<std::pair<float, int32_t>>> monticulos(numDocs_);

  // Terminos ultra frecuentes no aportan a la similitud y encarecen todo.
  // El maximo con 4 evita que en catalogos muy chicos (pruebas unitarias) el
  // umbral valga 0 y se descarte el vocabulario completo.
  const size_t umbralFrecuencia = std::max<size_t>(numDocs_ / 8, 4);

  for (size_t id = 0; id < publicaciones_.size(); ++id) {
    const auto &lista = publicaciones_[id];
    if (lista.empty()) continue;
    if (lista.size() > umbralFrecuencia) continue;
    const std::string &t = vocab_.termino(static_cast<int>(id));
    if (t.size() < 3 || esStopword(t)) continue;
    const float pesoIdf = static_cast<float>(idf(static_cast<int>(id)));

    for (const Publicacion &p : lista) {
      const float tf = 1.0f + 2.0f * p.tfTitulo + 1.0f * p.tfSinopsis + 3.0f * p.tfTag;
      const float peso = std::log(tf) * pesoIdf;
      auto &h = monticulos[p.doc];
      if (h.size() < kTerminosPorPerfil) {
        h.emplace_back(peso, static_cast<int32_t>(id));
        std::push_heap(h.begin(), h.end(), std::greater<>());
      } else if (peso > h.front().first) {
        std::pop_heap(h.begin(), h.end(), std::greater<>());
        h.back() = {peso, static_cast<int32_t>(id)};
        std::push_heap(h.begin(), h.end(), std::greater<>());
      }
    }
  }

  for (size_t d = 0; d < numDocs_; ++d) {
    auto &h = monticulos[d];
    double norma2 = 0.0;
    perfiles_[d].reserve(h.size());
    for (const auto &par : h) {
      perfiles_[d].push_back({par.second, par.first});
      norma2 += static_cast<double>(par.first) * par.first;
    }
    std::sort(perfiles_[d].begin(), perfiles_[d].end(),
              [](const TerminoPeso &a, const TerminoPeso &b) {
                return a.termino < b.termino;
              });
    normas_[d] = static_cast<float>(std::sqrt(norma2)) + 1e-6f;
  }
}

} // namespace pf
