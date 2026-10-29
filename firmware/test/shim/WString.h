// The part of Arduino's String that the firmware's portable code uses, for host tests.
#pragma once
#include <stdlib.h>
#include <string.h>

#include <string>

class String {
 public:
  String() = default;
  String(const char *s) : s_(s ? s : "") {}
  String(const std::string &s) : s_(s) {}
  unsigned length() const { return unsigned(s_.size()); }
  const char *c_str() const { return s_.c_str(); }
  char operator[](unsigned i) const { return i < s_.size() ? s_[i] : 0; }
  bool startsWith(const char *p) const { return s_.compare(0, strlen(p), p) == 0; }
  bool endsWith(const char *p) const {
    const size_t n = strlen(p);
    return s_.size() >= n && s_.compare(s_.size() - n, n, p) == 0;
  }
  int indexOf(char c, unsigned from = 0) const { return find(s_.find(c, from)); }
  int indexOf(const char *p, unsigned from = 0) const { return find(s_.find(p, from)); }
  String substring(unsigned from, unsigned to = 0xffffffff) const {
    if (from > s_.size()) return String();
    return String(s_.substr(from, (to > s_.size() ? s_.size() : to) - from));
  }
  void replace(const char *a, const char *b) {
    const size_t la = strlen(a), lb = strlen(b);
    for (size_t i = s_.find(a); la && i != std::string::npos; i = s_.find(a, i + lb)) s_.replace(i, la, b);
  }
  void remove(unsigned idx, unsigned count = 0xffffffff) { if (idx < s_.size()) s_.erase(idx, count); }
  long toInt() const { return atol(s_.c_str()); }
  void trim() {
    const size_t a = s_.find_first_not_of(" \t\r\n"), b = s_.find_last_not_of(" \t\r\n");
    s_ = a == std::string::npos ? "" : s_.substr(a, b - a + 1);
  }
  bool operator==(const char *o) const { return s_ == o; }
  bool operator==(const String &o) const { return s_ == o.s_; }
  bool operator!=(const String &o) const { return s_ != o.s_; }
  String &operator+=(const String &o) {
    s_ += o.s_;
    return *this;
  }
  friend String operator+(const String &a, const String &b) { return String(a.s_ + b.s_); }

 private:
  static int find(size_t i) { return i == std::string::npos ? -1 : int(i); }
  std::string s_;
};
