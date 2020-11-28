#!/usr/bin/env bash

set -eux
set -o pipefail

for paranoia_level in low medium high ; do

    "${DCMA_BIN}" \
      "${TEST_FILES_ROOT}"/3x3x3_random_positive.3ddose \
      \
      -o DICOMExportImagesAsDose \
         -p Filename='test.dcm' \
         -p ImageSelection=first \
         -p ParanoiaLevel="${paranoia_level}" |
      tee -a fullstdout


    "${DCMA_BIN}" \
      'test.dcm' \
      \
      -o DroverDebug |
      tee -a fullstdout |
      grep -i "1 Image_Arrays loaded" | 
      `# Ensure the output stream is not empty. ` \
      grep .

    rm 'test.dcm'

done

