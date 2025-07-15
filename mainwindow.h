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
        qreal progress = 0.0; // 0..1 progress of the flip
        qreal angle    = 0.0; // cached angle for painting
        bool  flipped  = false;

        // Parameters for post-flip oscillation
        qreal swingAmplitude = 0.0; // in degrees
        qreal swingPhase     = 0.0;
    };


    QVector<PanelData> m_panels;

    int m_rows = 6;
    int m_cols = 10;

    QPixmap m_imageFrom, m_imageTo;
    QTimer  m_animTimer;
    int     m_animationTick = 0;
};
