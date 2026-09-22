#include "buscador.hpp"

#include "normalizador.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace pf {
namespace {

// --- Parametros del algoritmo de importancia -------------------------------
constexpr double kBm25K1 = 1.2;   // saturacion de la frecuencia de termino
constexpr double kBm25B = 0.6;    // influencia de la longitud del documento
constexpr double kPesoTitulo = 6.0;
constexpr double kPesoTag = 4.0;
constexpr double kPesoSinopsis = 1.0;
constexpr double kBonusCobertura = 0.60;  // premia cumplir TODOS los terminos
constexpr double kBonusFraseTitulo = 12.0;
constexpr double kBonusFraseSinopsis = 3.5;
constexpr double kPenalizacionParcial = 0.85; // sub-cadena vs palabra completa
constexpr size_t kMaxExpansion = 3000;   // tope de terminos por sub-cadena
constexpr size_t kDocsAReordenar = 400;  // ventana de re-ranking por frase

// Componente de saturacion de BM25. El idf NO se aplica aqui: viene ya
// incorporado en el factor de cada expansion (ver expandir()).
double saturacionBm25(double tf, double longitud, double promedio) {
  if (tf <= 0.0) return 0.0;
  const double norma = 1.0 - kBm25B + kBm25B * (longitud / (promedio + 1e-9));
  return (tf * (kBm25K1 + 1.0)) / (tf + kBm25K1 * norma);
}

uint8_t campoDesdeEtiqueta(const std::string &etiqueta) {
  if (etiqueta == "director") return C_DIRECTOR;
  if (etiqueta == "reparto" || etiqueta == "actor" || etiqueta == "cast")
    return C_REPARTO;
  if (etiqueta == "genero" || etiqueta == "genre") return C_GENERO;
  if (etiqueta == "origen" || etiqueta == "origin" || etiqueta == "pais")
    return C_ORIGEN;
  if (etiqueta == "titulo" || etiqueta == "title") return C_TITULO;
  if (etiqueta == "sinopsis" || etiqueta == "plot") return C_SINOPSIS;
  return 0;
}

} // namespace

Buscador::Buscador(const Catalogo &catalogo, const Indice &indice)
    : catalogo_(catalogo), indice_(indice) {
  const size_t n = indice.numDocumentos();
  acumulado_.assign(n, 0.0);
  mejorAporte_.assign(n, 0.0);
  coincidencias_.assign(n, 0);
  marcaConsulta_.assign(n, 0);
  marcaTermino_.assign(n, 0);
}

// ---------------------------------------------------------------------------
// Expansion lexica: dado un patron, que terminos del vocabulario lo satisfacen
// y con que peso. Aqui es donde se usan los dos arboles.
// ---------------------------------------------------------------------------
std::vector<std::pair<int, double>> Buscador::expandir(
    const std::string &patron) const {
  std::vector<std::pair<int, double>> salida;
  if (patron.empty()) return salida;

  // 1) Coincidencia exacta de palabra: descenso directo en el trie de
  //    prefijos, O(|patron|). Conserva su idf completo.
  const int exacto = indice_.vocabulario().buscar(patron);

  // 2) Coincidencia por sub-cadena: descenso en el trie de sufijos.
  std::vector<int> candidatos;
  if (patron.size() >= 2) {
    candidatos = indice_.sufijos().terminosQueContienen(patron, kMaxExpansion);
  }

  // El idf de una coincidencia PARCIAL no puede ser el del termino aislado.
  // "shipp" aparece en una sola pelicula, asi que su idf es enorme y, sin
  // control, una pelicula con el apellido "Shipp" le ganaria a una titulada
  // "Ghost Ship" en la busqueda de "ship". Lo correcto es tratar todas las
  // expansiones como UN concepto y usar el idf de la union: el patron "ship"
  // aparece en miles de documentos (relationship, friendship, worship...) y
  // por lo tanto discrimina poco.
  double dfUnion = 0.0;
  for (int id : candidatos) dfUnion += indice_.frecuenciaDocumental(id);
  const double idfTope = indice_.idfDesdeDf(dfUnion);

  salida.reserve(candidatos.size() + 1);
  if (exacto >= 0) salida.emplace_back(exacto, indice_.idf(exacto));
  for (int id : candidatos) {
    if (id == exacto) continue;
    const size_t largo = indice_.vocabulario().termino(id).size();
    const double cobertura = static_cast<double>(patron.size()) / largo;
    const double idfEfectivo = std::min(indice_.idf(id), idfTope);
    if (idfEfectivo <= 0.0) continue;
    salida.emplace_back(id, kPenalizacionParcial * cobertura * cobertura * idfEfectivo);
  }
  return salida;
}

