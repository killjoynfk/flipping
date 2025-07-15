#pragma once

#include <QMainWindow>
#include <QPixmap>
#include <QTimer>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void startFlipAnimation();
    void animateStep();
    void preapreDst();

    struct PanelData
    {
        qreal angle   = 0.0;
        bool  flipped = false;
    };


    QVector<PanelData> m_panels;

    int m_rows = 6;
    int m_cols = 10;

    QPixmap m_imageFrom, m_imageTo;
    QTimer  m_animTimer;
    int     m_animationTick = 0;
};
