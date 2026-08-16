#!/usr/bin/env bash
# Builds the RPM from the current state of the working tree.
#
#     ./packaging/make-rpm.sh          # build
#     ./packaging/make-rpm.sh --lint   # plus rpmlint, if installed
#
# The source archive name has to match Source0 in the spec. It is
# therefore derived from the name and version in the spec rather than
# typed - rpmbuild would only notice a mismatch after compiling.
#
# git archive rather than tar: only what is in the repository ends up in
# the archive. A forgotten build/ directory or a leftover from a session
# would otherwise be found first by whoever downloads the package.

set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
SPEC="$HERE/nt-taskmanager.spec"

LINT=false
[ "${1:-}" = "--lint" ] && LINT=true

NAME="$(rpmspec -q --queryformat '%{name}\n' "$SPEC" | head -1)"
VERSION="$(rpmspec -q --queryformat '%{version}\n' "$SPEC" | head -1)"
echo "Package: $NAME $VERSION"

TREE="${HOME}/rpmbuild"
mkdir -p "$TREE"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# The directory INSIDE the archive has to be called <name>-<version>,
# otherwise %autosetup will not find it.
ARCHIVE="$TREE/SOURCES/${NAME}-${VERSION}.tar.gz"
git -C "$ROOT" archive --format=tar.gz \
    --prefix="${NAME}-${VERSION}/" -o "$ARCHIVE" HEAD
echo "Source archive: $ARCHIVE ($(du -h "$ARCHIVE" | cut -f1))"

cp "$SPEC" "$TREE/SPECS/"

rpmbuild -ba "$TREE/SPECS/$(basename "$SPEC")"

echo
echo "Built:"
find "$TREE/RPMS" "$TREE/SRPMS" -name "${NAME}-${VERSION}*" -newermt '-5 minutes' \
    -printf '  %p  (%s bytes)\n'

if $LINT; then
    if command -v rpmlint >/dev/null; then
        echo
        echo "rpmlint:"
        rpmlint "$TREE"/RPMS/*/"${NAME}-${VERSION}"*.rpm \
                "$TREE"/SRPMS/"${NAME}-${VERSION}"*.rpm || true
    else
        echo
        echo "rpmlint is not installed - skipped."
        echo "  sudo dnf install rpmlint"
    fi
fi
