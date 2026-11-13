// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "vcp.h"

#include <array>

namespace Pmmt
{

namespace
{

struct NamedKey {
    QStringView key;
    VcpCode code;
};

// docs/development/architecture.md, "Data model".
constexpr std::array<NamedKey, 6> namedKeys{{
    {u"brightness", Vcp::Luminance},
    {u"contrast", Vcp::Contrast},
    {u"color_preset", Vcp::SelectColorPreset},
    {u"red_gain", Vcp::VideoGainRed},
    {u"green_gain", Vcp::VideoGainGreen},
    {u"blue_gain", Vcp::VideoGainBlue},
}};

int hexDigit(QChar c)
{
    const char16_t u = c.unicode();
    if (u >= u'0' && u <= u'9') {
        return u - u'0';
    }
    if (u >= u'a' && u <= u'f') {
        return u - u'a' + 10;
    }
    if (u >= u'A' && u <= u'F') {
        return u - u'A' + 10;
    }
    return -1;
}

} // namespace

std::optional<VcpCode> vcpCodeForKey(QStringView key)
{
    for (const auto &named : namedKeys) {
        if (named.key == key) {
            return named.code;
        }
    }
    return std::nullopt;
}

QString keyForVcpCode(VcpCode code)
{
    for (const auto &named : namedKeys) {
        if (named.code == code) {
            return named.key.toString();
        }
    }
    return {};
}

std::optional<VcpCode> parseVcpCode(QStringView text)
{
    if (text.size() != 4 || text[0] != u'0' || text[1] != u'x') {
        return std::nullopt;
    }
    const int high = hexDigit(text[2]);
    const int low = hexDigit(text[3]);
    if (high < 0 || low < 0) {
        return std::nullopt;
    }
    return static_cast<VcpCode>(high * 16 + low);
}

QString formatVcpCode(VcpCode code)
{
    return QLatin1String("0x") + QString::number(code, 16).rightJustified(2, QLatin1Char('0')).toUpper();
}

} // namespace Pmmt
