// Localization. All in-game text is written in English in the source code; tr() looks the text up in the
// active language table (src/lang/*.inc) and falls back to the English original when there is no entry.
#pragma once
#include <string>
#include <string_view>

enum Lang { LANG_EN, LANG_PT, LANG_ES, LANG_FR, LANG_RU, LANG_DE, LANG_COUNT };

void i18nSet(int lang);              // switch the active language (rebuilds the lookup table)
int i18nGet();
inline bool i18nActive() { return i18nGet() != LANG_EN; }
const char* i18nName(int lang);      // native name, e.g. "Deutsch" (always drawable with the built-in font)
const char* i18nCode(int lang);      // "en", "pt-BR", "es", "fr", "ru", "de"
int i18nFromCode(const char* code);  // best match for a locale like "pt_BR", "de-AT" (-1 when unsupported)

std::string_view trv(std::string_view en);   // translated text, or the argument itself when no entry exists
const char* tr(const char* en);              // same, for string literals (result stays valid: tables are static)
