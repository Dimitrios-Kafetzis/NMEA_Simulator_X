# Install on Windows, macOS or Linux

NMEA Simulator X is distributed free of charge and **without commercial code-signing
certificates**. This keeps the project independent of paid developer programs, but it means
Windows and macOS show a warning the first time you start the application. The warning is
expected, and this page describes exactly what to click. See
[ADR 0005](../adr/0005-unsigned-distribution.md) for the reasoning.

Every package contains both the desktop application, *NMEA Simulator X*, and the command-line
tool, `nmeasim`.

## Downloads

Download from the [releases page](https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/releases/latest).
Replace `<version>` with the release number, for example `1.0.0`.

| System | File | Requirements |
| --- | --- | --- |
| Windows, installer | `NMEASimulatorX-<version>-win64.exe` | Windows 10 1809 or later, 64-bit |
| Windows, no installation | `NMEASimulatorX-<version>-win64-portable.zip` | Windows 10 1809 or later, 64-bit |
| macOS, Apple Silicon (M1 and later) | `NMEASimulatorX-<version>-macos-arm64.dmg` | macOS 13.3 or later |
| macOS, Intel | `NMEASimulatorX-<version>-macos-x86_64.dmg` | macOS 13.3 or later |
| Linux, x86_64 | `NMEASimulatorX-<version>-x86_64.AppImage` | glibc 2.39 or later: Ubuntu 24.04, Debian 13, Fedora 40 or newer |
| Linux, ARM 64-bit | `NMEASimulatorX-<version>-aarch64.AppImage` | glibc 2.39 or later, for example Raspberry Pi OS based on Debian 13 |
| Linux, any distribution with Flatpak | `NMEASimulatorX-<version>-x86_64.flatpak` | Flatpak 1.12 or later |
| All | `SHA256SUMS.txt` | Checksums of every file above |

!!! tip "Verify what you download"
    Compare the checksum of the file with its line in `SHA256SUMS.txt` before you bypass a
    security prompt, and download only from the releases page.

    === "Windows (PowerShell)"

        ```powershell
        Get-FileHash .\NMEASimulatorX-1.0.0-win64.exe -Algorithm SHA256
        ```

    === "macOS"

        ```bash
        shasum -a 256 -c SHA256SUMS.txt --ignore-missing
        ```

    === "Linux"

        ```bash
        sha256sum --check --ignore-missing SHA256SUMS.txt
        ```

!!! info "Package managers"
    Manifests for winget, Scoop, a Homebrew tap and Flathub are prepared with every release
    and are being submitted to those repositories. Until a listing appears there, install from
    the downloads above; the commands for each package manager are given below for when it
    does.

=== "Windows"

    **Installer**

    1. Run `NMEASimulatorX-<version>-win64.exe`.
    2. Microsoft Defender SmartScreen shows a blue window titled *Windows protected your PC*
       saying that it *prevented an unrecognized app from starting*. It offers only
       *Don't run*. Click **More info**: the window now shows *App:* with the file name and
       *Publisher: Unknown publisher*, and a second button. Click **Run anyway**.
    3. User Account Control asks *Do you want to allow this app from an unknown publisher to
       make changes to your device?* Click **Yes**.
    4. Follow the installer. On the *Install Options* page, choose *Add NMEASimulatorX to the
       system PATH for all users* (or *for current user*) if you want to use `nmeasim` in any
       terminal, and tick *Create NMEASimulatorX Desktop Icon* if you want one.

    The application is added to the Start menu. It is installed in
    `C:\Program Files\NMEA Simulator X`, where `nmeasim.exe` sits next to
    `NMEASimulatorX.exe`. When you install a newer version, the installer reports that
    *NMEA Simulator X is already installed* and offers to uninstall the old version first;
    answer *Yes*. Uninstall from *Settings → Apps*.

    **Portable ZIP**

    1. Right-click `NMEASimulatorX-<version>-win64-portable.zip`, choose *Properties*, tick
       **Unblock** at the bottom of the *General* tab and click *OK*. Without this, Windows
       marks every extracted file as downloaded and SmartScreen asks again for each one.
    2. Extract the ZIP anywhere, for example to a USB stick, and run `NMEASimulatorX.exe` in
       the extracted folder. If SmartScreen appears, proceed as for the installer.

    The command-line tool is `nmeasim.exe` in the same folder. Settings are stored in the
    user's registry, not in the folder.

    **winget and Scoop**

    ```powershell
    winget install DimitriosKafetzis.NMEASimulatorX   # installer
    scoop bucket add extras
    scoop install nmeasimulatorx                      # portable, with nmeasim on the PATH
    ```

