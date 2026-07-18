#!/usr/bin/env bash
# One command on the Pi (copy-paste into terminal):
#   curl -sSL https://raw.githubusercontent.com/IDEAGREY/MILLIE/main/millie-pi/quick-install-pi.sh | bash
set -euo pipefail
CLONE="${MILLIE_PI_SRC:-$HOME/MILLIE}"
if [ ! -d "$CLONE/.git" ]; then
  git clone --depth 1 https://github.com/IDEAGREY/MILLIE.git "$CLONE"
fi
cd "$CLONE/millie-pi"
git pull --ff-only 2>/dev/null || true
bash setup.sh
echo ""
echo "Starting hub now..."
exec "$HOME/.millie-pi-install/run-millie-pi.sh"
