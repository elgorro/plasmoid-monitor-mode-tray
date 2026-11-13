// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "fakebackend.h"

#include <toml++/toml.hpp>

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

std::optional<VcpValue> toVcpValue(const toml::node &node)
{
    const auto *integer = node.as_integer();
    if (!integer || integer->get() < 0 || integer->get() > std::numeric_limits<VcpValue>::max()) {
        return std::nullopt;
    }
    return static_cast<VcpValue>(integer->get());
}

/// Fixture parser. Problems are collected as "line N: message".
class FixtureReader
{
public:
    QStringList errors;
    DisplayInfo info;
    QMap<VcpCode, FakeBackend::Feature> features;
    QList<FakeBackend::Effect> effects;

    void read(const toml::table &root)
    {
        for (const auto &[key, node] : root) {
            const auto name = key.str();
            if (name == "version") {
                if (node.value<int64_t>() != 1) {
                    fail(node, QStringLiteral("unsupported fixture version, expected 1"));
                }
            } else if (name == "display") {
                readDisplay(node);
            } else if (name == "vcp") {
                readFeatures(node);
            } else if (name == "effects") {
                readEffects(node);
            } else {
                fail(key.source(), QStringLiteral("unknown key `%1`").arg(toQString(name)));
            }
        }
        if (!root.contains("display")) {
            errors << QStringLiteral("missing [display] table");
        }
    }

private:
    void fail(const toml::source_region &where, const QString &message)
    {
        errors << QStringLiteral("line %1: %2").arg(where.begin.line).arg(message);
    }
    void fail(const toml::node &node, const QString &message)
    {
        fail(node.source(), message);
    }

    void readDisplay(const toml::node &node)
    {
        const toml::table *table = node.as_table();
        if (!table) {
            fail(node, QStringLiteral("`display` must be a table"));
            return;
        }
        QString manufacturer, model, serial;
        for (const auto &[key, value] : *table) {
            const auto name = key.str();
            const auto text = value.value<std::string_view>();
            if (name == "product_code" || name == "mccs_version") {
                continue; // informational
            }
            if (!text) {
                fail(value, QStringLiteral("`display.%1` must be a string").arg(toQString(name)));
            } else if (name == "manufacturer") {
                manufacturer = toQString(*text);
            } else if (name == "model") {
                model = toQString(*text);
            } else if (name == "serial") {
                serial = toQString(*text);
            } else if (name == "connector") {
                info.connector = toQString(*text);
            } else {
                fail(key.source(), QStringLiteral("unknown key `display.%1`").arg(toQString(name)));
            }
        }
        if (manufacturer.isEmpty() || model.isEmpty()) {
            fail(node, QStringLiteral("`display` needs `manufacturer` and `model`"));
        }
        // Same ID scheme as DisplayConfig::id.
        info.id = manufacturer + QLatin1Char(':') + model + QLatin1Char(':') + serial;
        info.label = model;
        info.reachable = true;
    }

    void readFeatures(const toml::node &node)
    {
        const toml::table *table = node.as_table();
        if (!table) {
            fail(node, QStringLiteral("`vcp` must be a table"));
            return;
        }
        for (const auto &[key, value] : *table) {
            const auto code = parseVcpCode(toQString(key.str()));
            const toml::table *entry = value.as_table();
            if (!code || !entry) {
                fail(key.source(), QStringLiteral("expected a [vcp.0xNN] table"));
                continue;
            }
            FakeBackend::Feature feature;
            for (const auto &[fieldKey, field] : *entry) {
                const auto name = fieldKey.str();
                if (name == "value" || name == "max") {
                    const auto v = toVcpValue(field);
                    if (!v) {
                        fail(field, QStringLiteral("`%1` must be an integer 0…65535").arg(toQString(name)));
                    } else {
                        (name == "value" ? feature.value : feature.maximum) = *v;
                    }
                } else if (name == "allowed") {
                    const toml::array *array = field.as_array();
                    if (!array) {
                        fail(field, QStringLiteral("`allowed` must be an array"));
                        continue;
                    }
                    for (const toml::node &item : *array) {
                        if (const auto v = toVcpValue(item)) {
                            feature.allowed << *v;
                        } else {
                            fail(item, QStringLiteral("`allowed` entries must be integers 0…65535"));
                        }
                    }
                } else if (name == "writable") {
                    if (const auto b = field.value<bool>()) {
                        feature.writable = *b;
                    } else {
                        fail(field, QStringLiteral("`writable` must be a boolean"));
                    }
                } else if (name == "names") {
                    continue; // OSD names, for humans
                } else {
                    fail(fieldKey.source(), QStringLiteral("unknown key `%1`").arg(toQString(name)));
                }
            }
            features.insert(*code, feature);
        }
    }

