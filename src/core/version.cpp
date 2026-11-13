// SPDX-FileCopyrightText: 2026 elgorro
// SPDX-License-Identifier: MIT

#include "version.h"

#include "pmmt_version.h"

namespace Pmmt
{

QString versionString()
{
    return QStringLiteral(PMMT_VERSION_STRING);
}

} // namespace Pmmt
