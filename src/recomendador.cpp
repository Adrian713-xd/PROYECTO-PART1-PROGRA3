#include "recomendador.hpp"

#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <unordered_set>

namespace pf {
namespace {
// Un termino presente en mas del 5% del catalogo no discrimina nada y ademas
// obligaria a recorrer decenas de miles de publicaciones.
constexpr double kMaxFraccionDocumental = 0.05;
// Terminos del perfil que realmente se usan para buscar candidatos.
constexpr size_t kMaxTerminosPerfil = 80;
} // namespace

Recomendador::Recomendador(const Catalogo &catalogo, const Indice &indice)
    : catalogo_(catalogo), indice_(indice) {
  acumulador_.assign(indice.numDocumentos(), 0.0);
  marca_.assign(indice.numDocumentos(), 0);
}

// Similitud del coseno entre un perfil y todos los documentos que comparten al
// menos un termino con el. En vez de comparar contra las 35 000 peliculas, se
// recorren las listas de publicaciones de los terminos del perfil: el costo es
// proporcional al numero de documentos que de verdad pueden parecerse.
std::vector<Resultado> Recomendador::vecinos(
    const std::vector<TerminoPeso> &perfil, double normaPerfil,
    const std::vector<int> &excluir, size_t limite) const {
  std::vector<Resultado> salida;
  if (perfil.empty()) return salida;

  const uint32_t sello = ++contador_;
  std::vector<int> tocados;
  tocados.reserve(20000);

  const size_t umbralDf = std::max<size_t>(
      static_cast<size_t>(kMaxFraccionDocumental * indice_.numDocumentos()), 8);

  // Ordenar el perfil por peso descendente y quedarnos con los mas fuertes.
  std::vector<TerminoPeso> ordenado = perfil;
  std::sort(ordenado.begin(), ordenado.end(),
            [](const TerminoPeso &a, const TerminoPeso &b) {
              return a.peso > b.peso;
            });
  if (ordenado.size() > kMaxTerminosPerfil) ordenado.resize(kMaxTerminosPerfil);

  for (const TerminoPeso &par : ordenado) {
    const int id = par.termino;
    const double pesoPerfil = par.peso;
    const auto &lista = indice_.publicaciones(id);
    if (lista.size() > umbralDf) continue;

    for (const Publicacion &p : lista) {
      // El peso del documento se toma de su propio perfil tf-idf, que ya esta
      // precalculado y normalizado.
      const auto &perfilDoc = indice_.perfil(p.doc);
      auto it = std::lower_bound(
          perfilDoc.begin(), perfilDoc.end(), id,
          [](const TerminoPeso &e, int v) { return e.termino < v; });
      if (it == perfilDoc.end() || it->termino != id) continue;

      if (marca_[p.doc] != sello) {
        marca_[p.doc] = sello;
        acumulador_[p.doc] = 0.0;
        tocados.push_back(p.doc);
      }
      acumulador_[p.doc] += pesoPerfil * it->peso;
    }
  }

  std::unordered_set<int> prohibidos(excluir.begin(), excluir.end());
  salida.reserve(tocados.size());
  for (int doc : tocados) {
    if (prohibidos.count(doc)) continue;
    const double coseno =
        acumulador_[doc] / (normaPerfil * indice_.normaPerfil(doc) + 1e-9);
    if (coseno <= 0.0) continue;
    salida.push_back({doc, coseno, ""});
  }

  const size_t n = std::min(salida.size(), limite);
  std::partial_sort(salida.begin(), salida.begin() + n, salida.end(),
                    [&](const Resultado &a, const Resultado &b) {
                      if (a.puntaje != b.puntaje) return a.puntaje > b.puntaje;
                      return catalogo_[a.doc].anio > catalogo_[b.doc].anio;
                    });
  salida.resize(n);
  return salida;
}

std::vector<Resultado> Recomendador::similaresA(
    int doc, size_t limite, const std::vector<int> &excluir) const {
  std::vector<int> prohibidos = excluir;
  prohibidos.push_back(doc);
  std::vector<Resultado> res =
      vecinos(indice_.perfil(doc), indice_.normaPerfil(doc), prohibidos, limite);
  for (Resultado &r : res) {
    r.explicacion = "similar a \"" + catalogo_[doc].titulo + "\"";
  }
  return res;
}

std::vector<Resultado> Recomendador::paraUsuario(
    const std::vector<int> &megusta, const std::vector<int> &excluir,
    size_t limite) const {
  std::vector<Resultado> salida;
  if (megusta.empty()) return salida;

  // Perfil del usuario = centroide de los vectores tf-idf de sus "Me gusta".
  // Las peliculas mas recientes en la lista pesan un poco mas, para que el
  // gusto actual mande sobre el historico.
  std::unordered_map<int32_t, double> acumulado;
  double factor = 1.0;
  for (size_t i = 0; i < megusta.size(); ++i) {
    const double peso = 0.6 + 0.4 * (static_cast<double>(i + 1) / megusta.size());
    for (const TerminoPeso &par : indice_.perfil(megusta[i])) {
      acumulado[par.termino] += peso * par.peso;
    }
    factor = peso;
  }
  (void)factor;

  std::vector<TerminoPeso> perfil;
  perfil.reserve(acumulado.size());
  double norma2 = 0.0;
  for (const auto &par : acumulado) {
    perfil.push_back({par.first, static_cast<float>(par.second)});
    norma2 += par.second * par.second;
  }
  std::sort(perfil.begin(), perfil.end(),
            [](const TerminoPeso &a, const TerminoPeso &b) {
              return a.termino < b.termino;
            });

  std::vector<int> prohibidos = excluir;
  prohibidos.insert(prohibidos.end(), megusta.begin(), megusta.end());

  salida = vecinos(perfil, std::sqrt(norma2) + 1e-9, prohibidos, limite);

  // Explicar cada recomendacion con el "Me gusta" que mas se le parece.
  for (Resultado &r : salida) {
    int mejor = megusta.front();
    double mejorSim = -1.0;
    for (int m : megusta) {
      double s = 0.0;
      const auto &a = indice_.perfil(m);
      const auto &b = indice_.perfil(r.doc);
      size_t i = 0, j = 0;
      while (i < a.size() && j < b.size()) {
        if (a[i].termino == b[j].termino) {
          s += static_cast<double>(a[i].peso) * b[j].peso;
          ++i; ++j;
        } else if (a[i].termino < b[j].termino) {
          ++i;
        } else {
          ++j;
        }
      }
      s /= (indice_.normaPerfil(m) * indice_.normaPerfil(r.doc) + 1e-9);
      if (s > mejorSim) { mejorSim = s; mejor = m; }
    }
    r.explicacion = "porque te gusto \"" + catalogo_[mejor].titulo + "\"";
  }
  return salida;
}

} // namespace pf
