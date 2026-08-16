#!/usr/bin/env bash
# Baut das RPM aus dem aktuellen Stand des Arbeitsverzeichnisses.
#
#     ./packaging/mach-rpm.sh            # bauen
#     ./packaging/mach-rpm.sh --pruefen  # zusaetzlich rpmlint, falls da
#
# Der Quellarchiv-Name muss zu Source0 in der Spec passen. Deshalb wird
# er hier aus Name und Version der Spec abgeleitet und nicht getippt -
# eine Abweichung merkt rpmbuild erst nach dem Uebersetzen.
#
# git archive statt tar: so landet nur, was auch im Repository liegt.
# Ein vergessenes build/-Verzeichnis oder ein Sitzungsrest im Archiv
# faellt sonst erst dem auf, der das Paket herunterlaedt.

set -euo pipefail

HIER="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
WURZEL="$(cd "$HIER/.." && pwd)"
SPEC="$HIER/nt-taskmanager.spec"

PRUEFEN=false
[ "${1:-}" = "--pruefen" ] && PRUEFEN=true

NAME="$(rpmspec -q --queryformat '%{name}\n' "$SPEC" | head -1)"
VERSION="$(rpmspec -q --queryformat '%{version}\n' "$SPEC" | head -1)"
echo "Paket: $NAME $VERSION"

BAUM="${HOME}/rpmbuild"
mkdir -p "$BAUM"/{BUILD,BUILDROOT,RPMS,SOURCES,SPECS,SRPMS}

# Der Ordner IM Archiv muss <name>-<version> heissen, sonst findet
# %autosetup ihn nicht.
ARCHIV="$BAUM/SOURCES/${NAME}-${VERSION}.tar.gz"
git -C "$WURZEL" archive --format=tar.gz \
    --prefix="${NAME}-${VERSION}/" -o "$ARCHIV" HEAD
echo "Quellarchiv: $ARCHIV ($(du -h "$ARCHIV" | cut -f1))"

cp "$SPEC" "$BAUM/SPECS/"

rpmbuild -ba "$BAUM/SPECS/$(basename "$SPEC")"

echo
echo "Entstanden:"
find "$BAUM/RPMS" "$BAUM/SRPMS" -name "${NAME}-${VERSION}*" -newermt '-5 minutes' \
    -printf '  %p  (%s Bytes)\n'

if $PRUEFEN; then
    if command -v rpmlint >/dev/null; then
        echo
        echo "rpmlint:"
        rpmlint "$BAUM"/RPMS/*/"${NAME}-${VERSION}"*.rpm \
                "$BAUM"/SRPMS/"${NAME}-${VERSION}"*.rpm || true
    else
        echo
        echo "rpmlint ist nicht installiert - uebersprungen."
        echo "  sudo dnf install rpmlint"
    fi
fi
