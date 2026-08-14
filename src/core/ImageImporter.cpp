#include "ImageImporter.h"

#include <QProcess>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace vmm {

namespace {
// extension -> {label, needsConversion}
struct Fmt { const char *ext; const char *label; bool convert; };
const Fmt kFormats[] = {
    {"qcow2", "QEMU copy-on-write",        false},
    {"img",   "Raw disk",                  false},
    {"raw",   "Raw disk",                  false},
    {"vmdk",  "VMware disk",               true},
    {"vhdx",  "Hyper-V disk (v2)",         true},
    {"vhd",   "Hyper-V / Virtual PC disk", true},
    {"vpc",   "Virtual PC disk",           true},
    {"vdi",   "VirtualBox disk",           true},
    {"qed",   "QEMU enhanced disk",        true},
    {"ova",   "OVF appliance (archive)",   true},
    {"ovf",   "OVF appliance (descriptor)",true},
};
} // namespace

ImageImporter::ImageImporter(QObject *parent) : QObject(parent) {}

ImageImporter::~ImageImporter() {
    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
}

QVariantList ImageImporter::supportedFormats() {
    QVariantList out;
    for (const auto &f : kFormats)
        out.push_back(QVariantMap{
            {"extension", QString::fromLatin1(f.ext)},
            {"label", QString::fromLatin1(f.label)},
            {"needsConversion", f.convert}});
    return out;
}

QString ImageImporter::detectFormat(const QString &path) {
    const QString ext = QFileInfo(path).suffix().toLower();
    for (const auto &f : kFormats)
        if (ext == QLatin1String(f.ext))
            return ext;
    return {};
}

QString ImageImporter::resolveQemuImg() {
    const QString exe =
#if defined(Q_OS_WIN)
        QStringLiteral("qemu-img.exe");
#else
        QStringLiteral("qemu-img");
#endif
    // 1. Bundled next to the app binary (self-contained installer).
    const QString bundled = QDir(QCoreApplication::applicationDirPath()).filePath(exe);
    if (QFileInfo::exists(bundled))
        return bundled;
    // 2. Explicit override.
    const QString override = QProcessEnvironment::systemEnvironment().value("VMM_QEMU_IMG");
    if (!override.isEmpty() && QFileInfo::exists(override))
        return override;
    // 3. PATH.
    return QStandardPaths::findExecutable(exe);
}

void ImageImporter::setBusy(bool b)               { if (m_busy != b) { m_busy = b; emit busyChanged(); } }
void ImageImporter::setProgress(double p)         { m_progress = p; emit progressChanged(); }
void ImageImporter::setStatus(const QString &s)   { m_statusText = s; emit statusTextChanged(); }

void ImageImporter::finishWith(const QString &path, const QString &error, DoneFn done) {
    setBusy(false);
    setProgress(error.isEmpty() ? 1.0 : -1.0);
    setStatus(error.isEmpty() ? tr("Done") : error);
    if (done)
        done(path, error);
    emit finished(error.isEmpty(), path, error);
}

void ImageImporter::startConvert(const QString &sourcePath, const QString &targetFormat) {
    convert(sourcePath, targetFormat.isEmpty() ? QStringLiteral("qcow2") : targetFormat, {});
}

void ImageImporter::cancel() {
    if (m_proc && m_proc->state() != QProcess::NotRunning)
        m_proc->kill();
}

void ImageImporter::convert(const QString &sourcePath, const QString &targetFormat, DoneFn done) {
    if (m_busy) {
        finishWith({}, tr("An import is already in progress"), done);
        return;
    }

    const QFileInfo src(sourcePath);
    if (!src.exists()) {
        finishWith({}, tr("Source file not found: %1").arg(sourcePath), done);
        return;
    }

    const QString ext = detectFormat(sourcePath);
    setBusy(true);
    setProgress(-1.0);

    // Already native — register in place, no conversion.
    if (ext == QLatin1String("qcow2") || ext == QLatin1String("raw") || ext == QLatin1String("img")) {
        setStatus(tr("Registering %1").arg(src.fileName()));
        finishWith(sourcePath, {}, done);
        return;
    }

    // OVA/OVF: unpack + parse the descriptor, then convert the primary disk.
    // TODO(phase-3): full multi-disk OVF (parse <Disk>/<File>/<VirtualHardware>
    // for CPU/RAM/NICs). For now we surface a clear message.
    if (ext == QLatin1String("ova") || ext == QLatin1String("ovf")) {
        finishWith({}, tr("OVA/OVF import lands in phase 3; single-disk vmdk/vhdx/vdi "
                          "conversion works today. Extract the appliance and import its disk."),
                   done);
        return;
    }

    if (ext.isEmpty()) {
        finishWith({}, tr("Unsupported or unrecognised image format: %1").arg(src.fileName()), done);
        return;
    }

    const QString target = src.dir().filePath(src.completeBaseName() + "." + targetFormat);
    runQemuConvert(sourcePath, target, targetFormat, std::move(done));
}

void ImageImporter::runQemuConvert(const QString &source, const QString &target,
                                   const QString &targetFormat, DoneFn done) {
    const QString qemuImg = resolveQemuImg();
    if (qemuImg.isEmpty()) {
        finishWith({}, tr("qemu-img not found. Install qemu (Homebrew: brew install qemu) "
                          "or bundle it with the app."), done);
        return;
    }

    setStatus(tr("Converting %1 -> %2").arg(QFileInfo(source).fileName(), targetFormat));

    m_proc = new QProcess(this);
    m_proc->setProgram(qemuImg);
    // -p prints a running percentage we parse for the progress bar.
    m_proc->setArguments({"convert", "-p", "-O", targetFormat, source, target});

    connect(m_proc, &QProcess::readyReadStandardOutput, this, [this] {
        const QString out = QString::fromLocal8Bit(m_proc->readAllStandardOutput());
        static const QRegularExpression re(QStringLiteral("\\(?\\s*([0-9]+(?:\\.[0-9]+)?)\\s*%"));
        auto it = re.globalMatch(out);
        double last = -1.0;
        while (it.hasNext())
            last = it.next().captured(1).toDouble();
        if (last >= 0.0)
            setProgress(last / 100.0);
    });

    connect(m_proc, &QProcess::finished, this,
            [this, target, done](int code, QProcess::ExitStatus status) {
        if (!m_proc) return;
        const QString err = QString::fromLocal8Bit(m_proc->readAllStandardError());
        m_proc->deleteLater();
        m_proc = nullptr;
        if (status == QProcess::CrashExit)
            finishWith({}, tr("qemu-img was terminated"), done);
        else if (code != 0)
            finishWith({}, err.isEmpty() ? tr("qemu-img failed (exit %1)").arg(code) : err.trimmed(), done);
        else
            finishWith(target, {}, done);
    });

    connect(m_proc, &QProcess::errorOccurred, this, [this, done](QProcess::ProcessError) {
        if (!m_proc) return;
        const QString e = m_proc->errorString();
        m_proc->deleteLater();
        m_proc = nullptr;
        finishWith({}, e, done);
    });

    m_proc->start();
}

} // namespace vmm
