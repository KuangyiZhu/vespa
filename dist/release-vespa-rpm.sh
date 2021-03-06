#!/bin/bash
# Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
set -e

if [ $# -ne 1 ]; then
  echo "Usage: $0 <version>"
  exit 1
fi

readonly VERSION=$1
readonly SPECFILE="dist/vespa.spec"
readonly RPM_BRANCH="rpmbuild"
readonly CURRENT_BRANCH=$(git branch | grep "^\*" | cut -d' ' -f2)

# Make sure we are up to date
git checkout master
git pull --rebase

# Delete existing branch if exists and create new one
git push origin :$RPM_BRANCH &> /dev/null || true
git branch -D $RPM_BRANCH &> /dev/null || true 
git checkout -b $RPM_BRANCH $VERSION

# Tito expects spec file to be on root
git mv $SPECFILE .

# Hide pom.xml to avoid tito doing anything to our pom.xml files
mv pom.xml pom.xml.hide

# Run tito to update spec file and tag
tito init
tito tag --use-version=$VERSION --no-auto-changelog

# Push changes and tag to branc
git push -u origin --follow-tags $RPM_BRANCH

# Trig the build on Copr
curl -X POST \
     -H "Content-type: application/json" \
     -d '{ "ref_type": "tag", "repository": { "clone_url": "https://github.com/vespa-engine/vespa.git" } }' \
     https://copr.fedorainfracloud.org/webhooks/github/8037/d1dd5867-b493-4647-a888-0c887e6087b3/

git reset --hard HEAD
git checkout $CURRENT_BRANCH

