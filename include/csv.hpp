// csv.hpp — Lector de archivos CSV conforme a RFC 4180.
//
// El dataset de Wikipedia contiene comillas dobles escapadas ("") y saltos de
// linea CRLF *dentro* de las sinopsis, por lo que un split ingenuo por comas o
// por std::getline rompe el archivo. Este lector maneja ambos casos.
#pragma once
#include <string>
#include <vector>

namespace pf {

class LectorCSV {
public:
  // Carga el archivo completo en memoria (el dataset son ~81 MB).
  // Lanza std::runtime_error si no se puede abrir.
  explicit LectorCSV(const std::string &ruta);

  // Lee la siguiente fila. Devuelve false al llegar al final del archivo.
  bool siguienteFila(std::vector<std::string> &fila);

  const std::vector<std::string> &encabezados() const { return encabezados_; }

  // Indice de la columna con ese nombre, o -1 si no existe.
  int columna(const std::string &nombre) const;

private:
  std::string contenido_;
  size_t pos_ = 0;
  std::vector<std::string> encabezados_;
};

} // namespace pf
