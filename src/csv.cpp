#include "csv.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

namespace pf {

LectorCSV::LectorCSV(const std::string &ruta) {
  std::ifstream archivo(ruta, std::ios::binary);
  if (!archivo) {
    throw std::runtime_error("No se pudo abrir el archivo CSV: " + ruta);
  }
  std::ostringstream buffer;
  buffer << archivo.rdbuf();
  contenido_ = buffer.str();

  // BOM UTF-8.
  if (contenido_.size() >= 3 &&
      static_cast<unsigned char>(contenido_[0]) == 0xEF &&
      static_cast<unsigned char>(contenido_[1]) == 0xBB &&
      static_cast<unsigned char>(contenido_[2]) == 0xBF) {
    pos_ = 3;
  }
  siguienteFila(encabezados_);
}

int LectorCSV::columna(const std::string &nombre) const {
  for (size_t i = 0; i < encabezados_.size(); ++i) {
    if (encabezados_[i] == nombre) return static_cast<int>(i);
  }
  return -1;
}

bool LectorCSV::siguienteFila(std::vector<std::string> &fila) {
  fila.clear();
  if (pos_ >= contenido_.size()) return false;

  std::string campo;
  bool entreComillas = false;
  const size_t n = contenido_.size();

  while (pos_ < n) {
    char c = contenido_[pos_];
    if (entreComillas) {
      if (c == '"') {
        if (pos_ + 1 < n && contenido_[pos_ + 1] == '"') {
          campo.push_back('"'); // comilla escapada ("")
          pos_ += 2;
          continue;
        }
        entreComillas = false;
        ++pos_;
        continue;
      }
      campo.push_back(c);
      ++pos_;
      continue;
    }
    if (c == '"') {
      entreComillas = true;
      ++pos_;
      continue;
    }
    if (c == ',') {
      fila.push_back(campo);
      campo.clear();
      ++pos_;
      continue;
    }
    if (c == '\r') {
      ++pos_;
      if (pos_ < n && contenido_[pos_] == '\n') ++pos_;
      fila.push_back(campo);
      return true;
    }
    if (c == '\n') {
      ++pos_;
      fila.push_back(campo);
      return true;
    }
    campo.push_back(c);
    ++pos_;
  }
  fila.push_back(campo);
  return true;
}

} // namespace pf