// ---------------------------------------------------------------------------
// Parseo de la consulta
// ---------------------------------------------------------------------------
Consulta Buscador::interpretar(const std::string &linea) const {
  Consulta q;
  q.textoNormalizado = recortar(normalizar(linea));

  // Separar la linea original en palabras conservando los ':' de las etiquetas.
  std::vector<std::string> palabras;
  {
    std::string actual;
    for (char c : linea) {
      if (c == ' ' || c == '\t' || c == ',') {
        if (!actual.empty()) palabras.push_back(actual);
        actual.clear();
      } else if (c != '"') {
        actual.push_back(c);
      }
    }
    if (!actual.empty()) palabras.push_back(actual);
  }

  uint8_t campoActual = C_CUALQUIERA;
  bool hayEtiqueta = false;
  for (std::string bruta : palabras) {
    const size_t dosPuntos = bruta.find(':');
    if (dosPuntos != std::string::npos && dosPuntos > 0) {
      const std::string etiqueta = recortar(normalizar(bruta.substr(0, dosPuntos)));
      const std::string resto = bruta.substr(dosPuntos + 1);
      if (etiqueta == "anio" || etiqueta == "year" || etiqueta == "ano") {
        q.anio = std::atoi(resto.c_str());
        continue;
      }
      const uint8_t campo = campoDesdeEtiqueta(etiqueta);
      if (campo != 0) {
        campoActual = campo;
        hayEtiqueta = true;
        bruta = resto;
        if (recortar(bruta).empty()) continue;
      }
    }

    std::vector<std::string> tokens = tokenizar(bruta);
    for (const std::string &tok : tokens) {
      // Las palabras vacias solo se descartan en texto libre y solo si la
      // consulta tiene mas contenido.
      if (!hayEtiqueta && esStopword(tok) && tokens.size() > 1) continue;
      TerminoConsulta tc;
      tc.patron = tok;
      tc.campos = campoActual;
      tc.expansion = expandir(tok);
      if (!tc.expansion.empty()) q.terminos.push_back(std::move(tc));
    }
  }

  // Si todo eran palabras vacias, se reintenta sin filtrarlas.
  if (q.terminos.empty() && q.anio < 0 && !q.textoNormalizado.empty()) {
    for (const std::string &tok : tokenizar(linea)) {
      TerminoConsulta tc;
      tc.patron = tok;
      tc.campos = C_CUALQUIERA;
      tc.expansion = expandir(tok);
      if (!tc.expansion.empty()) q.terminos.push_back(std::move(tc));
    }
  }

  q.valida = !q.terminos.empty() || q.anio > 0;
  if (!q.valida) {
    q.mensaje = "Ninguna palabra de la consulta aparece en el catalogo.";
  }
  return q;
}

