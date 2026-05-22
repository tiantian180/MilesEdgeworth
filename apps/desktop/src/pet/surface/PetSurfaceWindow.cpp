#include "pet/surface/PetSurfaceWindow.h"

#include "DesktopShellController.h"
#include "chat/ChatController.h"
#include "pet/PetRuntime.h"
#include "pet/events/PetEventBridge.h"
#include "pet/surface/PetContextMenu.h"
#include "pet/surface/PropSurfaceWindow.h"

#ifdef Q_OS_MACOS
#include "platform/MacPetWindowBehavior.h"
#endif

#include <QContextMenuEvent>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QMovie>
#include <QPropertyAnimation>
#include <QResizeEvent>
#include <QRegion>
#include <QSoundEffect>
#include <QUrl>
#include <QVariantMap>
#include <QtGlobal>

namespace {
constexpr int kAlphaThreshold = 8;
constexpr int kDoubleClickIntervalMs = 300;
constexpr int kDragThresholdPx = 3;
} // namespace

PetSurfaceWindow::PetSurfaceWindow(
    PetRuntime *runtime,
    PetEventBridge *eventBridge,
    DesktopShellController *shellController,
    ChatController *chatController,
    QWidget *parent
)
    : QWidget(parent)
    , m_runtime(runtime)
    , m_eventBridge(eventBridge)
    , m_shellController(shellController)
    , m_chatController(chatController)
    , m_petLabel(new QLabel(this))
    , m_movie(new QMovie(this))
    , m_soundEffect(new QSoundEffect(this))
    , m_propWindow(new PropSurfaceWindow())
{
    Q_ASSERT(m_runtime != nullptr);
    Q_ASSERT(m_eventBridge != nullptr);
    Q_ASSERT(m_shellController != nullptr);

    setWindowTitle(QStringLiteral("MilesEdgeworth v2"));
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setWindowFlags(Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Tool);
    setAutoFillBackground(false);
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);

    m_petLabel->setAttribute(Qt::WA_TranslucentBackground, true);
    m_petLabel->setAttribute(Qt::WA_NoSystemBackground, true);
    m_petLabel->setAutoFillBackground(false);
    m_petLabel->setAlignment(Qt::AlignCenter);
    m_petLabel->setScaledContents(true);
    m_petLabel->setMovie(m_movie);

    m_movie->setCacheMode(QMovie::CacheAll);
    m_soundEffect->setVolume(m_runtime->audioMuted() ? 0.0f : 0.8f);

    m_singleClickTimer.setInterval(kDoubleClickIntervalMs);
    m_singleClickTimer.setSingleShot(true);
    connect(&m_singleClickTimer, &QTimer::timeout, this, [this]() {
        m_eventBridge->submitPrimaryClick(
            m_pendingSingleClickPosition.x(),
            m_pendingSingleClickPosition.y(),
            width(),
            height()
        );
    });

    m_propExpireTimer.setSingleShot(true);
    connect(&m_propExpireTimer, &QTimer::timeout, this, [this]() {
        m_eventBridge->submitPropExpired();
    });

    m_propWindow->setClickedCallback([this]() {
        m_propExpireTimer.stop();
        hidePropWindow();
        m_eventBridge->submitPropClicked();
    });
    m_propWindow->setAlwaysOnTop(m_shellController->alwaysOnTop());

    connect(m_movie, &QMovie::frameChanged, this, &PetSurfaceWindow::handleMovieFrameChanged);
    connect(m_runtime, &PetRuntime::petScaleChanged, this, &PetSurfaceWindow::syncSizeFromRuntime);
    connect(m_runtime, &PetRuntime::playbackSerialChanged, this, &PetSurfaceWindow::restartMovieFromRuntime);
    connect(m_runtime, &PetRuntime::soundPlaybackSerialChanged, this, &PetSurfaceWindow::playSoundFromRuntime);
    connect(m_runtime, &PetRuntime::audioMutedChanged, this, [this]() {
        m_soundEffect->setVolume(m_runtime->audioMuted() ? 0.0f : 0.8f);
    });
    connect(m_runtime, &PetRuntime::currentPropPlaybackSerialChanged, this, &PetSurfaceWindow::showPropFromRuntime);
    connect(m_runtime, &PetRuntime::currentPropChanged, this, [this]() {
        if (!m_runtime->currentPropVisible()) {
            hidePropWindow();
        }
    });
    connect(m_shellController, &DesktopShellController::alwaysOnTopChanged, this, [this]() {
        m_propWindow->setAlwaysOnTop(m_shellController->alwaysOnTop());
    });

    syncSizeFromRuntime();
    restartMovieFromRuntime();
}

PetSurfaceWindow::~PetSurfaceWindow()
{
    delete m_propWindow;
}

void PetSurfaceWindow::contextMenuEvent(QContextMenuEvent *event)
{
    if (!m_runtime->pointerInteractionEnabled()) {
        event->ignore();
        return;
    }

    event->accept();
    showContextMenuQueued(event->globalPos());
}

