#include "estado_usuario.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace pf {
namespace {

bool contiene(const std::vector<int> &v, int x) {
  return std::find(v.begin(), v.end(), x) != v.end();
}

bool alternar(std::vector<int> &v, int x) {
  auto it = std::find(v.begin(), v.end(), x);
  if (it != v.end()) {
    v.erase(it);
    return false;
  }
  v.push_back(x);
  return true;
}

} // namespace

void EstadoUsuario::cargar(const std::string &ruta, const Catalogo &catalogo) {
  meGusta_.clear();
  verMasTarde_.clear();
  vistas_.clear();

  std::ifstream archivo(ruta);
  if (!archivo) return; // primera ejecucion: no hay estado previo

  std::string linea;
  while (std::getline(archivo, linea)) {
    if (linea.empty() || linea[0] == '#') continue;
    const size_t tab = linea.find('\t');
    if (tab == std::string::npos) continue;
    const std::string tipo = linea.substr(0, tab);
    const std::string clave = linea.substr(tab + 1);
    const int doc = catalogo.buscarPorClave(clave);
    if (doc < 0) continue; // la pelicula ya no esta en el catalogo
    if (tipo == "LIKE" && !contiene(meGusta_, doc)) meGusta_.push_back(doc);
    else if (tipo == "VER" && !contiene(verMasTarde_, doc)) verMasTarde_.push_back(doc);
    else if (tipo == "VISTA" && !contiene(vistas_, doc)) vistas_.push_back(doc);
  }
}

bool EstadoUsuario::guardar(const std::string &ruta,
                            const Catalogo &catalogo) const {
  std::ofstream archivo(ruta, std::ios::trunc);
  if (!archivo) return false;
  archivo << "# Estado de UTEC Stream. Formato: TIPO<TAB>titulo_normalizado|anio\n";
  for (int d : meGusta_) archivo << "LIKE\t" << catalogo.clave(d) << "\n";
  for (int d : verMasTarde_) archivo << "VER\t" << catalogo.clave(d) << "\n";
  const size_t desde = vistas_.size() > 200 ? vistas_.size() - 200 : 0;
  for (size_t i = desde; i < vistas_.size(); ++i) {
    archivo << "VISTA\t" << catalogo.clave(vistas_[i]) << "\n";
  }
  return archivo.good();
}

bool EstadoUsuario::alternarMeGusta(int doc) { return alternar(meGusta_, doc); }
bool EstadoUsuario::alternarVerMasTarde(int doc) { return alternar(verMasTarde_, doc); }

void EstadoUsuario::marcarVisto(int doc) {
  auto it = std::find(vistas_.begin(), vistas_.end(), doc);
  if (it != vistas_.end()) vistas_.erase(it);
  vistas_.push_back(doc);
}

bool EstadoUsuario::tieneMeGusta(int doc) const { return contiene(meGusta_, doc); }
bool EstadoUsuario::tieneVerMasTarde(int doc) const { return contiene(verMasTarde_, doc); }

} // namespace pf
