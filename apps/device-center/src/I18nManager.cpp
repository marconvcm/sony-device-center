#include "I18nManager.h"

namespace sony::devicecenter {

I18nManager& I18nManager::instance() {
    static I18nManager s_instance;
    return s_instance;
}

I18nManager::I18nManager() {
    _initTranslations();
}

QVariantList I18nManager::availableLanguages() const {
    QVariantList list;
    list.append(QVariantMap{{"code", "en"},    {"name", "English"}});
    list.append(QVariantMap{{"code", "pt_BR"}, {"name", "Português"}});
    list.append(QVariantMap{{"code", "es"},    {"name", "Español"}});
    list.append(QVariantMap{{"code", "de"},    {"name", "Deutsch"}});
    list.append(QVariantMap{{"code", "fr"},    {"name", "Français"}});
    list.append(QVariantMap{{"code", "ja"},    {"name", "日本語"}});
    return list;
}

QString I18nManager::translate(const QString& key, const QString& langCode) const {
    auto langIt = _strings.find(langCode);
    if (langIt != _strings.end()) {
        auto keyIt = langIt->find(key);
        if (keyIt != langIt->end()) {
            return *keyIt;
        }
    }
    // Fallback to English
    auto enIt = _strings.find("en");
    if (enIt != _strings.end()) {
        auto keyIt = enIt->find(key);
        if (keyIt != enIt->end()) {
            return *keyIt;
        }
    }
    return key;
}

void I18nManager::_initTranslations() {
    // English (en) - Source
    auto& en = _strings["en"];
    en["nav_overview"]        = "Overview";
    en["nav_noise_control"]   = "Noise Control";
    en["nav_equalizer"]       = "Equalizer";
    en["nav_audio_features"]  = "Audio Features";
    en["nav_device_switcher"] = "Device Switcher";
    en["nav_settings"]        = "Settings";

    en["settings_eyebrow"]    = "Preferences & Details";
    en["settings_title"]      = "Settings";
    en["settings_subtitle"]   = "System startup, language selection, application version, and community links.";
    en["system_preferences"]  = "System & Interface";
    en["init_with_os"]        = "Launch at System Startup";
    en["init_with_os_desc"]   = "Automatically launch Sony Device Center when you log in to your desktop.";
    en["language"]            = "Display Language";
    en["language_desc"]       = "Select your preferred language for the user interface.";
    en["about_app"]           = "About Application";
    en["disclaimer"]         = "Sony Device Center is an independent open-source project, free for anyone to use, study and modify. It is not affiliated with, endorsed by, or connected to Sony Corporation. Sony and all product names are trademarks of their respective owners.";
    en["app_version"]         = "Version";
    en["links_support"]       = "Community & Support";
    en["donate_desc"]         = "Enjoying Sony Device Center? Support the development or check out the open-source repository.";
    en["btn_github"]          = "GitHub";
    en["btn_donate"]          = "Donate";

    en["connected"]           = "Connected";
    en["disconnected"]        = "Disconnected";
    en["battery"]             = "Battery";
    en["charging"]            = "Charging";
    en["ready"]               = "Ready";
    en["noise_cancelling"]    = "Noise Cancelling";
    en["ambient_sound"]       = "Ambient Sound";
    en["noise_control_off"]   = "Off";
    en["ambient_level"]       = "Ambient Level";
    en["focus_on_voice"]      = "Focus on Voice";
    en["clear_bass"]          = "Clear Bass";
    en["active"]              = "Active";
    en["connect"]             = "Connect";
    en["available"]           = "Available";

    // Português (pt_BR)
    auto& pt = _strings["pt_BR"];
    pt["nav_overview"]        = "Visão Geral";
    pt["nav_noise_control"]   = "Controle de Ruído";
    pt["nav_equalizer"]       = "Equalizador";
    pt["nav_audio_features"]  = "Recursos de Áudio";
    pt["nav_device_switcher"] = "Alternar Dispositivo";
    pt["nav_settings"]        = "Configurações";

    pt["settings_eyebrow"]    = "Preferências & Detalhes";
    pt["settings_title"]      = "Configurações";
    pt["settings_subtitle"]   = "Inicialização com o sistema, idioma, versão do aplicativo e links da comunidade.";
    pt["system_preferences"]  = "Sistema & Interface";
    pt["init_with_os"]        = "Iniciar com o Sistema Operacional";
    pt["init_with_os_desc"]   = "Inicia o Sony Device Center automaticamente ao fazer login no seu computador.";
    pt["language"]            = "Idioma de Exibição";
    pt["language_desc"]       = "Selecione seu idioma preferido para a interface.";
    pt["about_app"]           = "Sobre o Aplicativo";
    pt["disclaimer"]         = "O Sony Device Center é um projeto open source independente, livre para qualquer pessoa usar, estudar e modificar. Não é afiliado, endossado nem conectado à Sony Corporation. Sony e todos os nomes de produtos são marcas de seus respectivos proprietários.";
    pt["app_version"]         = "Versão";
    pt["links_support"]       = "Comunidade & Apoio";
    pt["donate_desc"]         = "Gostando do Sony Device Center? Apoie o desenvolvimento contínuo ou confira o código-fonte.";
    pt["btn_github"]          = "GitHub";
    pt["btn_donate"]          = "Doar / Apoiar";

    pt["connected"]           = "Conectado";
    pt["disconnected"]        = "Desconectado";
    pt["battery"]             = "Bateria";
    pt["charging"]            = "Carregando";
    pt["ready"]               = "Pronto";
    pt["noise_cancelling"]    = "Cancelamento de Ruído";
    pt["ambient_sound"]       = "Som Ambiente";
    pt["noise_control_off"]   = "Desativado";
    pt["ambient_level"]       = "Nível Ambiente";
    pt["focus_on_voice"]      = "Foco na Voz";
    pt["clear_bass"]          = "Clear Bass";
    pt["active"]              = "Ativo";
    pt["connect"]             = "Conectar";
    pt["available"]           = "Disponível";

    // Español (es)
    auto& es = _strings["es"];
    es["nav_overview"]        = "Resumen";
    es["nav_noise_control"]   = "Control de Ruido";
    es["nav_equalizer"]       = "Ecualizador";
    es["nav_audio_features"]  = "Funciones de Audio";
    es["nav_device_switcher"] = "Cambiar Dispositivo";
    es["nav_settings"]        = "Ajustes";

    es["settings_eyebrow"]    = "Preferencias y Detalles";
    es["settings_title"]      = "Ajustes";
    es["settings_subtitle"]   = "Inicio con el sistema, idioma, versión de la aplicación y enlaces de la comunidad.";
    es["system_preferences"]  = "Sistema e Interfaz";
    es["init_with_os"]        = "Iniciar con el Sistema Operativo";
    es["init_with_os_desc"]   = "Inicia automáticamente Sony Device Center al iniciar sesión en su ordenador.";
    es["language"]            = "Idioma";
    es["language_desc"]       = "Seleccione su idioma preferido para la interfaz.";
    es["about_app"]           = "Acerca de la Aplicación";
    es["disclaimer"]         = "Sony Device Center es un proyecto de código abierto independiente, libre para que cualquiera lo use, estudie y modifique. No está afiliado, respaldado ni conectado con Sony Corporation. Sony y todos los nombres de productos son marcas de sus respectivos propietarios.";
    es["app_version"]         = "Versión";
    es["links_support"]       = "Comunidad y Soporte";
    es["donate_desc"]         = "¿Disfrutando de Sony Device Center? Apoye el desarrollo continuo o explore el código fuente.";
    es["btn_github"]          = "GitHub";
    es["btn_donate"]          = "Donar";

    es["connected"]           = "Conectado";
    es["disconnected"]        = "Desconectado";
    es["battery"]             = "Batería";
    es["charging"]            = "Cargando";
    es["ready"]               = "Listo";
    es["noise_cancelling"]    = "Cancelación de Ruido";
    es["ambient_sound"]       = "Sonido Ambiente";
    es["noise_control_off"]   = "Desactivado";
    es["ambient_level"]       = "Nivel Ambiente";
    es["focus_on_voice"]      = "Enfoque en Voz";
    es["clear_bass"]          = "Clear Bass";
    es["active"]              = "Activo";
    es["connect"]             = "Conectar";
    es["available"]           = "Disponible";

    // Deutsch (de)
    auto& de = _strings["de"];
    de["nav_overview"]        = "Übersicht";
    de["nav_noise_control"]   = "Geräuschkontrolle";
    de["nav_equalizer"]       = "Equalizer";
    de["nav_audio_features"]  = "Audiofunktionen";
    de["nav_device_switcher"] = "Geräte wechseln";
    de["nav_settings"]        = "Einstellungen";

    de["settings_eyebrow"]    = "Präferenzen & Details";
    de["settings_title"]      = "Einstellungen";
    de["settings_subtitle"]   = "Systemstart, Sprachauswahl, Anwendungsversion und Community-Links.";
    de["system_preferences"]  = "System & Oberfläche";
    de["init_with_os"]        = "Beim Systemstart ausführen";
    de["init_with_os_desc"]   = "Startet das Sony Device Center automatisch bei der Anmeldung am Desktop.";
    de["language"]            = "Sprache";
    de["language_desc"]       = "Wählen Sie Ihre bevorzugte Sprache für die Benutzeroberfläche.";
    de["about_app"]           = "Über die Anwendung";
    de["disclaimer"]         = "Sony Device Center ist ein unabhängiges Open-Source-Projekt, das jeder frei nutzen, untersuchen und verändern kann. Es steht in keiner Verbindung zur Sony Corporation und wird von ihr weder unterstützt noch empfohlen. Sony und alle Produktnamen sind Marken ihrer jeweiligen Inhaber.";
    de["app_version"]         = "Version";
    de["links_support"]       = "Community & Unterstützung";
    de["donate_desc"]         = "Gefällt Ihnen Sony Device Center? Unterstützen Sie die Weiterentwicklung oder besuchen Sie das Repository.";
    de["btn_github"]          = "GitHub";
    de["btn_donate"]          = "Spenden";

    de["connected"]           = "Verbunden";
    de["disconnected"]        = "Getrennt";
    de["battery"]             = "Akku";
    de["charging"]            = "Lädt";
    de["ready"]               = "Bereit";
    de["noise_cancelling"]    = "Geräuschminimierung";
    de["ambient_sound"]       = "Umgebungsklang";
    de["noise_control_off"]   = "Aus";
    de["ambient_level"]       = "Umgebungspegel";
    de["focus_on_voice"]      = "Fokus auf Stimme";
    de["clear_bass"]          = "Clear Bass";
    de["active"]              = "Aktiv";
    de["connect"]             = "Verbinden";
    de["available"]           = "Verfügbar";

    // Français (fr)
    auto& fr = _strings["fr"];
    fr["nav_overview"]        = "Aperçu";
    fr["nav_noise_control"]   = "Contrôle du Bruit";
    fr["nav_equalizer"]       = "Égaliseur";
    fr["nav_audio_features"]  = "Fonctions Audio";
    fr["nav_device_switcher"] = "Changer d'Appareil";
    fr["nav_settings"]        = "Paramètres";

    fr["settings_eyebrow"]    = "Préférences & Détails";
    fr["settings_title"]      = "Paramètres";
    fr["settings_subtitle"]   = "Démarrage système, langue, version de l'application et liens communautaires.";
    fr["system_preferences"]  = "Système & Interface";
    fr["init_with_os"]        = "Lancer au Démarrage du Système";
    fr["init_with_os_desc"]   = "Lance automatiquement Sony Device Center lors de la connexion à votre session.";
    fr["language"]            = "Langue d'Affichage";
    fr["language_desc"]       = "Sélectionnez votre langue préférée pour l'interface utilisateur.";
    fr["about_app"]           = "À propos de l'application";
    fr["disclaimer"]         = "Sony Device Center est un projet open source indépendant, que chacun est libre d'utiliser, d'étudier et de modifier. Il n'est ni affilié à Sony Corporation, ni approuvé ni soutenu par elle. Sony et tous les noms de produits sont des marques de leurs propriétaires respectifs.";
    fr["app_version"]         = "Version";
    fr["links_support"]       = "Communauté & Soutien";
    fr["donate_desc"]         = "Vous appréciez Sony Device Center ? Soutenez le développement continu ou consultez le code source.";
    fr["btn_github"]          = "GitHub";
    fr["btn_donate"]          = "Faire un don";

    fr["connected"]           = "Connecté";
    fr["disconnected"]        = "Déconnecté";
    fr["battery"]             = "Batterie";
    fr["charging"]            = "En charge";
    fr["ready"]               = "Prêt";
    fr["noise_cancelling"]    = "Réduction de bruit";
    fr["ambient_sound"]       = "Bruit ambiant";
    fr["noise_control_off"]   = "Désactivé";
    fr["ambient_level"]       = "Niveau ambiant";
    fr["focus_on_voice"]      = "Priorité à la voix";
    fr["clear_bass"]          = "Clear Bass";
    fr["active"]              = "Actif";
    fr["connect"]             = "Connecter";
    fr["available"]           = "Disponible";

    // 日本語 (ja)
    auto& ja = _strings["ja"];
    ja["nav_overview"]        = "概要";
    ja["nav_noise_control"]   = "ノイズコントロール";
    ja["nav_equalizer"]       = "イコライザー";
    ja["nav_audio_features"]  = "オーディオ機能";
    ja["nav_device_switcher"] = "デバイス切り替え";
    ja["nav_settings"]        = "設定";

    ja["settings_eyebrow"]    = "設定と情報";
    ja["settings_title"]      = "設定";
    ja["settings_subtitle"]   = "OS自動起動、言語設定、アプリのバージョン、コミュニティリンク。";
    ja["system_preferences"]  = "システムとインターフェース";
    ja["init_with_os"]        = "OS起動時に自動開始";
    ja["init_with_os_desc"]   = "ログイン時にSony Device Centerを自動的に起動します。";
    ja["language"]            = "表示言語";
    ja["language_desc"]       = "UIの表示言語を選択します。";
    ja["about_app"]           = "アプリについて";
    ja["disclaimer"]         = "Sony Device Center は独立したオープンソースプロジェクトであり、誰でも自由に使用・研究・改変できます。ソニー株式会社とは提携しておらず、承認や支援も受けていません。Sony および各製品名は各権利者の商標です。";
    ja["app_version"]         = "バージョン";
    ja["links_support"]       = "コミュニティと支援";
    ja["donate_desc"]         = "Sony Device Centerを気に入っていただけましたら、開発のご支援やリポジトリの確認をお願いします。";
    ja["btn_github"]          = "GitHub";
    ja["btn_donate"]          = "支援・寄付";

    ja["connected"]           = "接続済み";
    ja["disconnected"]        = "未接続";
    ja["battery"]             = "バッテリー";
    ja["charging"]            = "充電中";
    ja["ready"]               = "準備完了";
    ja["noise_cancelling"]    = "ノイズキャンセリング";
    ja["ambient_sound"]       = "外音取り込み (アンビエント)";
    ja["noise_control_off"]   = "オフ";
    ja["ambient_level"]       = "外音取り込みレベル";
    ja["focus_on_voice"]      = "ボイスフォーカス";
    ja["clear_bass"]          = "クリアベース";
    ja["active"]              = "接続中";
    ja["connect"]             = "接続";
    ja["available"]           = "利用可能";
}

} // namespace sony::devicecenter
