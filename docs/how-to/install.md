# Install on Windows, macOS or Linux

NMEA Simulator X is distributed free of charge and **without commercial code-signing
certificates**. This keeps the project independent of paid developer programs, but it means
every operating system shows a warning the first time you start the application. The warning
is expected and this page shows exactly what to click. See
[ADR 0005](../adr/0005-unsigned-distribution.md) for the reasoning.

!!! tip "Verify what you download"
    Every release lists SHA-256 checksums for its files. Compare them before you bypass a
    security prompt, and download only from the
    [official releases page](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/releases)
    or the package managers listed below.

=== "Windows"

    **Option A: winget (recommended)**

    ```powershell
    winget install NMEASimulatorX
    ```

    **Option B: Scoop**

    ```powershell
    scoop bucket add extras
    scoop install nmeasimulatorx
    ```

    **Option C: installer or portable zip from GitHub Releases**

    1. Download `NMEASimulatorX-<version>-win64.exe` (installer) or
       `NMEASimulatorX-<version>-win64-portable.zip`.
    2. Start it. Windows SmartScreen shows a blue *"Windows protected your PC"* dialog because
       the file has no publisher certificate.
    3. Click **More info**, then **Run anyway**. This is only asked once per file.

    The portable zip needs no installation: extract it anywhere and run `NMEASimulatorX.exe`.

=== "macOS"

    **Option A: Homebrew (recommended)**

    ```bash
    brew install --cask nmeasimulatorx
    ```

    Homebrew downloads the same disk image as below but handles placement in `/Applications`.

    **Option B: disk image from GitHub Releases**

    1. Download `NMEASimulatorX-<version>-macos.dmg`, open it and drag the application to
       *Applications*.
    2. Start it. macOS reports that it *cannot verify the developer* and offers only to move
       the app to the bin. Close that dialog.
    3. Open **System Settings → Privacy & Security**, scroll to the *Security* section and click
       **Open Anyway** next to the NMEA Simulator X message. Confirm with your password or
       Touch ID.
    4. Start the application again. From now on it opens normally.

    !!! note "Why not right-click → Open?"
        Since macOS 15 the right-click shortcut no longer bypasses Gatekeeper for applications
        without notarisation. The *Open Anyway* button in System Settings is the supported way.

    The application is *ad-hoc signed*, which is required for Apple Silicon Macs to launch it
    at all, but it is not notarised by Apple.

=== "Linux"

    **Option A: Flatpak (recommended)**

    ```bash
    flatpak install flathub io.github.dimitrios_kafetzis.NMEASimulatorX
    ```

    **Option B: AppImage**

    ```bash
    chmod +x NMEASimulatorX-<version>-x86_64.AppImage
    ./NMEASimulatorX-<version>-x86_64.AppImage
    ```

    **Option C: Debian or RPM package**

    ```bash
    sudo apt install ./nmeasimulatorx_<version>_amd64.deb   # Debian, Ubuntu
    sudo dnf install ./nmeasimulatorx-<version>.x86_64.rpm  # Fedora, RHEL
    ```

    Serial port access requires membership of the `dialout` group on Debian-based systems
    (`uucp` on Arch):

    ```bash
    sudo usermod -aG dialout "$USER"
    ```

    Log out and in again for the group change to take effect.
