#!/usr/bin/env bash
#
#  TestMe cleanup script
#

#
#   -r because a test that aborts on a failed assertion can leave a fixture directory behind,
#   and "rm -f" on a directory fails - which would then break file.tst.c's directory counts.
#
rm -rf data/temp-* data/sendfile-*