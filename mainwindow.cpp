#include "mainwindow.h"
#include <QPainter>
#include <QtMath>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(800, 600);

    m_panels.resize(m_rows * m_cols);

    m_imageFrom.load("test.jpg");

    preapreDst();

    QTimer::singleShot(1000, this, &MainWindow::startFlipAnimation);
    connect(&m_animTimer, &QTimer::timeout, this, &MainWindow::animateStep);
    m_animTimer.setInterval(16); // ~60 FPS
}

void MainWindow::startFlipAnimation()
{
    //m_imageFrom     = this->grab();
    m_animationTick = 0;

    for (auto& panel : m_panels) {
        panel.progress = 0.0;
        panel.angle    = 0.0;
        panel.flipped  = false;
    }

    m_animTimer.start();
}

void MainWindow::animateStep()
{
    bool      allDone       = true;
    const int delayPerPanel = 4;
    const int totalSteps    = 40; // controls animation speed
    const qreal step        = 1.0 / totalSteps;

    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            int   index = row * m_cols + col;
            auto& panel = m_panels[index];

            if (m_animationTick < (row + col) * delayPerPanel) {
                continue;
            }

            if (panel.progress < 1.0) {
                panel.progress = qMin(1.0, panel.progress + step);
                qreal t    = panel.progress;
                qreal ease = 0.5 * (1 - qCos(M_PI * t)); // ease-in-out
                panel.angle = ease * 180.0;
                if (!panel.flipped && panel.angle >= 90) {
                    panel.flipped = true;
                }
                allDone = false;
            }
        }
    }

    m_animationTick++;
    update();

    if (allDone) {
        m_animTimer.stop();
    }
}

void MainWindow::preapreDst()
{
    // === Генерация тестового изображения ===
    int imgW = 800;
    int imgH = 600;

    m_imageTo = QPixmap(imgW, imgH);
    m_imageTo.fill(Qt::white);

    QPainter painter(&m_imageTo);
    QFont    font = painter.font();
    font.setPointSize(12);
    painter.setFont(font);

    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            QRect cell(col * imgW / m_cols, row * imgH / m_rows,
                       imgW / m_cols, imgH / m_rows);

            QColor color = ((row + col) % 2 == 0) ? Qt::lightGray : Qt::darkGray;
            painter.fillRect(cell, color);

            painter.setPen(Qt::black);
            painter.drawRect(cell);
            painter.drawText(cell, Qt::AlignCenter,
                             QString("%1,%2").arg(row).arg(col));
        }
    }
}

void MainWindow::paintEvent(QPaintEvent *)
{
    if (m_imageFrom.isNull() || m_imageTo.isNull()) {
        return;
    }

    QPainter p(this);
    p.setRenderHint(QPainter::SmoothPixmapTransform);

    p.drawPixmap(rect(), m_imageFrom);

    const qreal D       = 100.0;        // условное расстояние до «камеры»
    int         windowW = width();
    int         windowH = height();
    double      panelW  = windowW / m_cols;
    double      panelH  = windowH / m_rows;
    int         imgPW   = m_imageFrom.width() / m_cols;
    int         imgPH   = m_imageFrom.height() / m_rows;

    auto project = [&](qreal x, qreal y, qreal angleDeg, QPointF origin, bool top) -> QPointF {
        if (top) {
            return QPointF(origin.x() + x, origin.y() + y);
        }

        bool  inward   = angleDeg <= 90.0;
        qreal normAng  = inward ? angleDeg : 180.0 - angleDeg; // 0..90
        qreal sign     = inward ? 1.0 : -1.0;

        qreal A    = qDegreesToRadians(normAng);
        qreal cosA = qCos(A);
        qreal sinA = qSin(A);

        qreal z      = y * sinA * sign;
        qreal depthF = D / (D + z);

        qreal dx      = origin.x() + panelW * 0.5 - windowW * 0.5;
        qreal dy      = origin.y() + panelH * 0.5 - windowH * 0.5;
        qreal xyDist  = std::hypot(dx, dy);
        qreal s       = qAbs(qSin(qDegreesToRadians(normAng)));
        qreal xyF     = D / (D + xyDist / 10.0 * s); // более мягкое влияние

        qreal factor = depthF * xyF;

        qreal px = x * factor;
        qreal py = y * cosA * xyF;

        return QPointF(origin.x() + px, origin.y() - sign * py);
    };

    for (int row = 0; row < m_rows; ++row) {
        for (int col = 0; col < m_cols; ++col) {
            int   idx   = row * m_cols + col;
            auto& panel = m_panels[idx];

            if (panel.angle < 90) {
                continue;
            }

            // выбираем текстуру
            QRect   src(col * imgPW, row * imgPH, imgPW, imgPH);
            QPixmap pix = m_imageTo.copy(src);

            // экранная область панели
            QRect dst(col * panelW, row * panelH, panelW, panelH);
            // точка-ориентир для проекции (верхний‑левый угол dst)
            QPointF origin(dst.left(), dst.top());

            // четыре угла панели в локальных coords
            QVector<QPointF> srcQuad = {
                {      0,      0 },
                { panelW,      0 },
                { panelW, panelH },
                {      0, panelH }
            };

            // проецируем их
            QVector<QPointF> dstQuad;
            dstQuad.reserve(4);

            int i = 0;
            for (auto& pt : srcQuad) {
                dstQuad.push_back(
                    project(pt.x(), pt.y(), panel.angle, origin, i < 2));
                i++;
            }

            // 1) сначала тень: нарисуем ломаный полигон над верхним краем
            //    вычислим среднюю точку верхнего ребра и угол для градиента
            qreal rawSin    = qSin(qDegreesToRadians(panel.angle));
            qreal shadowStr = qAbs(rawSin);
            if (shadowStr > 0.01) {
                // dstQuad — QVector<QPointF> из четырёх точек
                QPointF a = dstQuad[0], b = dstQuad[1];
                QPointF mid  = (a + b) * 0.5;
                QPointF edge = b - a;
                QPointF norm(-edge.y(), edge.x());
                if (norm.y() < 0) {
                    norm = -norm; // тень всегда вниз
                }
                norm /= std::hypot(norm.x(), norm.y());
                qreal len = panelH * 0.3 * shadowStr;

                QLinearGradient grad(mid, mid + norm * len);
                QColor          c(0, 0, 0, int(shadowStr * 180));
                grad.setColorAt(0.0, c);
                grad.setColorAt(1.0, QColor(0, 0, 0, 0));

                // Убедитесь, что dstQuad хранит QPointF, а не QPoint:
                QPolygonF poly;
                poly.reserve(4);
                poly << a << b << (b + norm * len) << (a + norm * len);

                p.save();
                p.setPen(Qt::NoPen);
                p.setBrush(QBrush(grad));
                p.drawPolygon(poly);
                p.restore();
            }

            // 2) рисуем саму панель, маппя квадрат на произвольный четырёхугольник
            QTransform t;
            if (QTransform::quadToQuad(srcQuad, dstQuad, t)) {
                p.setTransform(t, /*combine=*/ false);
                p.drawPixmap(0, 0, panelW, panelH, pix);
                p.resetTransform();
            }
        }
    }
}
