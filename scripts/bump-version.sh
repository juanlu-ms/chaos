#!/usr/bin/env bash
set -euo pipefail

BUMP="${1:-patch}"

case "$BUMP" in
  major|minor|patch) ;;
  *)
    echo "Usage: $0 [major|minor|patch]  (default: patch)" >&2
    exit 1
    ;;
esac

bump_semver() {
  local version="$1"
  local component="$2"
  local major minor patch

  IFS='.' read -r major minor patch <<< "$version"
  case "$component" in
    major)
      major=$((major + 1))
      minor=0
      patch=0
      ;;
    minor)
      minor=$((minor + 1))
      patch=0
      ;;
    patch)
      patch=$((patch + 1))
      ;;
  esac
  echo "$major.$minor.$patch"
}

bump_in_file() {
  local file="$1"
  local old="$2"
  local new="$3"
  sed -i "s/$old/$new/g" "$file"
}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

CURRENT=$(grep -oP 'VERSION \K[0-9]+\.[0-9]+\.[0-9]+' "$ROOT_DIR/CMakeLists.txt")
NEXT=$(bump_semver "$CURRENT" "$BUMP")

echo "Bumping backend: $CURRENT -> $NEXT ($BUMP)"

bump_in_file "$ROOT_DIR/CMakeLists.txt" "$CURRENT" "$NEXT"
bump_in_file "$ROOT_DIR/vcpkg.json" "$CURRENT" "$NEXT"

FRONTEND_CURRENT=$(grep -oP '"version": "\K[0-9]+\.[0-9]+\.[0-9]+(?=")' "$ROOT_DIR/frontend/package.json")
FRONTEND_NEXT=$(bump_semver "$FRONTEND_CURRENT" "$BUMP")

echo "Bumping frontend: $FRONTEND_CURRENT -> $FRONTEND_NEXT ($BUMP)"

bump_in_file "$ROOT_DIR/frontend/package.json" "$FRONTEND_CURRENT" "$FRONTEND_NEXT"

git -C "$ROOT_DIR" add \
  CMakeLists.txt \
  vcpkg.json \
  frontend/package.json

git -C "$ROOT_DIR" commit -m "Bump version to $NEXT"

git -C "$ROOT_DIR" tag -a "v$NEXT" -m "v$NEXT"

git -C "$ROOT_DIR" push --follow-tags

echo "Done. Committed, tagged v$NEXT, and pushed."
