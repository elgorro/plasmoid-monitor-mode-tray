// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "modestore.h"

#include <QFile>
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>

#include <toml++/toml.hpp>

#include <algorithm>
#include <limits>
#include <string_view>

namespace Pmmt
{

namespace
{

QString toQString(std::string_view s)
{
    return QString::fromUtf8(s.data(), static_cast<qsizetype>(s.size()));
}

QString quoted(const QString &s)
{
    return QLatin1Char('`') + s + QLatin1Char('`');
}

/// Collects errors and walks the parsed document.
class Validator
{
public:
    explicit Validator(const QString &file)
        : m_file(file)
    {
    }

    QList<ConfigError> errors;
    QList<DisplayConfig> displays;
    QList<Mode> modes;

    void run(const toml::table &root)
    {
        checkVersion(root);
        if (!errors.isEmpty()) {
            // A different version may mean anything; don't pile on errors.
            return;
        }

        for (const auto &[key, node] : root) {
            const QString name = toQString(key.str());
            if (name != QLatin1String("version") && name != QLatin1String("displays") && name != QLatin1String("modes")) {
                error(key.source(), QStringLiteral("unknown key %1").arg(quoted(name)));
            }
        }

        if (const toml::node *node = root.get("displays")) {
            if (const toml::table *table = expectTable(*node, QStringLiteral("displays"))) {
                readDisplays(*table);
            }
        }
        if (const toml::node *node = root.get("modes")) {
            if (const toml::table *table = expectTable(*node, QStringLiteral("modes"))) {
                readModes(*table);
            }
        }
    }

private:
    QString m_file;

    // toml++ tables iterate sorted by key, not in file order. Lists keep file
    // order, so sort them by where each entry starts.
    template<typename T>
    static void sortByPosition(QList<std::pair<toml::source_position, T>> &entries, QList<T> &out)
    {
        std::stable_sort(entries.begin(), entries.end(), [](const auto &a, const auto &b) {
            return a.first < b.first;
        });
        for (auto &entry : entries) {
            out.append(std::move(entry.second));
        }
    }

    void error(const toml::source_region &where, const QString &message)
    {
        errors.append(ConfigError{m_file, static_cast<int>(where.begin.line), static_cast<int>(where.begin.column), message});
    }

    const toml::table *expectTable(const toml::node &node, const QString &what)
    {
        if (const toml::table *table = node.as_table()) {
            return table;
        }
        error(node.source(), QStringLiteral("%1 must be a table").arg(quoted(what)));
        return nullptr;
    }

    std::optional<QString> expectString(const toml::node &node, const QString &what)
    {
        if (const auto *value = node.as_string()) {
            return toQString(value->get());
        }
        error(node.source(), QStringLiteral("%1 must be a string").arg(quoted(what)));
        return std::nullopt;
    }

    std::optional<VcpValue> expectVcpValue(const toml::node &node, const QString &what)
    {
        const auto *value = node.as_integer();
        if (!value) {
            error(node.source(), QStringLiteral("%1 must be an integer").arg(quoted(what)));
            return std::nullopt;
        }
        const int64_t v = value->get();
        if (v < 0 || v > std::numeric_limits<VcpValue>::max()) {
            error(node.source(), QStringLiteral("%1 = %2 is out of range 0…65535").arg(quoted(what)).arg(v));
            return std::nullopt;
        }
        return static_cast<VcpValue>(v);
    }

    void checkVersion(const toml::table &root)
    {
        const toml::node *node = root.get("version");
        if (!node) {
            errors.append(
                ConfigError{m_file,
                            1,
                            1,
                            QStringLiteral("missing %1 (expected `version = %2`)").arg(quoted(QStringLiteral("version"))).arg(ModeStore::ConfigVersion)});
            return;
        }
        const auto *value = node->as_integer();
        if (!value || value->get() != ModeStore::ConfigVersion) {
            error(node->source(), QStringLiteral("unsupported config version, expected `version = %1`").arg(ModeStore::ConfigVersion));
        }
    }

    // Display IDs are `MFG:MODEL:SERIAL`. MFG is the EDID manufacturer ID:
    // three letters A–Z (VESA E-EDID 1.4, "ID Manufacturer Name"). The serial
    // may be empty, since not every monitor stores one in its EDID.
    static bool isValidDisplayId(const QString &id)
    {
        static const QRegularExpression re(QStringLiteral(R"(^[A-Z]{3}:[^:]+:[^:]*$)"));
        return re.match(id).hasMatch();
    }