    void readEffects(const toml::node &node)
    {
        const toml::array *array = node.as_array();
        if (!array) {
            fail(node, QStringLiteral("`effects` must be an array of tables"));
            return;
        }
        for (const toml::node &item : *array) {
            const toml::table *entry = item.as_table();
            if (!entry) {
                fail(item, QStringLiteral("`effects` entries must be tables"));
                continue;
            }
            FakeBackend::Effect effect;
            bool hasTrigger = false;
            for (const auto &[key, value] : *entry) {
                const auto name = key.str();
                if (name == "on_write") {
                    const auto v = toVcpValue(value);
                    if (!v || *v > 0xFF) {
                        fail(value, QStringLiteral("`on_write` must be a VCP code"));
                    } else {
                        effect.onWrite = static_cast<VcpCode>(*v);
                        hasTrigger = true;
                    }
                } else if (name == "value") {
                    effect.value = toVcpValue(value);
                    if (!effect.value) {
                        fail(value, QStringLiteral("`value` must be an integer 0…65535"));
                    }
                } else if (name == "sets") {
                    const toml::table *sets = value.as_table();
                    if (!sets) {
                        fail(value, QStringLiteral("`sets` must be a table"));
                        continue;
                    }
                    for (const auto &[codeKey, codeValue] : *sets) {
                        const auto code = parseVcpCode(toQString(codeKey.str()));
                        const auto v = toVcpValue(codeValue);
                        if (!code || !v) {
                            fail(codeKey.source(), QStringLiteral("`sets` entries must be 0xNN = value"));
                        } else {
                            effect.sets.insert(*code, *v);
                        }
                    }
                } else {
                    fail(key.source(), QStringLiteral("unknown key `%1` in effect").arg(toQString(name)));
                }
            }
            if (!hasTrigger) {
                fail(item, QStringLiteral("effect without `on_write`"));
            }
            effects << effect;
        }
    }
};

} // namespace

QString FakeBackend::loadFixture(const QString &path, QString *error)
{
    const QByteArray pathUtf8 = path.toUtf8();
    toml::table root;
    try {
        root = toml::parse_file(std::string_view(pathUtf8.constData(), static_cast<size_t>(pathUtf8.size())));
    } catch (const toml::parse_error &e) {
        if (error) {
            *error = QStringLiteral("%1:%2: %3").arg(path).arg(e.source().begin.line).arg(toQString(e.description()));
        }
        return {};
    }

    FixtureReader reader;
    reader.read(root);
    if (!reader.errors.isEmpty()) {
        if (error) {
            *error = path + QLatin1String(": ") + reader.errors.join(QLatin1String("; "));
        }
        return {};
    }
    addDisplay(reader.info, reader.features, reader.effects);
    return reader.info.id;
}

void FakeBackend::addDisplay(const DisplayInfo &info, const QMap<VcpCode, Feature> &features, const QList<Effect> &effects)
{
    m_monitors.append(Monitor{info, features, effects});
}

void FakeBackend::setFeature(const QString &displayId, VcpCode code, const Feature &feature)
{
    for (auto &monitor : m_monitors) {
        if (monitor.info.id == displayId) {
            monitor.features.insert(code, feature);
        }
    }
}

void FakeBackend::setReachable(const QString &displayId, bool reachable)
{
    for (auto &monitor : m_monitors) {
        if (monitor.info.id == displayId) {
            monitor.info.reachable = reachable;
        }
    }
}

void FakeBackend::failOn(VcpCode code)
{
    m_failing.append(code);
}

void FakeBackend::clearFailures()
{
    m_failing.clear();
}

void FakeBackend::setDelayMs(int ms)
{
    m_delayMs = ms;
}

qint64 FakeBackend::elapsedMs() const
{
    return m_elapsedMs;
}

QList<FakeBackend::Write> FakeBackend::writes() const
{
    return m_writes;
}

