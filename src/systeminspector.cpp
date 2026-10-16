#include "systeminspector.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSslCertificate>
#include <QSysInfo>

namespace
{
struct ConfigValue
{
    bool readable = false;
    bool enabled = false;
};

QString readTextFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    return QString::fromLocal8Bit(file.readAll()).trimmed();
}

QStringList kernelCommandLine()
{
    return readTextFile(QStringLiteral("/proc/cmdline"))
        .split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
}

ConfigValue kernelConfigValue(const QString &option)
{
    const QString release = QSysInfo::kernelVersion();
    const QStringList paths = {
        QStringLiteral("/boot/config-%1").arg(release),
        QStringLiteral("/lib/modules/%1/config").arg(release),
        QStringLiteral("/usr/lib/modules/%1/config").arg(release),
    };
    const QRegularExpression enabledPattern(
        QStringLiteral("^%1=y$").arg(QRegularExpression::escape(option)));
    const QRegularExpression disabledPattern(
        QStringLiteral("^# %1 is not set$").arg(QRegularExpression::escape(option)));

    for (const QString &path : paths) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            continue;
        }

        while (!file.atEnd()) {
            const QString line = QString::fromLocal8Bit(file.readLine()).trimmed();
            if (enabledPattern.match(line).hasMatch()) {
                return {true, true};
            }
            if (disabledPattern.match(line).hasMatch()) {
                return {true, false};
            }
        }
    }
    return {};
}

quint32 readLittleEndian32(const QByteArray &data, qsizetype offset)
{
    const auto byte = [&data](qsizetype index) {
        return static_cast<quint32>(static_cast<unsigned char>(data.at(index)));
    };
    return byte(offset) | (byte(offset + 1) << 8) | (byte(offset + 2) << 16)
        | (byte(offset + 3) << 24);
}

QString supplierForCertificate(const QSslCertificate &certificate)
{
    QStringList subjectParts;
    for (const QSslCertificate::SubjectInfo field : {
             QSslCertificate::CommonName,
             QSslCertificate::Organization,
             QSslCertificate::OrganizationalUnitName,
         }) {
        subjectParts.append(certificate.subjectInfo(field));
    }
    const QString subject = subjectParts.join(QLatin1Char(' ')).simplified();
    const QString normalized = subject.toCaseFolded();

    const QVector<QPair<QString, QStringList>> suppliers = {
        {QStringLiteral("Canonical Ltd."),
         {QStringLiteral("canonical ltd. master certificate authority")}},
        {QStringLiteral("Fedora"),
         {QStringLiteral("fedora secure boot ca")}},
        {QStringLiteral("Universal Blue"),
         {QStringLiteral("universal blue"), QStringLiteral("ublue")}},
        {QStringLiteral("Debian"),
         {QStringLiteral("debian secure boot ca")}},
        {QStringLiteral("openSUSE"),
         {QStringLiteral("opensuse secure boot ca")}},
        {QStringLiteral("SUSE Linux Enterprise"),
         {QStringLiteral("suse linux enterprise secure boot ca")}},
        {QStringLiteral("Rocky Linux"),
         {QStringLiteral("rocky linux secure boot root ca")}},
        {QStringLiteral("Microsoft"),
         {QStringLiteral("microsoft uefi ca"), QStringLiteral("microsoft corporation uefi ca")}},
        {QStringLiteral("AlmaLinux"),
         {QStringLiteral("almalinux secure boot ca")}},
    };

    for (const auto &[name, aliases] : suppliers) {
        for (const QString &alias : aliases) {
            if (normalized.contains(alias)) {
                return name;
            }
        }
    }
    return {};
}

struct CertificateDatabaseResult
{
    bool readable = false;
    bool parsedDatabase = false;
    int certificateCount = 0;
    QStringList suppliers;
};

