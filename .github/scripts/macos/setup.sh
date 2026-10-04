#!/bin/zsh
# Setup macOS environment
set -e

echo "Setting up macOS environment..."

sudo xcode-select --switch /Applications/Xcode_26.1.app/Contents/Developer

local brew_prefix=$(brew --prefix)
# The runner image ships the deprecated openssl@1.1, whose bin/openssl symlink
# makes linking openssl@3 (a dependency of libwebsockets) fail
local -a unwanted_formulas=(openssl@1.1)
local -a remove_formulas=()
for formula (${unwanted_formulas}) {
  if [[ -d ${brew_prefix}/Cellar/${formula} ]] remove_formulas+=(${formula})
}

if (( #remove_formulas )) brew uninstall --ignore-dependencies ${remove_formulas}

# Brew may no longer know about the keg (disabled formula), so also drop any symlinks left behind
for link (${brew_prefix}/bin/*(N@)) {
  if [[ $(readlink ${link}) == *openssl@1.1* ]] rm -f ${link}
}

print "cpuName=${TARGET_ARCH}" >> $GITHUB_OUTPUT

local xcode_cas_path="${HOME}/Library/Developer/Xcode/DerivedData/CompilationCache.noindex"

if ! [[ -d ${xcode_cas_path} ]] mkdir -p ${xcode_cas_path}

print "xcodeCasPath=${xcode_cas_path}" >> $GITHUB_OUTPUT


echo "Installing dependencies for ${TARGET_ARCH}..."
brew update

if [[ "${TARGET_ARCH}" == "x86_64" ]]; then
  # Install x86_64 Homebrew and dependencies for cross-compilation
  echo "Installing x86_64 Homebrew for cross-compilation..."
  arch -x86_64 /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" || true
  arch -x86_64 /usr/local/bin/brew unlink openssl@1.1 || true
  arch -x86_64 /usr/local/bin/brew install cmake libwebsockets sdl2
else
  echo "Installing native dependencies..."
  brew install cmake libwebsockets sdl2
fi

echo "Dependencies installed successfully!"
