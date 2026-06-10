#!/bin/bash
set -e

# Parse current version from Cargo.toml
CURRENT_VERSION=$(grep '^version = ' Cargo.toml | head -n 1 | cut -d'"' -f2)

if [ -z "$CURRENT_VERSION" ]; then
    echo "Error: Could not read current version from Cargo.toml"
    exit 1
fi

# Show usage if no argument
if [ -z "$1" ]; then
    echo "Usage: $0 {major|minor|patch|<version>}"
    echo "Current version: $CURRENT_VERSION"
    exit 1
fi

# Parse major, minor, patch
IFS='.' read -r major minor patch <<< "$CURRENT_VERSION"

# Determine new version
case "$1" in
    major)
        NEW_VERSION="$((major + 1)).0.0"
        ;;
    minor)
        NEW_VERSION="${major}.$((minor + 1)).0"
        ;;
    patch)
        NEW_VERSION="${major}.${minor}.$((patch + 1))"
        ;;
    *)
        # Verify custom version format (X.Y.Z)
        if [[ ! "$1" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
            echo "Error: Version must be major, minor, patch, or in X.Y.Z format (got: $1)"
            exit 1
        fi
        NEW_VERSION="$1"
        ;;
esac

echo "Bumping version from $CURRENT_VERSION to $NEW_VERSION..."

# 1. Cargo.toml
perl -i -pe "s/^(version\s*=\s*\")[^\"]+/\${1}$NEW_VERSION/ if \$. < 10" Cargo.toml

# 2. meson.build
perl -i -pe "s/(version:\s*')[^']+(\s*,\s*\n\s*meson_version)/\${1}$NEW_VERSION\${2}/if \$. < 5" meson.build

# 3. mygestures.spec
perl -i -pe "s/^(Version:\s*)\S+/\${1}$NEW_VERSION/g" packaging/mygestures.spec
SPEC_DATE=$(date +"%a %b %d %Y")
# Prepend changelog entry in RPM spec
perl -i -pe "s/(\%changelog)/\${1}\n* $SPEC_DATE Lucas Augusto Deters <lucasdeters\@gmail.com> - $NEW_VERSION-1\n- New release $NEW_VERSION./g" packaging/mygestures.spec

# 4. APKBUILD
perl -i -pe "s/^(pkgver=)\S+/\${1}$NEW_VERSION/g" packaging/APKBUILD
perl -i -pe "s/^(pkgrel=)\S+/\${1}0/g" packaging/APKBUILD

# 5. src/bin/gestos.rs
perl -i -pe "s/(dialog\.set_version\(Some\(\")[^\"]+/\${1}$NEW_VERSION/g" src/bin/gestos.rs

# 6. debian/changelog
if ! grep -q "mygestures ($NEW_VERSION-1)" debian/changelog; then
    DATE_DEB=$(date -R)
    TEMP_CHANGELOG=$(mktemp)
    cat <<EOF > "$TEMP_CHANGELOG"
mygestures ($NEW_VERSION-1) stable; urgency=low

  * New release $NEW_VERSION.

 -- Lucas Augusto Deters <lucasdeters@gmail.com>  $DATE_DEB

EOF
    cat debian/changelog >> "$TEMP_CHANGELOG"
    mv "$TEMP_CHANGELOG" debian/changelog
fi

# Run cargo check to update Cargo.lock version
echo "Running cargo check to update Cargo.lock..."
cargo check >/dev/null 2>&1 || true

# Commit and tag
echo "Staging version changes in Git..."
git add Cargo.toml Cargo.lock meson.build packaging/mygestures.spec packaging/APKBUILD src/bin/gestos.rs debian/changelog

git commit -m "Bump version to $NEW_VERSION"
git tag "v$NEW_VERSION"

echo "------------------------------------------------------------"
echo "Successfully bumped version to $NEW_VERSION!"
echo "To publish, run:"
echo "  git push origin master --tags"
echo "------------------------------------------------------------"