=== "macOS"

    **Disk image**

    1. Choose the image for your Mac: *Apple menu → About This Mac* shows *Chip: Apple M…*
       for `macos-arm64` or *Processor: … Intel …* for `macos-x86_64`.
    2. Open the `.dmg` and drag *NMEASimulatorX* onto the *Applications* folder next to it.
    3. Start NMEA Simulator X from *Applications*. macOS shows a dialog *"NMEASimulatorX" Not
       Opened*: *Apple could not verify "NMEASimulatorX" is free of malware that may harm your
       Mac or compromise your privacy.* It offers *Done* and *Move to Trash*. Click **Done**.
    4. Open **System Settings → Privacy & Security** and scroll to *Security*. The message
       *"NMEASimulatorX" was blocked to protect your Mac.* has an **Open Anyway** button next
       to it. Click it, confirm **Open Anyway** in the dialog that follows and authenticate
       with your password or Touch ID.
    5. The application starts. From now on it opens normally, until you install a new
       version, which asks once more.

    !!! note "Why not right-click → Open?"
        Since macOS 15 the right-click shortcut no longer bypasses Gatekeeper for
        applications that are not notarised. *Open Anyway* in System Settings is the supported
        way. The application is *ad-hoc signed*, which Apple Silicon requires for it to launch
        at all, but it is not notarised by Apple.

    The command-line tool is inside the application. To use it from a terminal:

    ```bash
    sudo ln -s /Applications/NMEASimulatorX.app/Contents/MacOS/nmeasim /usr/local/bin/nmeasim
    nmeasim --version
    ```

    If macOS blocks `nmeasim` itself the first time, approve it with *Open Anyway* in the
    same place, or remove the download quarantine from the whole application once:
    `xattr -dr com.apple.quarantine /Applications/NMEASimulatorX.app`.

    **Homebrew**

    ```bash
    brew install --cask dimitrios-kafetzis/tap/nmeasimulatorx
    ```

    The official Homebrew cask repository no longer accepts applications that are not
    notarised, so the cask lives in the project's own tap. Homebrew installs the same disk
    image, puts `nmeasim` on the PATH, and the first start needs the same *Open Anyway* step.

=== "Linux"

    **Flatpak**

    The Flatpak runs on any distribution and gets its libraries from the KDE runtime on
    Flathub:

    ```bash
    flatpak remote-add --user --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
    flatpak install --user NMEASimulatorX-<version>-x86_64.flatpak
    flatpak run io.github.dimitrios_kafetzis.NMEASimulatorX
    flatpak run --command=nmeasim io.github.dimitrios_kafetzis.NMEASimulatorX --version
    ```

    Once the application is listed on Flathub, `flatpak install flathub
    io.github.dimitrios_kafetzis.NMEASimulatorX` installs it with automatic updates, on
    x86_64 and ARM.

    **AppImage**

    ```bash
    chmod +x NMEASimulatorX-<version>-x86_64.AppImage
    ./NMEASimulatorX-<version>-x86_64.AppImage
    ```

    AppImages need FUSE 2; if the file does not start, install it
    (`sudo apt install libfuse2t64` on Ubuntu 24.04, `sudo dnf install fuse-libs` on Fedora)
    or run it with `--appimage-extract-and-run`. The command-line tool is inside:

    ```bash
    ./NMEASimulatorX-<version>-x86_64.AppImage nmeasim --version
    ln -s "$PWD/NMEASimulatorX-<version>-x86_64.AppImage" ~/.local/bin/nmeasim
    nmeasim run --stdout --duration 5
    ```

    A link named `nmeasim` starts the command-line tool; any other name starts the desktop
    application. The AppImage needs glibc 2.39 or later; on older distributions use the
    Flatpak.

    Debian and RPM packages are not published, because no distribution ships the Qt version
    the application needs ([ADR 0016](../adr/0016-linux-packages.md)).

    **Serial ports**

    Serial port access requires membership of the `dialout` group on Debian-based systems
    (`uucp` on Arch):

    ```bash
    sudo usermod -aG dialout "$USER"
    ```

    Log out and in again for the group change to take effect.

## Check the installation

```bash
nmeasim --version
```

prints `NMEASimulatorX` and the version, for example `NMEASimulatorX 1.0.0`. *Help → About*
in the desktop application shows the same. Continue with
[Your first simulated voyage](../tutorials/first-voyage.md).
