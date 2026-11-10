#include "i18n.h"
#include <unordered_map>
#include <cstring>
#include <cctype>

struct LangPair { const char* en; const char* text; };

// One file per language, each entry is {"English text", "translation"}. Add a language: create a new .inc, list it here.
static const LangPair kPt[] = {
#include "lang/pt_BR.inc"
};
static const LangPair kEs[] = {
#include "lang/es.inc"
};
static const LangPair kFr[] = {
#include "lang/fr.inc"
};
static const LangPair kRu[] = {
#include "lang/ru.inc"
};
static const LangPair kDe[] = {
#include "lang/de.inc"
};

struct LangTable { const LangPair* pairs; size_t count; const char* name; const char* code; };
#define TBL(a) a, sizeof(a) / sizeof(a[0])
static const LangTable kLangs[LANG_COUNT] = {
  {nullptr, 0, "English", "en"},
  {TBL(kPt), "Português (BR)", "pt-BR"},
  {TBL(kEs), "Español", "es"},
  {TBL(kFr), "Français", "fr"},
  {TBL(kRu), "Русский", "ru"},
  {TBL(kDe), "Deutsch", "de"},
};

static int gLang = LANG_EN;
static std::unordered_map<std::string_view, const char*> gMap;

void i18nSet(int lang) {
  if (lang < 0 || lang >= LANG_COUNT) lang = LANG_EN;
  gLang = lang;
  gMap.clear();
  const LangTable& t = kLangs[lang];
  gMap.reserve(t.count * 2);
  for (size_t i = 0; i < t.count; i++) gMap[std::string_view(t.pairs[i].en)] = t.pairs[i].text;
}
int i18nGet() { return gLang; }
const char* i18nName(int lang) { return (lang >= 0 && lang < LANG_COUNT) ? kLangs[lang].name : "?"; }
const char* i18nCode(int lang) { return (lang >= 0 && lang < LANG_COUNT) ? kLangs[lang].code : "en"; }

int i18nFromCode(const char* code) {
  if (!code || !*code) return -1;
  std::string c; for (const char* p = code; *p; p++) c += (char)(*p == '_' ? '-' : tolower((unsigned char)*p));
  if (c.rfind("pt", 0) == 0) return LANG_PT;
  for (int i = 0; i < LANG_COUNT; i++) {
    std::string k = kLangs[i].code; for (auto& ch : k) ch = (char)tolower((unsigned char)ch);
    if (c == k || c.rfind(k + "-", 0) == 0) return i;
  }
  return -1;
}

std::string_view trv(std::string_view en) {
  if (gLang == LANG_EN || en.empty()) return en;
  auto it = gMap.find(en);
  return it == gMap.end() ? en : std::string_view(it->second);
}
const char* tr(const char* en) {
  if (gLang == LANG_EN || !en || !*en) return en;
  auto it = gMap.find(std::string_view(en));
  return it == gMap.end() ? en : it->second;
}