CertificateDatabaseResult inspectCertificateDatabase(const QString &variableName)
{
    static const QByteArray x509Guid = QByteArray::fromHex(
        "a159c0a5e494a74a87b5ab155c2bf072");
    const QString directory = QStringLiteral("/sys/firmware/efi/efivars");
    const QStringList paths = QDir(directory).entryList(
        {variableName + QStringLiteral("-*")}, QDir::Files | QDir::Readable, QDir::Name);
    CertificateDatabaseResult result;

    for (const QString &name : paths) {
        QFile file(QDir(directory).filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray contents = file.readAll();
        if (contents.size() < 4) {
            continue;
        }
        result.readable = true;

        qsizetype offset = 4;
        while (offset + 28 <= contents.size()) {
            const quint32 listSize = readLittleEndian32(contents, offset + 16);
            const quint32 headerSize = readLittleEndian32(contents, offset + 20);
            const quint32 signatureSize = readLittleEndian32(contents, offset + 24);
            if (listSize < 28 || listSize > static_cast<quint32>(contents.size() - offset)
                || signatureSize < 16 || 28 + headerSize > listSize) {
                break;
            }
            result.parsedDatabase = true;

            const qsizetype listEnd = offset + listSize;
            qsizetype entryOffset = offset + 28 + headerSize;
            if (entryOffset > listEnd || signatureSize > listEnd - entryOffset) {
                offset = listEnd;
                continue;
            }

            if (contents.mid(offset, 16) == x509Guid) {
                while (entryOffset + signatureSize <= listEnd) {
                    const QByteArray certificateData =
                        contents.mid(entryOffset + 16, signatureSize - 16);
                    const QSslCertificate certificate(certificateData, QSsl::Der);
                    if (!certificate.isNull()) {
                        ++result.certificateCount;
                        const QString supplier = supplierForCertificate(certificate);
                        if (!supplier.isEmpty() && !result.suppliers.contains(supplier)) {
                            result.suppliers.append(supplier);
                        }
                    }
                    entryOffset += signatureSize;
                }
            }
            offset = listEnd;
        }
    }

    result.suppliers.sort(Qt::CaseInsensitive);
    return result;
}

SecurityCheck secureBootSupplierCheck()
{
    if (!QFileInfo(QStringLiteral("/sys/firmware/efi/efivars")).isDir()) {
        return {QStringLiteral("Recognized Secure Boot supplier"),
                QStringLiteral("Enrolled UEFI certificate authority"),
                QStringLiteral("UEFI variables are unavailable; the enrolled Secure Boot database cannot be inspected."),
                CheckStatus::unknown,
                QStringLiteral("UNVERIFIED")};
    }

    const CertificateDatabaseResult database =
        inspectCertificateDatabase(QStringLiteral("db"));
    if (!database.readable || !database.parsedDatabase) {
        return {QStringLiteral("Recognized Secure Boot supplier"),
                QStringLiteral("Enrolled UEFI certificate authority"),
                QStringLiteral("The UEFI signature database (db) could not be read or parsed. Check efivarfs availability and permissions."),
                CheckStatus::unknown,
                QStringLiteral("UNVERIFIED")};
    }

    if (database.suppliers.isEmpty()) {
        return {QStringLiteral("Recognized Secure Boot supplier"),
                QStringLiteral("Enrolled UEFI certificate authority"),
                QStringLiteral("No certificate subject matched the configured supplier names. This does not prove that no trusted supplier certificate exists in another format."),
                CheckStatus::unsupported,
                QStringLiteral("NO MATCH")};
    }

    return {QStringLiteral("Recognized Secure Boot supplier"),
            QStringLiteral("Enrolled UEFI certificate authority"),
            QStringLiteral("Subject match: %1. Subject text is not a verified fingerprint and does not prove this certificate signed the current boot.")
                .arg(database.suppliers.join(QStringLiteral(", "))),
            CheckStatus::supported,
            QStringLiteral("SUBJECT MATCH")};
}

SecurityCheck mokCertificateCheck()
{
    if (!QFileInfo(QStringLiteral("/sys/firmware/efi/efivars")).isDir()) {
        return {QStringLiteral("MOK certificates"),
                QStringLiteral("No Machine Owner Keys permitted"),
                QStringLiteral("UEFI variables are unavailable; the shim MOK list cannot be inspected."),
                CheckStatus::unknown,
                QStringLiteral("UNVERIFIED")};
    }

    const CertificateDatabaseResult database =
        inspectCertificateDatabase(QStringLiteral("MokListRT"));
    if (!database.readable || !database.parsedDatabase) {
        return {QStringLiteral("MOK certificates"),
                QStringLiteral("No Machine Owner Keys permitted"),
                QStringLiteral("The runtime shim MOK list (MokListRT) could not be read or parsed. This may mean no runtime MOK list is published, or that efivarfs access is restricted."),
                CheckStatus::unknown,
                QStringLiteral("UNVERIFIED")};
    }

    if (database.certificateCount > 0) {
        return {QStringLiteral("MOK certificates"),
                QStringLiteral("No Machine Owner Keys permitted"),
                QStringLiteral("%1 X.509 certificate(s) found in MokListRT. Any enrolled MOK is incompatible with this requirement, including certificates from otherwise recognized signers.")
                    .arg(database.certificateCount),
                CheckStatus::unsupported,
                QStringLiteral("MOK DETECTED")};
    }

    return {QStringLiteral("MOK certificates"),
            QStringLiteral("No Machine Owner Keys permitted"),
            QStringLiteral("No X.509 MOK certificates were found in the readable MokListRT database."),
            CheckStatus::supported,
            QStringLiteral("NONE DETECTED")};
}

SecurityCheck secureBootCheck()
{
    const QString directory = QStringLiteral("/sys/firmware/efi/efivars");
    const QFileInfo directoryInfo(directory);
    if (!directoryInfo.isDir()) {
        return {QStringLiteral("Secure Boot"),
                QStringLiteral("UEFI boot-chain authentication"),
                QStringLiteral("The system was not booted in UEFI mode, so firmware Secure Boot is unavailable."),
                CheckStatus::unsupported,
                QStringLiteral("DISABLED")};
    }

    const QStringList files = QDir(directory).entryList(
        {QStringLiteral("SecureBoot-*")}, QDir::Files | QDir::Readable);
    for (const QString &name : files) {
        QFile file(QDir(directory).filePath(name));
        if (!file.open(QIODevice::ReadOnly)) {
            continue;
        }
        const QByteArray contents = file.readAll();
        if (contents.size() < 5) {
            continue;
        }

        const unsigned char value = static_cast<unsigned char>(contents.at(4));
        if (value == 1) {
            return {QStringLiteral("Secure Boot"),
                    QStringLiteral("UEFI boot-chain authentication"),
                    QStringLiteral("Secure Boot is enabled. This confirms the firmware state, but does not verify that only well-known keys are enrolled."),
                    CheckStatus::unknown,
                    QStringLiteral("ON · KEYS UNVERIFIED")};
        }
        if (value == 0) {
            return {QStringLiteral("Secure Boot"),
                    QStringLiteral("UEFI boot-chain authentication"),
                    QStringLiteral("Secure Boot is disabled in firmware."),
                    CheckStatus::unsupported,
                    QStringLiteral("DISABLED")};
        }
    }

    return {QStringLiteral("Secure Boot"),
            QStringLiteral("UEFI boot-chain authentication"),
            QStringLiteral("The Secure Boot EFI variable was not readable; kernel signature status is unknown."),
            CheckStatus::unknown};
}

SecurityCheck signedKernelCheck()
{
    bool taintReadable = false;
    bool unsignedModuleLoaded = false;
    const QString taintValue = readTextFile(QStringLiteral("/proc/sys/kernel/tainted"));
    if (!taintValue.isEmpty()) {
        bool parsed = false;
        const qulonglong taint = taintValue.toULongLong(&parsed);
        if (parsed) {
            taintReadable = true;
            unsignedModuleLoaded = (taint & (1ULL << 13)) != 0;
        }
    }

    if (unsignedModuleLoaded) {
        return {QStringLiteral("Signed kernel & modules"),
                QStringLiteral("Trusted kernel and driver code"),
                QStringLiteral("The kernel taint flags report an unsigned module has been loaded."),
                CheckStatus::unsupported,
                QStringLiteral("UNSIGNED MODULE")};
    }

    const QString enforcement = readTextFile(QStringLiteral("/sys/module/module/parameters/sig_enforce"));
    if (enforcement == QStringLiteral("0")
        || enforcement.compare(QStringLiteral("N"), Qt::CaseInsensitive) == 0) {
        return {QStringLiteral("Signed kernel & modules"),
                QStringLiteral("Trusted kernel and driver code"),
                QStringLiteral("The kernel reports module signature enforcement is off. A known signer for the running kernel and every loaded module cannot be verified generically."),
                CheckStatus::unsupported,
                QStringLiteral("NOT ENFORCED")};
    }

    const bool enforcementEnabled = enforcement == QStringLiteral("1")
        || enforcement.compare(QStringLiteral("Y"), Qt::CaseInsensitive) == 0;
    const QString detail = enforcementEnabled && taintReadable
        ? QStringLiteral("Module signature enforcement is on and no unsigned-module taint is reported. The running kernel image signer and whether signing keys are well-known still cannot be verified generically.")
        : QStringLiteral("Linux does not expose a generic, authoritative check for the running kernel image signer and whether every loaded module was signed by a well-known key.");
    return {QStringLiteral("Signed kernel & modules"),
            QStringLiteral("Trusted kernel and driver code"),
            detail,
            CheckStatus::unknown,
            enforcementEnabled && taintReadable
                ? QStringLiteral("PARTIAL EVIDENCE")
                : QStringLiteral("UNVERIFIED")};
}

SecurityCheck tpmCheck()
{
    const QStringList devices = QDir(QStringLiteral("/sys/class/tpm")).entryList(
        {QStringLiteral("tpm*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    bool foundTpm = false;
    for (const QString &device : devices) {
        foundTpm = true;
        const QString versionPath = QStringLiteral("/sys/class/tpm/%1/tpm_version_major").arg(device);
        const QString version = readTextFile(versionPath);
        if (version == QStringLiteral("2")) {
            return {QStringLiteral("TPM 2.0"),
                    QStringLiteral("Trusted Platform Module"),
                    QStringLiteral("%1 reports TPM version 2.0.").arg(device),
                    CheckStatus::supported};
        }
    }

    if (foundTpm) {
        return {QStringLiteral("TPM 2.0"),
                QStringLiteral("Trusted Platform Module"),
                QStringLiteral("A TPM device is present, but its version could not be confirmed as 2.0."),
                CheckStatus::unknown};
    }

    return {QStringLiteral("TPM 2.0"),
            QStringLiteral("Trusted Platform Module"),
            QStringLiteral("No TPM device is exposed by the running system."),
            CheckStatus::unsupported};
}

SecurityCheck measuredBootCheck()
{
    const QStringList logs = {
        QStringLiteral("/sys/kernel/security/tpm0/binary_bios_measurements"),
        QStringLiteral("/sys/kernel/security/tpm0/ascii_bios_measurements"),
    };
    for (const QString &path : logs) {
        const QFileInfo logInfo(path);
        if (logInfo.isFile() && logInfo.isReadable() && logInfo.size() > 0) {
            return {QStringLiteral("Measured Boot"),
                    QStringLiteral("Firmware measurement event log"),
                    QStringLiteral("A non-empty TPM firmware measurement log is readable at %1.").arg(path),
                    CheckStatus::supported};
        }
    }

    if (QFileInfo::exists(QStringLiteral("/sys/kernel/security"))) {
        return {QStringLiteral("Measured Boot"),
                QStringLiteral("Firmware measurement event log"),
                QStringLiteral("No TPM firmware measurement log is exposed. Securityfs may need to be mounted."),
                CheckStatus::unsupported};
    }

    return {QStringLiteral("Measured Boot"),
            QStringLiteral("Firmware measurement event log"),
            QStringLiteral("The kernel security interface is unavailable, so measurement logging cannot be checked."),
            CheckStatus::unknown};
}

SecurityCheck iommuCheck()
{
    const QStringList commandLine = kernelCommandLine();
    const bool passthroughRequested = commandLine.contains(QStringLiteral("iommu=pt"))
        || commandLine.contains(QStringLiteral("iommu.passthrough=1"))
        || commandLine.contains(QStringLiteral("amd_iommu=pt"));
    const ConfigValue passthroughDefault =
        kernelConfigValue(QStringLiteral("CONFIG_IOMMU_DEFAULT_DMA_PASSTHROUGH"));
    if (passthroughRequested || (passthroughDefault.readable && passthroughDefault.enabled)) {
        return {QStringLiteral("IOMMU"),
                QStringLiteral("DMA remapping without passthrough"),
                passthroughRequested
                    ? QStringLiteral("The kernel command line requests IOMMU passthrough; devices may bypass normal DMA translation.")
                    : QStringLiteral("The kernel is configured to use passthrough as its default IOMMU DMA mode."),
                CheckStatus::unsupported,
                QStringLiteral("PASSTHROUGH")};
    }

    const QString groupsPath = QStringLiteral("/sys/kernel/iommu_groups");
    const QFileInfo groupsInfo(groupsPath);
    if (!groupsInfo.isDir()) {
        return {QStringLiteral("IOMMU"),
                QStringLiteral("DMA remapping"),
                QStringLiteral("The kernel does not expose IOMMU groups; IOMMU state cannot be confirmed."),
                CheckStatus::unknown};
    }

    const QStringList groups = QDir(groupsPath).entryList(
        QDir::Dirs | QDir::NoDotAndDotDot);
    if (!groups.isEmpty()) {
        const ConfigValue translatedDefault =
            kernelConfigValue(QStringLiteral("CONFIG_IOMMU_DEFAULT_DMA_STRICT"));
        const ConfigValue lazyDefault =
            kernelConfigValue(QStringLiteral("CONFIG_IOMMU_DEFAULT_DMA_LAZY"));
        const bool translatedByDefault = translatedDefault.enabled || lazyDefault.enabled;
        return {QStringLiteral("IOMMU"),
                QStringLiteral("DMA remapping without passthrough"),
                QStringLiteral("%1 IOMMU group(s) are active.%2")
                    .arg(groups.size())
                    .arg(translatedByDefault
                             ? QStringLiteral(" The kernel config defaults to translated DMA.")
                             : QStringLiteral(" The active default DMA domain could not be verified.")),
                translatedByDefault ? CheckStatus::supported : CheckStatus::unknown,
                translatedByDefault ? QStringLiteral("ACTIVE") : QStringLiteral("MODE UNVERIFIED")};
    }

    return {QStringLiteral("IOMMU"),
            QStringLiteral("DMA remapping without passthrough"),
            QStringLiteral("No IOMMU groups are active."),
            CheckStatus::unsupported,
            QStringLiteral("NOT ACTIVE")};
}

SecurityCheck lockdownCheck()
{
    const QString lockdown = readTextFile(QStringLiteral("/sys/kernel/security/lockdown"));
    if (!lockdown.isEmpty()) {
        const QString currentMode = [&lockdown]() {
            const QRegularExpression activeMode(QStringLiteral("\\[([^\\]]+)\\]"));
            const auto match = activeMode.match(lockdown);
            return match.hasMatch() ? match.captured(1) : QStringLiteral("unknown");
        }();

        const bool integrityActive = currentMode == QStringLiteral("integrity");
        return {QStringLiteral("Kernel Lockdown: Integrity"),
                QStringLiteral("Kernel self-protection mode"),
                integrityActive
                    ? QStringLiteral("Lockdown Integrity mode is explicitly active.")
                    : QStringLiteral("Lockdown is available, but the active mode is %1; Integrity mode must be active.")
                          .arg(currentMode),
                integrityActive ? CheckStatus::supported : CheckStatus::unsupported,
                integrityActive ? QStringLiteral("INTEGRITY ACTIVE") : QStringLiteral("NOT ACTIVE")};
    }

    return {QStringLiteral("Kernel Lockdown: Integrity"),
            QStringLiteral("Kernel self-protection mode"),
            QStringLiteral("The active lockdown mode cannot be read. Kernel config alone cannot prove Integrity mode was enabled at boot."),
            CheckStatus::unknown,
            QStringLiteral("MODE UNVERIFIED")};
}

SecurityCheck kexecCheck()
{
    const ConfigValue kexecConfig = kernelConfigValue(QStringLiteral("CONFIG_KEXEC"));
    const ConfigValue kexecFileConfig = kernelConfigValue(QStringLiteral("CONFIG_KEXEC_FILE"));
    if (kexecConfig.readable && !kexecConfig.enabled
        && kexecFileConfig.readable && !kexecFileConfig.enabled) {
        return {QStringLiteral("Kexec"),
                QStringLiteral("Kernel replacement loading"),
                QStringLiteral("Both CONFIG_KEXEC and CONFIG_KEXEC_FILE are disabled in the kernel configuration."),
                CheckStatus::supported,
                QStringLiteral("DISABLED")};
    }

    const QString disabled = readTextFile(QStringLiteral("/proc/sys/kernel/kexec_load_disabled"));
    if (disabled == QStringLiteral("1")) {
        return {QStringLiteral("Kexec"),
                QStringLiteral("Kernel replacement loading"),
                QStringLiteral("Kexec loading is disabled at runtime. This interface does not prove it was disabled explicitly at boot."),
                CheckStatus::unknown,
                QStringLiteral("DISABLED · BOOT UNVERIFIED")};
    }
    if (disabled == QStringLiteral("0")) {
        return {QStringLiteral("Kexec"),
                QStringLiteral("Kernel replacement loading"),
                QStringLiteral("Kexec is compiled in and kernel loading is enabled. EAC v2 anti-cheat requires Kexec to be disabled; disable it before attempting compatibility."),
                CheckStatus::unsupported,
                QStringLiteral("ENABLED")};
    }

    if (!kexecConfig.readable || !kexecFileConfig.readable) {
        return {QStringLiteral("Kexec"),
                QStringLiteral("Kernel replacement loading"),
                QStringLiteral("Kernel config and runtime load policy are unavailable; boot-time disablement cannot be verified."),
                CheckStatus::unknown,
                QStringLiteral("UNVERIFIED")};
    }

    return {QStringLiteral("Kexec"),
            QStringLiteral("Kernel replacement loading"),
            QStringLiteral("At least one Kexec interface is enabled in kernel config; runtime policy and explicit boot-time disablement cannot be verified. EAC v2 anti-cheat requires Kexec to be disabled."),
            CheckStatus::unknown};
}

SecurityCheck bpfLsmCheck()
{
    const QString activeLsMs = readTextFile(QStringLiteral("/sys/kernel/security/lsm"));
    if (activeLsMs.isEmpty()) {
        return {QStringLiteral("BPF LSM"),
                QStringLiteral("eBPF security hooks"),
                QStringLiteral("The active LSM list is unavailable, so BPF LSM activation cannot be confirmed."),
                CheckStatus::unknown};
    }

    const QStringList activeLsMNames = activeLsMs.split(QLatin1Char(','), Qt::SkipEmptyParts);
    const bool active = activeLsMNames.contains(QStringLiteral("bpf"));
    return {QStringLiteral("BPF LSM"),
            QStringLiteral("eBPF security hooks"),
            active ? QStringLiteral("BPF LSM is listed among the active Linux Security Modules.")
                   : QStringLiteral("BPF LSM is not present in the active LSM list."),
            active ? CheckStatus::supported : CheckStatus::unsupported,
            active ? QStringLiteral("ACTIVE") : QStringLiteral("NOT ACTIVE")};
}
} // namespace

QVector<SecurityCheck> SystemInspector::inspect()
{
    return {
        signedKernelCheck(),
        secureBootCheck(),
        secureBootSupplierCheck(),
        mokCertificateCheck(),
        tpmCheck(),
        measuredBootCheck(),
        iommuCheck(),
        lockdownCheck(),
        kexecCheck(),
        bpfLsmCheck(),
    };
}