void PetSurfaceWindow::mousePressEvent(QMouseEvent *event)
{
    if (!m_runtime->pointerInteractionEnabled()) {
        event->ignore();
        return;
    }

    if (event->button() == Qt::RightButton) {
        event->accept();
        showContextMenuQueued(event->globalPosition().toPoint());
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (m_singleClickTimer.isActive()) {
        m_singleClickTimer.stop();
        m_doubleClickPending = true;
    } else {
        m_doubleClickPending = false;
    }

    m_pressPosition = event->position().toPoint();
    m_dragMoved = false;
    m_eventBridge->submitDragStarted(event->globalPosition().x());
}

void PetSurfaceWindow::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_runtime->pointerInteractionEnabled()) {
        event->ignore();
        return;
    }

    if ((event->buttons() & Qt::LeftButton) == 0) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const QPoint currentPosition = event->position().toPoint();
    const QPoint delta = currentPosition - m_pressPosition;
    if (qAbs(delta.x()) > kDragThresholdPx || qAbs(delta.y()) > kDragThresholdPx) {
        m_dragMoved = true;
    }

    m_eventBridge->submitDragMoved(event->globalPosition().x());
    m_shellController->movePetWindowBy(delta.x(), delta.y());
}

void PetSurfaceWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_runtime->pointerInteractionEnabled()) {
        event->ignore();
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    m_eventBridge->submitDragEnded();

    if (m_dragMoved) {
        m_doubleClickPending = false;
        return;
    }

    if (m_doubleClickPending) {
        m_doubleClickPending = false;
        m_eventBridge->submitDoubleClick();
        return;
    }

    m_pendingSingleClickPosition = event->position().toPoint();
    m_singleClickTimer.start();
}

void PetSurfaceWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int imageSize = qRound(m_runtime->petImageSize());
    m_petLabel->setGeometry(
        (width() - imageSize) / 2,
        (height() - imageSize) / 2,
        imageSize,
        imageSize
    );
    applyCurrentFrameMask();
}

QString PetSurfaceWindow::imagePathFromUrl(const QUrl &url) const
{
    if (url.scheme() == QStringLiteral("qrc")) {
        return QStringLiteral(":") + url.path();
    }
    if (url.isLocalFile()) {
        return url.toLocalFile();
    }
    return url.toString();
}

void PetSurfaceWindow::syncSizeFromRuntime()
{
    const int windowSize = qRound(m_runtime->petWindowSize());
    setFixedSize(windowSize, windowSize);

    const int imageSize = qRound(m_runtime->petImageSize());
    m_petLabel->setGeometry(
        (windowSize - imageSize) / 2,
        (windowSize - imageSize) / 2,
        imageSize,
        imageSize
    );

    applyCurrentFrameMask();
}

void PetSurfaceWindow::restartMovieFromRuntime()
{
    const QString path = imagePathFromUrl(m_runtime->currentAnimationUrl());
    if (path.isEmpty()) {
        clearMask();
        return;
    }

    m_movie->stop();
    m_movie->setFileName(path);
    // 不要先 jumpToFrame(0) 再 start()：Qt 会同步发出 frame 0，
    // 随后 start() 立刻进入 frame 1，walk/run 的首帧就没有正常显示时长。
    // 直接 start() 可让 QMovie 以正常节奏从首帧开始。
    m_movie->start();
    applyCurrentFrameMask();
}

void PetSurfaceWindow::handleMovieFrameChanged(int frame)
{
    applyCurrentFrameMask();

    const QVariantMap movementDelta = m_runtime->consumeFrameMovementDelta();
    const double dx = movementDelta.value(QStringLiteral("dx")).toDouble();
    const double dy = movementDelta.value(QStringLiteral("dy")).toDouble();
    if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) {
        m_shellController->movePetWindowBy(dx, dy);
    }

    const int frameCount = m_movie->frameCount();
    if (frameCount <= 0 || frame < frameCount - 1) {
        return;
    }

    const int playbackSerial = m_runtime->playbackSerial();
    const int currentFrameDelayMs = qMax(1, m_movie->nextFrameDelay());

    if (m_runtime->currentLoopMode() == QStringLiteral("hold")) {
        m_eventBridge->submitHoldAnimationReachedEnd();
        m_movie->setPaused(true);
        return;
    }

    if (m_runtime->currentAutoReturnToIdle()
            || m_runtime->currentLoopMode() == QStringLiteral("once")) {
        // frameChanged 触发时 QLabel 还没有完成当前帧绘制。
        // 先暂停在最后一帧，再等这一帧的显示时长结束后切换动作，
        // 避免走路 / 跑步这类一次性 GIF 的最后一帧被立即覆盖。
        m_movie->setPaused(true);
        scheduleAnimationCompletion(playbackSerial, currentFrameDelayMs);
        return;
    }

    if (m_runtime->currentActionAcceptsIdleLoopFinished()) {
        scheduleIdleLoopFinished(playbackSerial, currentFrameDelayMs);
    }
}

void PetSurfaceWindow::scheduleAnimationCompletion(int playbackSerial, int delayMs)
{
    QTimer::singleShot(qMax(1, delayMs), this, [this, playbackSerial]() {
        completeAnimationIfStillCurrent(playbackSerial);
    });
}

