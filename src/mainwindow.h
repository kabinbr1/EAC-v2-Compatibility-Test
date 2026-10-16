#pragma once

#include <QMainWindow>

class QLabel;
class QGridLayout;
class QFrame;
class QEvent;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void refreshChecks();
    void buildUi();
    void applyPaletteStyles();
    void changeEvent(QEvent *event) override;

    QWidget *m_cardContainer = nullptr;
    QGridLayout *m_grid = nullptr;
    QLabel *m_summaryLabel = nullptr;
};
