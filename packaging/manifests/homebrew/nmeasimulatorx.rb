# Template: packaging/manifests/update_manifests.py fills in the @...@ placeholders.
# Published in the tap github.com/Dimitrios-Kafetzis/homebrew-tap as Casks/nmeasimulatorx.rb.
cask "nmeasimulatorx" do
  arch arm: "arm64", intel: "x86_64"

  version "@VERSION@"
  sha256 arm:   "@SHA256_MACOS_ARM64@",
         intel: "@SHA256_MACOS_X86_64@"

  url "https://github.com/Dimitrios-Kafetzis/NMEA_Simulator_X/releases/download/v#{version}/NMEASimulatorX-#{version}-macos-#{arch}.dmg"
  name "NMEA Simulator X"
  desc "NMEA 0183, AIS and Signal K data stream simulator"
  homepage "https://dimitrios-kafetzis.github.io/NMEA_Simulator_X/"

  livecheck do
    url :url
    strategy :github_latest
  end

  depends_on macos: ">= :ventura"

  app "NMEASimulatorX.app"
  binary "#{appdir}/NMEASimulatorX.app/Contents/MacOS/nmeasim"

  zap trash: [
    "~/Library/Application Support/NMEASimulatorX",
    "~/Library/Caches/NMEASimulatorX",
    "~/Library/Preferences/io.github.dimitrios-kafetzis.NMEASimulatorX.plist",
  ]

  caveats <<~EOS
    NMEA Simulator X is signed ad hoc but not notarised by Apple. The first time you open
    it, macOS blocks it: open System Settings > Privacy & Security and click Open Anyway.
  EOS
end