// ---------------------------------------------------------------------------
// Busqueda y ranking
// ---------------------------------------------------------------------------
std::vector<Resultado> Buscador::buscar(const Consulta &q, size_t limite) const {
  std::vector<Resultado> salida;
  if (!q.valida) return salida;

  const uint32_t selloConsulta = ++contadorConsulta_;
  const double promedio = indice_.longitudPromedio();
  std::vector<int> tocados;
  tocados.reserve(8192);
  std::vector<int> tocadosTermino;

  for (const TerminoConsulta &tc : q.terminos) {
    // Un termino de la consulta puede expandirse a muchos terminos del
    // vocabulario. El aporte del termino de consulta es el MAXIMO entre sus
    // expansiones, no la suma: asi "bar" no infla su puntaje solo porque un
    // documento contenga bar, barn, barber y barrel a la vez.
    const uint32_t selloTermino = ++contadorTermino_;
    tocadosTermino.clear();

    for (const auto &par : tc.expansion) {
      const int id = par.first;
      const double factor = par.second; // peso lexico x idf efectivo
      if (factor <= 0.0) continue;

      for (const Publicacion &p : indice_.publicaciones(id)) {
        const uint8_t comunes = static_cast<uint8_t>(tc.campos & p.campos);
        if (comunes == 0) continue;
        double tf = 0.0;
        if (comunes & C_TITULO) tf += kPesoTitulo * p.tfTitulo;
        if (comunes & C_SINOPSIS) tf += kPesoSinopsis * p.tfSinopsis;
        if (comunes & (C_DIRECTOR | C_REPARTO | C_GENERO | C_ORIGEN))
          tf += kPesoTag * p.tfTag;
        if (tf <= 0.0) continue;

        const double aporte =
            factor * saturacionBm25(tf, indice_.longitud(p.doc), promedio);

        if (marcaTermino_[p.doc] != selloTermino) {
          marcaTermino_[p.doc] = selloTermino;
          mejorAporte_[p.doc] = aporte;
          tocadosTermino.push_back(p.doc);
        } else if (aporte > mejorAporte_[p.doc]) {
          mejorAporte_[p.doc] = aporte;
        }
      }
    }

    for (int doc : tocadosTermino) {
      if (marcaConsulta_[doc] != selloConsulta) {
        marcaConsulta_[doc] = selloConsulta;
        acumulado_[doc] = 0.0;
        coincidencias_[doc] = 0;
        tocados.push_back(doc);
      }
      acumulado_[doc] += mejorAporte_[doc];
      ++coincidencias_[doc];
    }
  }

  // Bonus de cobertura: los documentos que satisfacen mas terminos suben.
  // Esto implementa la semantica "y/o" del enunciado: una frase devuelve
  // peliculas con cualquiera de las palabras, pero las que tienen todas
  // aparecen primero.
  const double numTerminos = static_cast<double>(q.terminos.size());
  salida.reserve(tocados.size());
  for (int doc : tocados) {
    if (q.anio > 0 && catalogo_[doc].anio != q.anio) continue;
    double puntaje = acumulado_[doc];
    if (numTerminos > 0) {
      puntaje *= 1.0 + kBonusCobertura * (coincidencias_[doc] / numTerminos);
    }
    salida.push_back({doc, puntaje, ""});
  }

  // Caso "solo filtro por anio", sin terminos de texto.
  if (q.terminos.empty() && q.anio > 0) {
    for (size_t d = 0; d < catalogo_.tamano(); ++d) {
      if (catalogo_[d].anio == q.anio) {
        salida.push_back({static_cast<int>(d), 1.0, ""});
      }
    }
  }
  if (salida.empty()) return salida;

  auto porPuntaje = [&](const Resultado &a, const Resultado &b) {
    if (a.puntaje != b.puntaje) return a.puntaje > b.puntaje;
    if (catalogo_[a.doc].anio != catalogo_[b.doc].anio)
      return catalogo_[a.doc].anio > catalogo_[b.doc].anio;
    return catalogo_[a.doc].tituloNorm < catalogo_[b.doc].tituloNorm;
  };

  const size_t ventana = std::min(salida.size(), kDocsAReordenar);
  std::partial_sort(salida.begin(), salida.begin() + ventana, salida.end(),
                    porPuntaje);
  salida.resize(ventana);

  reordenarPorFrase(q, salida);
  std::stable_sort(salida.begin(), salida.end(), porPuntaje);
  if (salida.size() > limite) salida.resize(limite);
  explicar(q, salida);
  return salida;
}

// Re-ranking de la ventana superior: premiar la aparicion de la FRASE completa.
// Se hace solo sobre kDocsAReordenar documentos porque exige normalizar la
// sinopsis, y eso no vale la pena sobre las 35 000 peliculas.
void Buscador::reordenarPorFrase(const Consulta &q,
                                 std::vector<Resultado> &res) const {
  if (q.textoNormalizado.size() < 3 || q.terminos.size() < 2) return;
  const std::string &frase = q.textoNormalizado;
  for (Resultado &r : res) {
    const Pelicula &p = catalogo_[r.doc];
    if (p.tituloNorm.find(frase) != std::string::npos) {
      r.puntaje += kBonusFraseTitulo;
      continue;
    }
    if (normalizar(p.sinopsis).find(frase) != std::string::npos) {
      r.puntaje += kBonusFraseSinopsis;
    }
  }
}

void Buscador::explicar(const Consulta &q, std::vector<Resultado> &res) const {
  for (Resultado &r : res) {
    const std::string &tn = catalogo_[r.doc].tituloNorm;
    if (q.terminos.size() > 1 && !q.textoNormalizado.empty() &&
        tn.find(q.textoNormalizado) != std::string::npos) {
      r.explicacion = "frase exacta en el titulo";
      continue;
    }
    size_t enTitulo = 0;
    for (const TerminoConsulta &tc : q.terminos) {
      if (tn.find(tc.patron) != std::string::npos) ++enTitulo;
    }
    if (enTitulo > 0) {
      r.explicacion = std::to_string(enTitulo) + "/" +
                      std::to_string(q.terminos.size()) + " en el titulo";
    } else {
      r.explicacion = std::to_string(coincidencias_[r.doc]) + "/" +
                      std::to_string(q.terminos.size()) + " en sinopsis/etiquetas";
    }
  }
}

} // namespace pf