    void readDisplays(const toml::table &table)
    {
        QHash<QString, QString> keyById;
        QList<std::pair<toml::source_position, DisplayConfig>> found;
        for (const auto &[tomlKey, node] : table) {
            const QString key = toQString(tomlKey.str());
            if (key.isEmpty()) {
                error(tomlKey.source(), QStringLiteral("display name must not be empty"));
                continue;
            }
            const toml::table *entry = expectTable(node, QStringLiteral("displays.") + key);
            if (!entry) {
                continue;
            }

            DisplayConfig display;
            display.key = key;
            bool ok = true;

            for (const auto &[fieldKey, value] : *entry) {
                const QString field = toQString(fieldKey.str());
                if (field == QLatin1String("id")) {
                    if (auto id = expectString(value, field)) {
                        if (!isValidDisplayId(*id)) {
                            error(value.source(), QStringLiteral("display id %1 is not of the form `MFG:MODEL:SERIAL` (see `pmmt displays`)").arg(quoted(*id)));
                            ok = false;
                        } else if (keyById.contains(*id)) {
                            error(value.source(), QStringLiteral("display id %1 is already used by display %2").arg(quoted(*id), quoted(keyById.value(*id))));
                            ok = false;
                        } else {
                            keyById.insert(*id, key);
                            display.id = *id;
                        }
                    } else {
                        ok = false;
                    }
                } else if (field == QLatin1String("connector")) {
                    if (auto connector = expectString(value, field)) {
                        display.connector = *connector;
                    } else {
                        ok = false;
                    }
                } else if (field == QLatin1String("after_brightness")) {
                    ok = readAfterBrightness(value, display) && ok;
                } else {
                    error(fieldKey.source(), QStringLiteral("unknown key %1 in display %2").arg(quoted(field), quoted(key)));
                    ok = false;
                }
            }

            if (!entry->contains("id")) {
                error(entry->source(), QStringLiteral("display %1 has no `id`").arg(quoted(key)));
                ok = false;
            }

            if (ok) {
                std::sort(display.afterBrightness.begin(), display.afterBrightness.end());
                found.append({entry->source().begin, display});
            }
        }
        sortByPosition(found, displays);
    }

    bool readAfterBrightness(const toml::node &node, DisplayConfig &display)
    {
        const toml::array *array = node.as_array();
        if (!array) {
            error(node.source(), QStringLiteral("`after_brightness` must be an array of VCP codes like \"0xE8\""));
            return false;
        }
        bool ok = true;
        for (const toml::node &item : *array) {
            const auto *text = item.as_string();
            const auto code = text ? parseVcpCode(toQString(text->get())) : std::nullopt;
            if (!code) {
                error(item.source(), QStringLiteral("`after_brightness` entries must be VCP codes like \"0xE8\""));
                ok = false;
            } else if (*code == Vcp::Luminance) {
                error(item.source(), QStringLiteral("`after_brightness` cannot contain brightness (0x10) itself"));
                ok = false;
            } else if (display.afterBrightness.contains(*code)) {
                error(item.source(), QStringLiteral("%1 is listed twice in `after_brightness`").arg(formatVcpCode(*code)));
                ok = false;
            } else {
                display.afterBrightness.append(*code);
            }
        }
        return ok;
    }

