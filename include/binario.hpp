// binario.hpp — Serializacion binaria simple para el cache del indice.
//
// El escritor acumula en memoria y vuelca el archivo de una sola vez; el lector
// carga el archivo completo y avanza sobre un buffer. Con ~200 MB de cache eso
// es mucho mas rapido que hacer miles de llamadas pequenas a ostream/istream.
#pragma once
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace pf {

class EscritorBinario {
public:
  template <typename T> void pod(const T &valor) {
    static_assert(std::is_trivially_copyable<T>::value, "solo tipos POD");
    const char *p = reinterpret_cast<const char *>(&valor);
    buffer_.append(p, sizeof(T));
  }

  void cadena(const std::string &s) {
    pod<uint32_t>(static_cast<uint32_t>(s.size()));
    buffer_.append(s.data(), s.size());
  }

  template <typename T> void vectorPod(const std::vector<T> &v) {
    static_assert(std::is_trivially_copyable<T>::value, "solo tipos POD");
    pod<uint64_t>(static_cast<uint64_t>(v.size()));
    if (!v.empty()) {
      buffer_.append(reinterpret_cast<const char *>(v.data()),
                     v.size() * sizeof(T));
    }
  }

  void vectorCadenas(const std::vector<std::string> &v) {
    pod<uint64_t>(static_cast<uint64_t>(v.size()));
    for (const std::string &s : v) cadena(s);
  }

  bool volcarA(const std::string &ruta) const {
    std::ofstream f(ruta, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
    return f.good();
  }

  size_t bytes() const { return buffer_.size(); }
  void reservar(size_t n) { buffer_.reserve(n); }

private:
  std::string buffer_;
};

class LectorBinario {
public:
  bool abrir(const std::string &ruta) {
    std::ifstream f(ruta, std::ios::binary | std::ios::ate);
    if (!f) return false;
    const std::streamsize n = f.tellg();
    if (n < 0) return false;
    f.seekg(0);
    buffer_.resize(static_cast<size_t>(n));
    if (n > 0) f.read(&buffer_[0], n);
    pos_ = 0;
    return f.good() || f.eof();
  }

  bool ok() const { return !error_; }

  template <typename T> bool pod(T &destino) {
    if (error_ || pos_ + sizeof(T) > buffer_.size()) return fallar();
    std::memcpy(&destino, buffer_.data() + pos_, sizeof(T));
    pos_ += sizeof(T);
    return true;
  }

  bool cadena(std::string &destino) {
    uint32_t n = 0;
    if (!pod(n)) return false;
    if (pos_ + n > buffer_.size()) return fallar();
    destino.assign(buffer_.data() + pos_, n);
    pos_ += n;
    return true;
  }

  template <typename T> bool vectorPod(std::vector<T> &destino) {
    uint64_t n = 0;
    if (!pod(n)) return false;
    const size_t bytes = static_cast<size_t>(n) * sizeof(T);
    if (pos_ + bytes > buffer_.size()) return fallar();
    destino.resize(static_cast<size_t>(n));
    if (n > 0) std::memcpy(destino.data(), buffer_.data() + pos_, bytes);
    pos_ += bytes;
    return true;
  }

  bool vectorCadenas(std::vector<std::string> &destino) {
    uint64_t n = 0;
    if (!pod(n)) return false;
    if (n > buffer_.size()) return fallar(); // cota superior barata
    destino.resize(static_cast<size_t>(n));
    for (uint64_t i = 0; i < n; ++i) {
      if (!cadena(destino[static_cast<size_t>(i)])) return false;
    }
    return true;
  }

  void liberar() {
    std::string().swap(buffer_);
    pos_ = 0;
  }

private:
  bool fallar() {
    error_ = true;
    return false;
  }
  std::string buffer_;
  size_t pos_ = 0;
  bool error_ = false;
};

} // namespace pf
