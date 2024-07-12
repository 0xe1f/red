#!/bin/bash

echo "Building fbneo" >&2

cd fbneo && make ss RELEASEBUILD=1 BUILD_NATIVE=1
