#include "mainwindow.h"

#include "systeminspector.h"

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPalette>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace
{
QColor blendColors(const QColor &foreground, const QColor &background, qreal amount)
{
    return QColor::fromRgbF(
        foreground.redF() * amount + background.redF() * (1.0 - amount),
        foreground.greenF() * amount + background.greenF() * (1.0 - amount),
        foreground.blueF() * amount + background.blueF() * (1.0 - amount));
}

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

    auto *refreshButton = new QPushButton(QStringLiteral("↻  Refresh"));
    refreshButton->setObjectName(QStringLiteral("refreshButton"));
    refreshButton->setCursor(Qt::PointingHandCursor);
    connect(refreshButton, &QPushButton::clicked, this, &MainWindow::refreshChecks);

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
    summaryLayout->addWidget(refreshButton);
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

    setCentralWidget(central);
    applyPaletteStyles();
}

void MainWindow::applyPaletteStyles()
{
    const QPalette palette = QApplication::palette();
    const QColor window = palette.color(QPalette::Window);
    const QColor windowText = palette.color(QPalette::WindowText);
    const QColor border = palette.color(QPalette::Mid);
    const QColor summaryBackground = blendColors(QColor(QStringLiteral("#54d6a0")), window, 0.12);
    const QColor summaryBorder = blendColors(QColor(QStringLiteral("#54d6a0")), border, 0.45);
    const QColor cardBackground = palette.color(QPalette::Base);
    const QColor mutedText = blendColors(windowText, window, 0.65);

    setStyleSheet(QStringLiteral(
        "QMainWindow, #central { background: %1; }"
        "QLabel { color: %2; }"
        "#summary { background: %3; border: 1px solid %4; border-radius: 12px; }"
        "#summaryMark { font-size: 18px; }"
        "#summaryText { color: %2; font-size: 14px; font-weight: 600; }"
        "#sectionTitle { color: %2; font-size: 17px; font-weight: 700; }"
        "#legend { color: %6; font-size: 11px; }"
        "#checkCard { background: %7; border: 1px solid %5; border-radius: 12px; }"
        "#cardTitle { color: %2; font-size: 15px; font-weight: 700; }"
        "#statusBadge { background: %7; border-radius: 7px; padding: 6px 8px;"
        " font-size: 10px; font-weight: 700; }"
        "#cardSubtitle { color: %6; font-size: 12px; }"
        "#cardDetail { color: %2; font-size: 12px; }")
        .arg(window.name())
        .arg(windowText.name())
        .arg(summaryBackground.name())
        .arg(summaryBorder.name())
        .arg(border.name())
        .arg(mutedText.name())
        .arg(cardBackground.name()));
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (event->type() == QEvent::ApplicationPaletteChange) {
        applyPaletteStyles();
    }
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
}
