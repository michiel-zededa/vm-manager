#pragma once

#include "Types.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class QProcess;

namespace vmm {

// Converts foreign disk images to the target libvirt-native format (qcow2 by
// default) using qemu-img, and unpacks OVA/OVF appliances. Runs the external
// process event-driven so the GUI thread never blocks, and reports progress.
//
// Self-contained deployment: qemu-img is looked up next to the application
// binary first (bundled in the installer), then $VMM_QEMU_IMG, then PATH — so a
// packaged build does not depend on a system qemu-img.
class ImageImporter : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged) // 0..1, -1 = indeterminate
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(bool available READ available CONSTANT) // is a qemu-img present?
public:
    explicit ImageImporter(QObject *parent = nullptr);
    ~ImageImporter() override;

    bool busy() const { return m_busy; }
    double progress() const { return m_progress; }
    QString statusText() const { return m_statusText; }
    bool available() const { return !resolveQemuImg().isEmpty(); }

    // Formats we can accept as input (for the file picker + drag-drop hints).
    Q_INVOKABLE static QVariantList supportedFormats();
    // Best-effort detection of a format from a file path/extension.
    Q_INVOKABLE static QString detectFormat(const QString &path);

    // Completion callback: (preparedDiskPath, errorString). error empty == ok.
    using DoneFn = std::function<void(const QString &preparedPath, const QString &error)>;

    // Convert `sourcePath` to `targetFormat` (e.g. "qcow2"). If the source is
    // already in the target format, it is registered in place. OVA/OVF inputs
    // are unpacked first. Emits progress; calls `done` on the GUI thread.
    void convert(const QString &sourcePath, const QString &targetFormat, DoneFn done);

    // QML-friendly entry point; result delivered via finished().
    Q_INVOKABLE void startConvert(const QString &sourcePath, const QString &targetFormat);
    Q_INVOKABLE void cancel();

signals:
    void busyChanged();
    void progressChanged();
    void statusTextChanged();
    void finished(bool ok, const QString &preparedPath, const QString &error);

private:
    static QString resolveQemuImg();
    void setBusy(bool b);
    void setProgress(double p);
    void setStatus(const QString &s);
    void runQemuConvert(const QString &source, const QString &target, const QString &targetFormat, DoneFn done);
    void finishWith(const QString &path, const QString &error, DoneFn done);

    QProcess *m_proc = nullptr;
    bool m_busy = false;
    double m_progress = -1.0;
    QString m_statusText;
};

} // namespace vmm
