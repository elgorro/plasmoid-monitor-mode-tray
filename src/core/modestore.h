// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#pragma once

#include "configerror.h"
#include "mode.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

namespace Pmmt
{

/// Loads and validates modes.toml (TOML v1.0.0). The format is described in
/// docs/usage/configuration.md.
///
/// Loading is all-or-nothing: if the file has any error, load() returns false,
/// errors() lists every problem found, and the previously loaded displays and
/// modes stay in place. That way a broken edit followed by ReloadConfig keeps
/// the daemon working with the last good config.
class ModeStore
{
public:
    /// The config format version this build understands (`version = 1`).
    static constexpr int ConfigVersion = 1;

    /// `$XDG_CONFIG_HOME/pmmt/modes.toml` (XDG Base Directory Specification;
    /// `$XDG_CONFIG_HOME` defaults to `~/.config`).
    static QString defaultConfigPath();

    /// Reads and validates the file at @p path.
    bool load(const QString &path);

    /// Validates @p data as if it were read from @p sourceName. The name only
    /// appears in error messages.
    bool loadFromData(const QByteArray &data, const QString &sourceName);

    /// Errors from the last load(); empty after a successful one.
    QList<ConfigError> errors() const;

    /// Displays in the order they appear in the file.
    QList<DisplayConfig> displays() const;

    /// Modes in the order they appear in the file.
    QList<Mode> modes() const;

    std::optional<DisplayConfig> display(const QString &key) const;
    std::optional<Mode> mode(const QString &name) const;

private:
    QList<DisplayConfig> m_displays;
    QList<Mode> m_modes;
    QList<ConfigError> m_errors;
};

} // namespace Pmmt