void FakeBackend::clearWrites()
{
    m_writes.clear();
}

int FakeBackend::readCount() const
{
    return m_reads;
}

std::optional<VcpValue> FakeBackend::value(const QString &displayId, VcpCode code) const
{
    for (const auto &monitor : m_monitors) {
        if (monitor.info.id == displayId && monitor.features.contains(code)) {
            return monitor.features.value(code).value;
        }
    }
    return std::nullopt;
}

QList<DisplayInfo> FakeBackend::displays()
{
    QList<DisplayInfo> out;
    for (const auto &monitor : std::as_const(m_monitors)) {
        out << monitor.info;
    }
    return out;
}

FakeBackend::Monitor *FakeBackend::access(const QString &displayId, VcpCode code, QString *error)
{
    m_elapsedMs += m_delayMs;
    auto setError = [error](const QString &message) {
        if (error) {
            *error = message;
        }
    };
    Monitor *monitor = nullptr;
    for (auto &candidate : m_monitors) {
        if (candidate.info.id == displayId) {
            monitor = &candidate;
            break;
        }
    }
    if (!monitor) {
        setError(QStringLiteral("no display with ID %1").arg(displayId));
        return nullptr;
    }
    if (!monitor->info.reachable) {
        setError(QStringLiteral("display %1 does not answer").arg(displayId));
        return nullptr;
    }
    if (m_failing.contains(code)) {
        setError(QStringLiteral("simulated DDC/CI failure on %1").arg(formatVcpCode(code)));
        return nullptr;
    }
    if (!monitor->features.contains(code)) {
        setError(QStringLiteral("%1 is not supported by the monitor").arg(formatVcpCode(code)));
        return nullptr;
    }
    return monitor;
}

std::optional<VcpReading> FakeBackend::readVcp(const QString &displayId, VcpCode code, QString *error)
{
    ++m_reads;
    const Monitor *monitor = access(displayId, code, error);
    if (!monitor) {
        return std::nullopt;
    }
    const Feature &feature = monitor->features[code];
    return VcpReading{feature.value, feature.maximum};
}

bool FakeBackend::writeVcp(const QString &displayId, VcpCode code, VcpValue value, QString *error)
{
    Monitor *monitor = access(displayId, code, error);
    if (!monitor) {
        return false;
    }
    Feature &feature = monitor->features[code];
    QString problem;
    if (!feature.writable) {
        problem = QStringLiteral("%1 is read-only").arg(formatVcpCode(code));
    } else if (feature.maximum > 0 && value > feature.maximum) {
        problem = QStringLiteral("%1 = %2 is above the maximum %3").arg(formatVcpCode(code)).arg(value).arg(feature.maximum);
    } else if (!feature.allowed.isEmpty() && !feature.allowed.contains(value)) {
        problem = QStringLiteral("%1 does not accept %2").arg(formatVcpCode(code)).arg(value);
    }
    if (!problem.isEmpty()) {
        if (error) {
            *error = problem;
        }
        return false;
    }

    feature.value = value;
    m_writes.append(Write{displayId, code, value});
    for (const Effect &effect : std::as_const(monitor->effects)) {
        if (effect.onWrite == code && (!effect.value || *effect.value == value)) {
            for (auto it = effect.sets.cbegin(); it != effect.sets.cend(); ++it) {
                monitor->features[it.key()].value = it.value();
            }
        }
    }
    return true;
}

FakeBrightness::FakeBrightness(FakeBackend *backend)
    : m_backend(backend)
{
}

void FakeBrightness::setResult(Result result)
{
    m_result = result;
}

QList<FakeBrightness::Call> FakeBrightness::calls() const
{
    return m_calls;
}

BrightnessSync::Result FakeBrightness::setBrightness(const DisplayInfo &display, VcpValue value, VcpValue maximum, QString *error)
{
    m_calls.append(Call{display.id, value, maximum});
    if (m_result == Result::Failed && error) {
        *error = QStringLiteral("simulated PowerDevil failure");
    }
    if (m_result == Result::Done && m_backend) {
        // PowerDevil writes 0x10 itself.
        if (!m_backend->writeVcp(display.id, Vcp::Luminance, value, error)) {
            return Result::Failed;
        }
    }
    return m_result;
}

} // namespace Pmmt
