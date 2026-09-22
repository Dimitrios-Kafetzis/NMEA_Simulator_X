# 0005 Unsigned distribution through free channels

- Status: accepted
- Date: 2026-09-22

## Context and problem statement

Users on Windows, macOS and Linux must be able to download and run the simulator easily. The
project will not purchase a Windows code-signing certificate or an Apple Developer Program
membership. How can distribution be as smooth as possible under that constraint?

## Decision drivers

- Zero recurring cost.
- Minimise the number of security prompts a user must click through.
- Never ship something that cannot launch at all.

## Considered options

1. GitHub Releases only, unsigned.
2. GitHub Releases plus free package managers, with ad-hoc signing on macOS.
3. Buy certificates.

## Decision outcome

Option 2.

- **macOS:** every build is ad-hoc signed (`codesign --sign -`). Apple Silicon refuses to
  launch unsigned arm64 binaries, so this step is mandatory. The app is not notarised; users
  use *Open Anyway* in System Settings once. A Homebrew cask is published for the common case.
- **Windows:** an NSIS installer and a portable zip are published. SmartScreen shows a warning
  until reputation accrues. winget and Scoop manifests are published.
- **Linux:** AppImage, deb, rpm and a Flathub submission. No signing issue exists.
- Every release publishes SHA-256 checksums so users can verify downloads before bypassing a
  prompt.

### Consequences

- The installation guide documents each platform's one-time prompt with exact steps.
- The release workflow must include the ad-hoc signing step for macOS.
- If certificates are acquired later, only the packaging scripts change; this ADR would then
  be superseded.
