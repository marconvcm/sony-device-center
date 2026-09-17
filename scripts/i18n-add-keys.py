"""Add UI translation keys to apps/device-center/src/I18nManager.cpp.

The table in _initTranslations() is generated, not hand-edited: this script
reads the existing entries, merges the new ones from a JSON file and rewrites
the function with every language aligned. Existing translations are kept
verbatim.

Usage:
    python scripts/i18n-add-keys.py new-keys.json

new-keys.json maps key -> {lang: text} for every language in LANGS, e.g.

    {
      "tray_quit": {"en": "Quit", "pt_BR": "Sair", "es": "Salir", "de": "Beenden",
                    "fr": "Quitter", "ja": "終了", "ru": "Выход"}
    }

Then use the key from QML as window.tr("tray_quit") or from C++ as
controller.t("tray_quit"). Brand/feature names Sony keeps untranslated
(DSEE, Speak-to-Chat, Clear Bass, ANC, LDAC) stay as-is in every language.
"""
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "apps/device-center/src/I18nManager.cpp"

LANGS = ["en", "pt_BR", "es", "de", "fr", "ja", "ru"]
VAR = {"en": "en", "pt_BR": "pt", "es": "es", "de": "de", "fr": "fr", "ja": "ja", "ru": "ru"}
LABEL = {"en": "English (en) - Source", "pt_BR": "Português (pt_BR)", "es": "Español (es)",
         "de": "Deutsch (de)", "fr": "Français (fr)", "ja": "日本語 (ja)", "ru": "Русский (ru)"}


def main(new_keys_path: str | None) -> None:
    src = SRC.read_text(encoding="utf-8")
    body_start = src.index("void I18nManager::_initTranslations() {")
    body_end = src.index("\n}\n", body_start) + 3

    existing = {lang: {} for lang in LANGS}
    order: list[str] = []
    current = None
    for line in src[body_start:body_end].splitlines():
        m = re.match(r'\s*auto& \w+ = _strings\["([\w_]+)"\];', line)
        if m:
            current = m.group(1)
            continue
        m = re.match(r'\s*\w+\["([\w_]+)"\]\s*=\s*"(.*)";\s*$', line)
        if m and current:
            existing[current][m.group(1)] = m.group(2)
            if current == "en" and m.group(1) not in order:
                order.append(m.group(1))

    if new_keys_path:
        new = json.loads(Path(new_keys_path).read_text(encoding="utf-8"))
        for key, values in new.items():
            if key in existing["en"]:
                raise SystemExit(f"{key} already exists")
            missing = [lang for lang in LANGS if lang not in values]
            if missing:
                raise SystemExit(f"{key}: missing {missing}")
            for lang in LANGS:
                existing[lang][key] = values[lang]
            order.append(key)

    gaps = [(lang, k) for lang in LANGS for k in order if k not in existing[lang]]
    if gaps:
        raise SystemExit(f"untranslated: {gaps}")

    width = max(len(k) for k in order) + 6
    out = ["void I18nManager::_initTranslations() {"]
    for lang in LANGS:
        out.append(f"    // {LABEL[lang]}")
        out.append(f'    auto& {VAR[lang]} = _strings["{lang}"];')
        for key in order:
            lhs = f'{VAR[lang]}["{key}"]'
            out.append(f'    {lhs.ljust(width)} = "{existing[lang][key]}";')
        out.append("")
    out.pop()
    out.append("}")
    SRC.write_text(src[:body_start] + "\n".join(out) + "\n" + src[body_end:], encoding="utf-8", newline="\n")
    print(len(order), "keys x", len(LANGS), "languages")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else None)
