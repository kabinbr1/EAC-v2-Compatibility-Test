# EAC v2 Compatibility Test

EAC v2 Compatibility Test is a native Qt 6 desktop application that summarizes Linux security
features detected on the running system.

<img width="2050" height="1164" alt="image" src="https://github.com/user-attachments/assets/6bea683d-e2fa-473b-92d8-e019985999e5" />


## Compatibility requirements

- **Signed kernel and modules:** reports a loaded unsigned module when the kernel
  taint flag exposes one. Linux does not provide a generic userspace interface to
  verify the running kernel image's signing key or establish that every signing
  key is a well-known key, so absence of that taint is not treated as proof.
- **TPM 2.0:** checks for a TPM device that reports TPM version 2.0.
- **Secure Boot:** reads the UEFI Secure Boot variable. Enabled Secure Boot is
  shown as unverified because the app does not audit the enrolled key database.
- **Recognized Secure Boot supplier:** parses X.509 certificate entries in the
  UEFI `db` variable and checks certificate subject text against the listed
  suppliers (Canonical, Fedora, Universal Blue, Debian, openSUSE, SUSE Linux
  Enterprise, Rocky Linux, Microsoft, AlmaLinux). A match is a subject-name
  match only—not fingerprint verification, proof of key authenticity, or proof
  that the certificate signed the current boot. MOK and revoked-certificate
  databases are not currently checked.
- **Measured Boot:** checks for the TPM firmware event log exposed by securityfs.
- **IOMMU:** checks for active IOMMU groups and flags known passthrough boot
  options or a passthrough kernel default. It cannot prove the DMA domain selected
  for every device on every platform.
- **Kernel Lockdown Integrity:** requires the runtime lockdown interface to show
  `integrity` as the active mode; support in kernel config alone is not enough.
- **Kexec disabled:** positive if both legacy and file-based Kexec interfaces are
  absent from the kernel config. If Kexec is enabled in the kernel but disabled at
  runtime, the app reports that boot-time disablement could not be verified.
- **BPF LSM:** checks whether `bpf` appears in the active LSM list, not merely
  whether eBPF is compiled into the kernel.

Checks are positive, negative, or unverified. An unverified result means the
running system does not expose enough information to prove the requirement.
The app does not request elevated privileges.

## Build

Requirements: CMake 3.16+, a C++17 compiler, and Qt 6 Network and Widgets.

```sh
cmake -S . -B build
cmake --build build
./build/eac-v2-compatibility-test
```