    void readModes(const toml::table &table)
    {
        QSet<QString> displayKeys;
        for (const auto &display : std::as_const(displays)) {
            displayKeys.insert(display.key);
        }
        QList<std::pair<toml::source_position, Mode>> found;

        for (const auto &[tomlKey, node] : table) {
            const QString name = toQString(tomlKey.str());
            if (name.isEmpty()) {
                error(tomlKey.source(), QStringLiteral("mode name must not be empty"));
                continue;
            }
            const toml::table *entry = expectTable(node, QStringLiteral("modes.") + name);
            if (!entry) {
                continue;
            }

            Mode mode;
            mode.name = name;
            bool ok = true;

            for (const auto &[fieldKey, value] : *entry) {
                const QString field = toQString(fieldKey.str());
                if (field == QLatin1String("display")) {
                    if (auto display = expectString(value, field)) {
                        if (!displayKeys.contains(*display)) {
                            // Also hit when the display entry itself had errors.
                            error(value.source(), QStringLiteral("mode %1 refers to unknown display %2").arg(quoted(name), quoted(*display)));
                            ok = false;
                        } else {
                            mode.display = *display;
                        }
                    } else {
                        ok = false;
                    }
                } else if (field == QLatin1String("vcp")) {
                    ok = readRawVcp(value, mode) && ok;
                } else if (const auto code = vcpCodeForKey(field)) {
                    if (const auto v = expectVcpValue(value, field)) {
                        mode.values.insert(*code, *v);
                    } else {
                        ok = false;
                    }
                } else {
                    error(fieldKey.source(), QStringLiteral("unknown key %1 in mode %2").arg(quoted(field), quoted(name)));
                    ok = false;
                }
            }

            if (!entry->contains("display")) {
                error(entry->source(), QStringLiteral("mode %1 has no `display`").arg(quoted(name)));
                ok = false;
            }
            if (ok && mode.values.isEmpty()) {
                error(entry->source(), QStringLiteral("mode %1 sets no values").arg(quoted(name)));
                ok = false;
            }

            if (ok) {
                found.append({entry->source().begin, mode});
            }
        }
        sortByPosition(found, modes);
    }

    bool readRawVcp(const toml::node &node, Mode &mode)
    {
        const toml::table *table = expectTable(node, QStringLiteral("modes.%1.vcp").arg(mode.name));
        if (!table) {
            return false;
        }
        bool ok = true;
        for (const auto &[tomlKey, value] : *table) {
            const QString key = toQString(tomlKey.str());
            const auto code = parseVcpCode(key);
            if (!code) {
                error(tomlKey.source(), QStringLiteral("%1 is not a VCP code; write it as \"0xNN\"").arg(quoted(key)));
                ok = false;
                continue;
            }
            // One spelling per code, so a mode can't set the same code twice.
            const QString named = keyForVcpCode(*code);
            if (!named.isEmpty()) {
                error(tomlKey.source(), QStringLiteral("%1 has a named key, use %2 instead").arg(formatVcpCode(*code), quoted(named)));
                ok = false;
                continue;
            }
            if (const auto v = expectVcpValue(value, QStringLiteral("vcp.\"%1\"").arg(key))) {
                mode.values.insert(*code, *v);
            } else {
                ok = false;
            }
        }
        return ok;
    }
};

} // namespace

QString ModeStore::defaultConfigPath()
{
    // GenericConfigLocation honours $XDG_CONFIG_HOME and falls back to
    // ~/.config (XDG Base Directory Specification).
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) + QLatin1String("/pmmt/modes.toml");
}

bool ModeStore::load(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        m_errors = {ConfigError{path, 0, 0, QStringLiteral("cannot read file: %1").arg(file.errorString())}};
        return false;
    }
    return loadFromData(file.readAll(), path);
}

bool ModeStore::loadFromData(const QByteArray &data, const QString &sourceName)
{
    const QByteArray sourceUtf8 = sourceName.toUtf8();
    toml::table root;
    try {
        root = toml::parse(std::string_view(data.constData(), static_cast<size_t>(data.size())),
                           std::string_view(sourceUtf8.constData(), static_cast<size_t>(sourceUtf8.size())));
    } catch (const toml::parse_error &e) {
        m_errors = {ConfigError{sourceName, static_cast<int>(e.source().begin.line), static_cast<int>(e.source().begin.column), toQString(e.description())}};
        return false;
    }

    Validator validator(sourceName);
    validator.run(root);
    m_errors = validator.errors;
    if (!m_errors.isEmpty()) {
        return false;
    }
    m_displays = validator.displays;
    m_modes = validator.modes;
    return true;
}

QList<ConfigError> ModeStore::errors() const
{
    return m_errors;
}

QList<DisplayConfig> ModeStore::displays() const
{
    return m_displays;
}

QList<Mode> ModeStore::modes() const
{
    return m_modes;
}

std::optional<DisplayConfig> ModeStore::display(const QString &key) const
{
    for (const auto &display : m_displays) {
        if (display.key == key) {
            return display;
        }
    }
    return std::nullopt;
}

std::optional<Mode> ModeStore::mode(const QString &name) const
{
    for (const auto &mode : m_modes) {
        if (mode.name == name) {
            return mode;
        }
    }
    return std::nullopt;
}

} // namespace Pmmt
