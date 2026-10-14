# The Mauvely OS installer image: the live system the ISO boots.
#
# image-builder's `bootc-generic-iso` turns this image into the ISO's squashfs
# and puts the OS image (the payload) in its containers-storage; the app runs
# `bootc install to-disk` from here. scripts/build-iso.sh does both. Started
# from OS/image's spike (spikes/installer/Containerfile.installer, plan 19 P1).

# ── The app, built and tested ────────────────────────────────────────────────
FROM quay.io/fedora/fedora:44 AS app
RUN dnf install -qy --setopt=install_weak_deps=False \
      cmake ninja-build gcc-c++ qt6-qtbase-devel qt6-qtbase-gui \
    && dnf clean all
COPY CMakeLists.txt /src/
COPY src /src/src
COPY resources /src/resources
COPY tests /src/tests
RUN cmake -S /src -B /build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr \
 && cmake --build /build -j8 \
 && QT_QPA_PLATFORM=offscreen ctest --test-dir /build --output-on-failure \
 && DESTDIR=/out cmake --install /build

# ── The live system ──────────────────────────────────────────────────────────
FROM quay.io/fedora/fedora-bootc:44

# What image-builder needs to make a bootable ISO of this image, then what the
# app needs: Qt on Wayland, cage and seatd. NetworkManager is in the base and
# brings wired DHCP up on its own.
RUN dnf install -qy --setopt=install_weak_deps=False \
      dracut-live dracut-network dracut-config-generic \
      grub2-efi-x64 grub2-efi-x64-cdboot shim-x64 \
      grub2-tools grub2-common grub2-pc-modules \
      xorriso squashfs-tools isomd5sum podman skopeo lsblk \
      qt6-qtbase-gui qt6-qtwayland cage seatd \
    && dnf clean all

# what image-builder looks for: the EFI binaries under /boot/efi/EFI/<vendor>
RUN mkdir -p /boot/efi && cp -ra /usr/lib/efi/*/*/EFI /boot/efi

COPY --from=app /out/usr/bin/mauvely-installer /usr/bin/mauvely-installer
RUN mkdir -p /usr/lib/image-builder/bootc
COPY image/iso.yaml /usr/lib/image-builder/bootc/iso.yaml
COPY image/mauvely-installer.service /usr/lib/systemd/system/mauvely-installer.service

# udisks2 would mount what the installer is about to wipe; bootloader-update
# and the GPT auto-generator act on the disks of the machine this boots on.
# No sshd: the live system has no way in but its screen.
RUN systemctl enable mauvely-installer.service NetworkManager.service \
 && systemctl disable sshd.service \
 && systemctl mask udisks2.service bootloader-update.service \
 && rm -f /usr/lib/systemd/system-generators/systemd-gpt-auto-generator

# an initramfs that can boot a squashfs from the ISO
RUN kernel=$(ls /usr/lib/modules | head -1) && \
    DRACUT_NO_XATTR=1 dracut --force --zstd --reproducible --no-hostonly \
      --add "dmsquash-live" "/usr/lib/modules/${kernel}/initramfs.img" "${kernel}"
