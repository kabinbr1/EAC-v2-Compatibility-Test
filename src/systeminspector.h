#pragma once

#include <QString>
#include <QVector>

enum class CheckStatus
{
    supported,
    unsupported,
    unknown
};

struct SecurityCheck
{
    QString title;
    QString subtitle;
    QString detail;
    CheckStatus status = CheckStatus::unknown;
    QString statusLabel;
};

class SystemInspector
{
public:
    static QVector<SecurityCheck> inspect();
    static QString kernelRelease();
};
