#!/bin/sh

pandoc reference_guide.md --from=markdown+raw_tex --to=man --self-contained --output="reference_guide.man"
man -l "reference_guide.man"

