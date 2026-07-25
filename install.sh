#!/usr/bin/env bash
set -euo pipefail

REPO="https://github.com/khosta77/stmtool.git"

echo "=== stmtool installer ==="

check_cmd() {
    command -v "$1" >/dev/null 2>&1
}

if ! check_cmd python3; then
    echo "Error: python3 not found. Install Python 3.10+ first."
    exit 1
fi

PY_VERSION=$(python3 -c 'import sys; print(f"{sys.version_info.minor}")')
if [ "$PY_VERSION" -lt 10 ]; then
    echo "Error: Python 3.10+ required, found 3.${PY_VERSION}"
    exit 1
fi

if ! check_cmd pipx; then
    echo "pipx not found, installing..."
    python3 -m pip install --user pipx 2>/dev/null || pip install --user pipx
    python3 -m pipx ensurepath
    export PATH="$HOME/.local/bin:$PATH"
fi

EXISTING=$(which stmtool 2>/dev/null || true)
if [ -n "$EXISTING" ] && [[ "$EXISTING" != *".local/bin"* ]] && [[ "$EXISTING" != *"pipx"* ]]; then
    echo "Removing old stmtool at $EXISTING..."
    pip3 uninstall -y stmtool 2>/dev/null || rm -f "$EXISTING"
fi

if pipx list 2>/dev/null | grep -q stmtool; then
    echo "Removing previous pipx installation..."
    pipx uninstall stmtool
fi

echo "Clearing SDK cache..."
rm -rf "$HOME/.stmtool/stm32-sdk"

LATEST_TAG=$(git ls-remote --tags --sort=-v:refname "$REPO" "v*" | grep -v '\^{}' | head -1 | sed 's|.*refs/tags/||')

if [ -z "$LATEST_TAG" ]; then
    echo "Error: no release tags found in $REPO"
    exit 1
fi

echo "Latest release: $LATEST_TAG"

SPEC="stmtool @ git+${REPO}@${LATEST_TAG}"

echo "Installing stmtool ($LATEST_TAG)..."
pipx install "$SPEC"

SHELL_NAME=$(basename "$SHELL")
COMPLETION_MARKER="# stmtool completion"

case "$SHELL_NAME" in
    zsh)
        RC_FILE="$HOME/.zshrc"
        ;;
    bash)
        RC_FILE="$HOME/.bashrc"
        ;;
    *)
        RC_FILE=""
        ;;
esac

if [ -n "$RC_FILE" ]; then
    if ! grep -q "$COMPLETION_MARKER" "$RC_FILE" 2>/dev/null; then
        echo ""
        echo "Installing $SHELL_NAME completion..."
        {
            echo ""
            echo "$COMPLETION_MARKER"
            stmtool completion "$SHELL_NAME" 2>/dev/null
        } >> "$RC_FILE"
        echo "Completion added to $RC_FILE"
    fi
fi

echo ""
echo "stmtool installed:"
stmtool show-version
echo ""
echo "Run 'stmtool doctor' to check your environment."
