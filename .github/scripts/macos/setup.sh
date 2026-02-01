#!/bin/zsh
# Setup macOS environment
set -e

echo "Setting up macOS environment..."

sudo xcode-select --switch /Applications/Xcode_26.1.app/Contents/Developer

local -a unwanted_formulas=()
local -a remove_formulas=()
for formula (${unwanted_formulas}) {
  if [[ -d ${HOMEBREW_PREFIX}/Cellar/${formula} ]] remove_formulas+=(${formula})
}

if (( #remove_formulas )) brew uninstall --ignore-dependencies ${remove_formulas}

local -A arch_names=(x86_64 intel arm64 apple)
print "cpuName=${arch_names[$1]}" >> $GITHUB_OUTPUT

local xcode_cas_path="${HOME}/Library/Developer/Xcode/DerivedData/CompilationCache.noindex"

if ! [[ -d ${xcode_cas_path} ]] mkdir -p ${xcode_cas_path}

print "xcodeCasPath=${xcode_cas_path}" >> $GITHUB_OUTPUT

echo "macOS environment setup complete!"

echo "Installing dependencies for ${TARGET_ARCH}..."
brew update

if [[ "${TARGET_ARCH}" == "x86_64" ]]; then
  # Install x86_64 Homebrew and dependencies for cross-compilation
  echo "Installing x86_64 Homebrew for cross-compilation..."
  arch -x86_64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" || true
  arch -x86_64 /usr/local/bin/brew install cmake zlib libwebsockets
else
  echo "Installing native dependencies..."
  brew install cmake zlib libwebsockets
fi

echo "Dependencies installed successfully!"