void PetSurfaceWindow::completeAnimationIfStillCurrent(int playbackSerial)
{
    if (m_runtime->playbackSerial() != playbackSerial) {
        return;
    }

    m_runtime->handleAnimationFinished();
}

void PetSurfaceWindow::scheduleIdleLoopFinished(int playbackSerial, int delayMs)
{
    QTimer::singleShot(qMax(1, delayMs), this, [this, playbackSerial]() {
        submitIdleLoopFinishedIfStillCurrent(playbackSerial);
    });
}

void PetSurfaceWindow::submitIdleLoopFinishedIfStillCurrent(int playbackSerial)
{
    if (m_runtime->playbackSerial() != playbackSerial
            || !m_runtime->currentActionAcceptsIdleLoopFinished()) {
        return;
    }

    m_eventBridge->submitIdleLoopFinished();
}

void PetSurfaceWindow::applyCurrentFrameMask()
{
    const QRegion region = regionFromCurrentFrame();
    if (region.isEmpty()) {
        clearMask();
        return;
    }

    setMask(region);
}

QRegion PetSurfaceWindow::regionFromCurrentFrame() const
{
    const QPixmap currentFrame = m_movie->currentPixmap();
    if (currentFrame.isNull() || m_petLabel == nullptr) {
        return {};
    }

    const QImage image = currentFrame
        .toImage()
        .scaled(m_petLabel->size(), Qt::IgnoreAspectRatio, Qt::FastTransformation)
        .convertToFormat(QImage::Format_ARGB32);
    QRegion region;

    // 按扫描线合并连续不透明像素。这样比逐像素加入 QRegion 少很多碎片，
    // 同时仍然能保持像素风 GIF 的硬边缘命中效果。
    for (int y = 0; y < image.height(); ++y) {
        int runStart = -1;
        for (int x = 0; x < image.width(); ++x) {
            const bool opaque = qAlpha(image.pixel(x, y)) > kAlphaThreshold;
            if (opaque && runStart < 0) {
                runStart = x;
            }

            const bool atLastPixel = (x == image.width() - 1);
            if ((!opaque || atLastPixel) && runStart >= 0) {
                const int runEnd = (opaque && atLastPixel) ? x + 1 : x;
                region += QRect(m_petLabel->x() + runStart, m_petLabel->y() + y, runEnd - runStart, 1);
                runStart = -1;
            }
        }
    }

    return region;
}

void PetSurfaceWindow::showContextMenuAt(const QPoint &globalPosition)
{
#ifdef Q_OS_MACOS
    prepareMacPetWindowForContextMenu(windowHandle());
#endif
    PetContextMenu::show(this, m_runtime, m_eventBridge, m_shellController, m_chatController, globalPosition);
}

void PetSurfaceWindow::showContextMenuQueued(const QPoint &globalPosition)
{
    m_pendingContextMenuPosition = globalPosition;
    if (m_contextMenuPending) {
        return;
    }

    m_contextMenuPending = true;
    QTimer::singleShot(0, this, [this]() {
        m_contextMenuPending = false;
        if (!m_runtime->pointerInteractionEnabled()) {
            return;
        }
        showContextMenuAt(m_pendingContextMenuPosition);
    });
}

void PetSurfaceWindow::playSoundFromRuntime()
{
    if (m_runtime->currentSoundUrl().isEmpty() || m_runtime->audioMuted()) {
        return;
    }

    m_soundEffect->stop();
    m_soundEffect->setSource(m_runtime->currentSoundUrl());
    m_soundEffect->play();
}

void PetSurfaceWindow::showPropFromRuntime()
{
    if (!m_runtime->currentPropVisible()) {
        return;
    }

    const QPoint startPosition(
        x() + qRound(m_runtime->currentPropStartX()),
        y() + qRound(m_runtime->currentPropStartY())
    );
    const QPoint endPosition(
        x() + qRound(m_runtime->currentPropEndX()),
        y() + qRound(m_runtime->currentPropEndY())
    );

    m_propWindow->showPixmap(
        imagePathFromUrl(m_runtime->currentPropImageUrl()),
        QSize(qMax(1, qRound(m_runtime->currentPropWidth())), qMax(1, qRound(m_runtime->currentPropHeight()))),
        QSize(qMax(1, qRound(m_runtime->currentPropVisualWidth())), qMax(1, qRound(m_runtime->currentPropVisualHeight()))),
        startPosition
    );

    auto *animation = new QPropertyAnimation(m_propWindow, "pos", m_propWindow);
    animation->setStartValue(startPosition);
    animation->setEndValue(endPosition);
    animation->setDuration(qMax(1, m_runtime->currentPropDurationMs()));
    animation->setEasingCurve(QEasingCurve::OutSine);
    animation->start(QAbstractAnimation::DeleteWhenStopped);

    m_propExpireTimer.start(qMax(1, m_runtime->currentPropDurationMs()));
}

void PetSurfaceWindow::hidePropWindow()
{
    m_propExpireTimer.stop();
    if (m_propWindow != nullptr) {
        m_propWindow->hide();
    }
}
