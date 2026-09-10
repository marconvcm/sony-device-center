#pragma once

#include <QMap>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

namespace sony::devicecenter {

class I18nManager {
public:
    static I18nManager& instance();

    I18nManager();

    [[nodiscard]] QString translate(const QString& key, const QString& langCode) const;
    [[nodiscard]] QVariantList availableLanguages() const;

private:
    void _initTranslations();
    QMap<QString, QMap<QString, QString>> _strings;
};

} // namespace sony::devicecenter
