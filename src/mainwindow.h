#pragma once

#include <QMainWindow>

class QLabel;
class QGridLayout;
class QFrame;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

private:
    void refreshChecks();
    void buildUi();

    QWidget *m_cardContainer = nullptr;
    QGridLayout *m_grid = nullptr;
    QLabel *m_summaryLabel = nullptr;
    QLabel *m_kernelLabel = nullptr;
};
