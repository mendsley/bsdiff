#!/bin/sh

touch AUTHORS NEWS README ChangeLog
cp LICENSE COPYING

autoreconf -fis
