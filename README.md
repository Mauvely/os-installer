# Mauvely OS installer

The Mauvely OS installer is a small Qt app. You:
1. enter an enrolment token;
2. pick a disk.

It then installs Mauvely OS with full-disk encryption unlocked by the TPM,
Secure Boot on, and the device ready to enrol on first boot.

## mauvely-signin

This repository also holds `mauvely-signin` (`signin/`), the installed
system's sign-in window. It shows Mauvely's own sign-in in a frameless,
branded window, and links the computer to the person's Mauvely account. It
needs Qt WebEngine, so it is built only with `-DMAUVELY_SIGNIN=ON`, and the
Mauvely OS image builds it from here.

Licensed under the GNU GPL, version 3 or later.
