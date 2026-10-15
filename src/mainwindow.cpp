#include "mainwindow.h"

#include "systeminspector.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QString statusText(const SecurityCheck &check)
{
    const QString label = check.statusLabel.isEmpty() ? QStringLiteral("SUPPORTED") : check.statusLabel;
    switch (check.status) {
    case CheckStatus::supported:
        return QStringLiteral("✓  %1").arg(label);
    case CheckStatus::unsupported:
        return QStringLiteral("✕  %1").arg(
            check.statusLabel.isEmpty() ? QStringLiteral("NOT DETECTED") : check.statusLabel);
    case CheckStatus::unknown:
        return QStringLiteral("?  %1").arg(
            check.statusLabel.isEmpty() ? QStringLiteral("UNKNOWN") : check.statusLabel);
    }
    return QStringLiteral("?  UNKNOWN");
}

QString statusColor(CheckStatus status)
{
    switch (status) {
    case CheckStatus::supported:
        return QStringLiteral("#54d6a0");
    case CheckStatus::unsupported:
        return QStringLiteral("#ff7d83");
    case CheckStatus::unknown:
        return QStringLiteral("#f4bd62");
    }
    return QStringLiteral("#f4bd62");
}

QFrame *makeCheckCard(const SecurityCheck &check)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("checkCard"));
    auto *layout = new QVBoxLayout(card);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(10);

    auto *topRow = new QHBoxLayout;
    auto *title = new QLabel(check.title);
    title->setObjectName(QStringLiteral("cardTitle"));
    title->setWordWrap(true);
    topRow->addWidget(title, 1);

    auto *status = new QLabel(statusText(check));
    status->setObjectName(QStringLiteral("statusBadge"));
    status->setStyleSheet(QStringLiteral("color: %1;").arg(statusColor(check.status)));
    topRow->addWidget(status, 0, Qt::AlignRight | Qt::AlignVCenter);
    layout->addLayout(topRow);

    auto *subtitle = new QLabel(check.subtitle);
    subtitle->setObjectName(QStringLiteral("cardSubtitle"));
    layout->addWidget(subtitle);

    auto *detail = new QLabel(check.detail);
    detail->setObjectName(QStringLiteral("cardDetail"));
    detail->setWordWrap(true);
    layout->addWidget(detail);

    return card;
}
} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    buildUi();
    refreshChecks();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("EAC v2 Compatibility Test"));
    setMinimumSize(760, 640);
    resize(1000, 820);

    auto *central = new QWidget;
    central->setObjectName(QStringLiteral("central"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(34, 28, 34, 24);
    root->setSpacing(22);

    auto *header = new QHBoxLayout;
    auto *branding = new QVBoxLayout;
    auto *eyebrow = new QLabel(QStringLiteral("EAC V2 COMPATIBILITY"));
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    branding->addWidget(eyebrow);

    auto *heading = new QLabel(QStringLiteral("Compatibility checks"));
    heading->setObjectName(QStringLiteral("heading"));
    branding->addWidget(heading);

    auto *intro = new QLabel(QStringLiteral("Checking the security requirements for protected gameplay."));
    intro->setObjectName(QStringLiteral("intro"));
    branding->addWidget(intro);
    header->addLayout(branding, 1);

    auto *refreshButton = new QPushButton(QStringLiteral("↻  Refresh"));
    refreshButton->setObjectName(QStringLiteral("refreshButton"));
    refreshButton->setCursor(Qt::PointingHandCursor);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshChecks);
    header->addWidget(refreshButton, 0, Qt::AlignTop);
    root->addLayout(header);

    auto *summary = new QFrame;
    summary->setObjectName(QStringLiteral("summary"));
    auto *summaryLayout = new QHBoxLayout(summary);
    summaryLayout->setContentsMargins(20, 16, 20, 16);
    auto *summaryMark = new QLabel(QStringLiteral("●"));
    summaryMark->setObjectName(QStringLiteral("summaryMark"));
    summaryMark->setStyleSheet(QStringLiteral("color: #54d6a0;"));
    summaryLayout->addWidget(summaryMark);
    m_summaryLabel = new QLabel;
    m_summaryLabel->setObjectName(QStringLiteral("summaryText"));
    summaryLayout->addWidget(m_summaryLabel, 1);
    root->addWidget(summary);

    auto *section = new QHBoxLayout;
    auto *sectionTitle = new QLabel(QStringLiteral("Requirement checks"));
    sectionTitle->setObjectName(QStringLiteral("sectionTitle"));
    section->addWidget(sectionTitle);
    section->addStretch();
    auto *legend = new QLabel(QStringLiteral("✓ Positive     ✕ Negative     ? Unverified"));
    legend->setObjectName(QStringLiteral("legend"));
    section->addWidget(legend);
    root->addLayout(section);

    auto *scrollArea = new QScrollArea;
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    m_cardContainer = new QWidget;
    m_grid = new QGridLayout(m_cardContainer);
    m_grid->setContentsMargins(0, 0, 0, 0);
    m_grid->setHorizontalSpacing(14);
    m_grid->setVerticalSpacing(14);
    m_grid->setColumnStretch(0, 1);
    m_grid->setColumnStretch(1, 1);
    scrollArea->setWidget(m_cardContainer);
    root->addWidget(scrollArea, 1);

    m_kernelLabel = new QLabel;
    m_kernelLabel->setObjectName(QStringLiteral("footer"));
    root->addWidget(m_kernelLabel);

    setCentralWidget(central);
    setStyleSheet(QStringLiteral(
        "QMainWindow, #central { background: #10151f; }"
        "QLabel { color: #e8edf5; }"
        "#eyebrow { color: #65c9ef; font-size: 11px; font-weight: 700; letter-spacing: 2px; }"
        "#heading { color: #f2f5fb; font-size: 30px; font-weight: 700; }"
        "#intro { color: #9aa8bb; font-size: 14px; }"
        "#refreshButton { color: #d9e6f4; background: #1c2635; border: 1px solid #34445a;"
        " border-radius: 9px; padding: 10px 15px; font-weight: 600; }"
        "#refreshButton:hover { background: #26364b; border-color: #5baed0; }"
        "#summary { background: #182a2c; border: 1px solid #28504a; border-radius: 12px; }"
        "#summaryMark { font-size: 18px; }"
        "#summaryText { color: #cce9dd; font-size: 14px; font-weight: 600; }"
        "#sectionTitle { color: #f0f3f8; font-size: 17px; font-weight: 700; }"
        "#legend { color: #8c9bb0; font-size: 11px; }"
        "#checkCard { background: #171e2b; border: 1px solid #293548; border-radius: 12px; }"
        "#cardTitle { color: #f0f3f8; font-size: 15px; font-weight: 700; }"
        "#statusBadge { background: #202a39; border-radius: 7px; padding: 6px 8px;"
        " font-size: 10px; font-weight: 700; }"
        "#cardSubtitle { color: #99a8bc; font-size: 12px; }"
        "#cardDetail { color: #c4ccda; font-size: 12px; line-height: 1.35; }"
        "#footer { color: #78879b; font-size: 11px; padding-top: 3px; }"
        "QScrollArea { background: transparent; }"
        "QScrollBar:vertical { background: #10151f; width: 8px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: #344257; border-radius: 4px; min-height: 24px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"));
}

void MainWindow::refreshChecks()
{
    while (QLayoutItem *item = m_grid->takeAt(0)) {
        if (QWidget *widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const QVector<SecurityCheck> checks = SystemInspector::inspect();
    int supported = 0;
    int unsupported = 0;
    int unknown = 0;
    for (qsizetype i = 0; i < checks.size(); ++i) {
        const SecurityCheck &check = checks.at(i);
        m_grid->addWidget(makeCheckCard(check), i / 2, i % 2);
        switch (check.status) {
        case CheckStatus::supported:
            ++supported;
            break;
        case CheckStatus::unsupported:
            ++unsupported;
            break;
        case CheckStatus::unknown:
            ++unknown;
            break;
        }
    }

    m_summaryLabel->setText(
        QStringLiteral("%1 positive  ·  %2 negative  ·  %3 unknown")
            .arg(supported)
            .arg(unsupported)
            .arg(unknown));
    m_kernelLabel->setText(
        QStringLiteral("Running kernel: %1   ·   Refreshed just now").arg(SystemInspector::kernelRelease()));
}
