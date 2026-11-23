// Copyright (c) 2022-2023 Unfolded Circle ApS and/or its affiliates. <hello@unfoldedcircle.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "soundEffects.h"

#include <QDir>

#include "../logging.h"

namespace uc {
namespace ui {
namespace {

const char *soundEffectStatusName(QSoundEffect::Status status) {
    switch (status) {
        case QSoundEffect::Null:
            return "Null";
        case QSoundEffect::Loading:
            return "Loading";
        case QSoundEffect::Ready:
            return "Ready";
        case QSoundEffect::Error:
            return "Error";
    }

    return "Unknown";
}

}  // namespace

SoundEffects *SoundEffects::s_instance = nullptr;

SoundEffects::SoundEffects(int volume, bool enabled, const QString &effectsDir, hw::HardwareModel::Enum model,
                           QObject *parent)
    : QObject(parent), m_effectsDir(effectsDir), m_model(model), m_volume(volume), m_enabled(enabled) {
    Q_ASSERT(s_instance == nullptr);
    s_instance = this;
}

SoundEffects::~SoundEffects() {
    s_instance = nullptr;
}

void SoundEffects::initialize() {
    qCInfo(lcUi()) << "[SoundEffects] Initializing sound effects"
                   << "- enabled:" << m_enabled
                   << "- volume:" << m_volume;
    qCInfo(lcUi()) << "[SoundEffects] Effects directory:" << m_effectsDir;

    if (m_effectsDir.isEmpty()) {
        qCWarning(lcUi())
            << "[SoundEffects] UC_SOUND_EFFECTS_PATH is empty: sound effects are disabled";
        return;
    }

    const QDir effectsDir(m_effectsDir);

    if (!effectsDir.exists()) {
        qCWarning(lcUi())
            << "[SoundEffects] Sound effects directory does not exist:"
            << m_effectsDir;
        return;
    }

    qCInfo(lcUi())
        << "[SoundEffects] Sound effects directory exists:"
        << effectsDir.absolutePath();

    const QStringList soundFiles = {
        QStringLiteral("click.wav"),
        QStringLiteral("click_lo.wav"),
        QStringLiteral("confirm.wav"),
        QStringLiteral("error.wav"),
        QStringLiteral("zap_future.wav")
    };

    for (const QString &fileName : soundFiles) {
        const QFileInfo fileInfo(effectsDir.filePath(fileName));

        if (!fileInfo.exists()) {
            qCWarning(lcUi())
                << "[SoundEffects] Missing sound file:" << fileInfo.absoluteFilePath();
            continue;
        }

        if (!fileInfo.isFile()) {
            qCWarning(lcUi())
                << "[SoundEffects] Not a regular file:" << fileInfo.absoluteFilePath();
            continue;
        }

        qCInfo(lcUi())
            << "[SoundEffects] Sound file found:"
            << fileInfo.absoluteFilePath()
            << "- size:" << fileInfo.size()
            << "bytes"
            << "- readable:" << fileInfo.isReadable();
    }

    const QAudioDeviceInfo deviceInfo =
        QAudioDeviceInfo::defaultOutputDevice();

    if (deviceInfo.isNull()) {
        qCWarning(lcUi())
            << "[SoundEffects] No default audio output device available";
    } else {
        qCInfo(lcUi())
            << "[SoundEffects] Default audio output device:"
            << deviceInfo.deviceName();
    }

    qCInfo(lcUi())
        << "[SoundEffects] Calling createEffects()";

    createEffects(deviceInfo);

    qCInfo(lcUi())
        << "[SoundEffects] createEffects() completed";
}

void SoundEffects::setVolume(int volume) {
    if (m_volume != volume) {
        m_volume = volume;
        qCDebug(lcUi()) << "Sound effects volume changed to:" << m_volume;
        emit volumeChanged();
    }
}

void SoundEffects::setEnabled(bool value) {
    if (m_enabled != value) {
        m_enabled = value;
        qCInfo(lcUi()) << "[SoundEffects] Enabled state changed to:" << m_enabled;
        emit enabledChanged();
    }
}

void SoundEffects::play(SoundEffects::SoundEffect effect) {
    QSoundEffect *sound = nullptr;
    const char *effectName = "Unknown";

    switch (effect) {
        case Click:
            sound = m_effectClick;
            effectName = "click";
            break;
        case ClickLow:
            sound = m_effectClickLow;
            effectName = "click_lo";
            break;
        case Confirm:
            sound = m_effectConfirm;
            effectName = "confirm";
            break;
        case Error:
            sound = m_effectError;
            effectName = "error";
            break;
        case BatteryCharge:
            sound = m_effectBatteryCharge;
            effectName = "zap_future";
            break;
    }

    qCInfo(lcUi())
        << "[SoundEffects] play() requested:"
        << effectName
        << "- enabled:" << m_enabled
        << "- available:" << isAvailable()
        << "- status:" << (sound ? soundEffectStatusName(sound->status()) : "not created")
        << "- volume:" << m_volume;

    if (!m_enabled || !isAvailable() || !sound) {
        qCInfo(lcUi())
            << "[SoundEffects] Playback skipped for" << effectName;
        return;
    }

    sound->setVolume(qreal(m_volume) / 100);
    sound->play();

    qCInfo(lcUi())
        << "[SoundEffects] play() invoked for" << effectName;
}

QObject *SoundEffects::qmlInstance(QQmlEngine *engine, QJSEngine *scriptEngine) {
    Q_UNUSED(scriptEngine);

    QObject *obj = s_instance;
    engine->setObjectOwnership(obj, QQmlEngine::CppOwnership);

    return obj;
}

void SoundEffects::createEffects(const QAudioDeviceInfo &deviceInfo) {
    const auto monitorEffect = [this](QSoundEffect *effect, const QString &name) {
        connect(effect, &QSoundEffect::statusChanged, this, [effect, name]() {
            qCInfo(lcUi())
                << "[SoundEffects]" << name
                << "status changed to:" << soundEffectStatusName(effect->status());
        });

        connect(effect, &QSoundEffect::playingChanged, this, [effect, name]() {
            qCInfo(lcUi())
                << "[SoundEffects]" << name
                << "playing changed to:" << effect->isPlaying();
        });
    };

    m_effectClick = new QSoundEffect(deviceInfo, this);
    monitorEffect(m_effectClick, QStringLiteral("click"));
    m_effectClick->setSource(QUrl::fromLocalFile(m_effectsDir + QStringLiteral("/click.wav")));

    m_effectClickLow = new QSoundEffect(deviceInfo, this);
    monitorEffect(m_effectClickLow, QStringLiteral("click_lo"));
    m_effectClickLow->setSource(QUrl::fromLocalFile(m_effectsDir + QStringLiteral("/click_lo.wav")));

    m_effectConfirm = new QSoundEffect(deviceInfo, this);
    monitorEffect(m_effectConfirm, QStringLiteral("confirm"));
    m_effectConfirm->setSource(QUrl::fromLocalFile(m_effectsDir + QStringLiteral("/confirm.wav")));

    m_effectError = new QSoundEffect(deviceInfo, this);
    monitorEffect(m_effectError, QStringLiteral("error"));
    m_effectError->setSource(QUrl::fromLocalFile(m_effectsDir + QStringLiteral("/error.wav")));

    m_effectBatteryCharge = new QSoundEffect(deviceInfo, this);
    monitorEffect(m_effectBatteryCharge, QStringLiteral("zap_future"));
    m_effectBatteryCharge->setSource(QUrl::fromLocalFile(m_effectsDir + QStringLiteral("/zap_future.wav")));
}

}  // namespace ui
}  // namespace uc
