#!/bin/bash
# Copyright 2017 Yahoo Holdings. Licensed under the terms of the Apache 2.0 license. See LICENSE in the project root.
set -e

SOURCE_DIR=/source
NUM_THREADS=4

cd "${SOURCE_DIR}"
source /opt/rh/devtoolset-6/enable || true
export MAVEN_OPTS="-Xms128m -Xmx512m"
sh ./bootstrap.sh java
mvn install -nsu -B -T ${NUM_THREADS} -V # Should ideally split out test phase, but some unit tests fails on 'mvn test'
