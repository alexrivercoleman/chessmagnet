// Game storage in NVS flash (via Arduino's Preferences library).
//
// NVS is a small key-value store designed for frequent updates: it spreads
// writes across its flash pages (wear levelling), and a single key is
// replaced atomically, so a power cut mid-save leaves the old game intact.
#pragma once
#include <ChessCore.h>
#include <Preferences.h>

namespace hw {

class Storage : public cm::Storage {
 public:
  bool begin() { return prefs_.begin("chessmagnet", false); }
  bool load(const char* key, std::string& out) override {
    if (!prefs_.isKey(key)) return false;
    out = prefs_.getString(key, "").c_str();
    return !out.empty();
  }
  bool save(const char* key, const std::string& value) override {
    return prefs_.putString(key, value.c_str()) == value.size();
  }

 private:
  Preferences prefs_;
};

}  // namespace hw
