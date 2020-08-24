#!/usr/bin/env bash

# This script compiles pdf and html documents from Markdown source files.
# Note that the Markdown sources are NOT automatically updated prior to compilation.
#
# Written by hal clark in 2019.

set -e

for i in ./reference_guide*.md ; do 

    file_in="$i"

    filename_in="$(basename "${file_in}")"
    pdf_file_out="./${filename_in%.*}.pdf"
    htm_file_out="./${filename_in%.*}.html"
    man_file_out="./${filename_in%.*}.man"

    # Build the compiler invocations.
    pdf_compiler_inv=()
    pdf_compiler_inv+=(pandoc)
    pdf_compiler_inv+=("${file_in}")
    pdf_compiler_inv+=(--from=markdown+raw_tex)
    #pdf_compiler_inv+=(--to=pdf)
    pdf_compiler_inv+=(--resource-path=./images/)
    pdf_compiler_inv+=(--toc)
    pdf_compiler_inv+=(--number-sections)
    pdf_compiler_inv+=(--pdf-engine=pdflatex)
    pdf_compiler_inv+=(--output="${pdf_file_out}")

    htm_compiler_inv=()
    htm_compiler_inv+=(pandoc)
    htm_compiler_inv+=("${file_in}")
    htm_compiler_inv+=(--from=markdown+raw_tex)
    htm_compiler_inv+=(--to=html5)
    #htm_compiler_inv+=(--standalone)
    htm_compiler_inv+=(--self-contained)
    htm_compiler_inv+=(--toc)
    #htm_compiler_inv+=(--variable=toc-title:\"Table of Contents\") # Note: cannot specify this way...
    htm_compiler_inv+=(--toc-depth=2)
    htm_compiler_inv+=(--number-sections)
    htm_compiler_inv+=(--output="${htm_file_out}")

    man_compiler_inv=()
    man_compiler_inv+=(pandoc)
    man_compiler_inv+=("${file_in}")
    man_compiler_inv+=(--from=markdown+raw_tex)
    man_compiler_inv+=(--to=man)
    #man_compiler_inv+=(--standalone)
    man_compiler_inv+=(--self-contained)
    man_compiler_inv+=(--output="${man_file_out}")

    if false ; then
        exit
    elif command -v pandoc >/dev/null 2>&1 ; then
        # Invoke with the system's pandoc.
        printf "Compiling '%s' with system pandoc.\n" "$i"
        "${pdf_compiler_inv[@]}"
        "${htm_compiler_inv[@]}"
        "${man_compiler_inv[@]}"
    elif [ -f ~/Appliances/docutools/Run_Container.sh ] ; then
        # If the docutools docker container is available, use it.
        printf "Compiling '%s' with docutools Docker container pandoc.\n" "$i"
        ~/Appliances/docutools/Run_Container.sh "${pdf_compiler_inv[@]}"
        ~/Appliances/docutools/Run_Container.sh "${htm_compiler_inv[@]}"
        ~/Appliances/docutools/Run_Container.sh "${man_compiler_inv[@]}"

        sudo chown $(id -un):$(id -gn) "${pdf_file_out}" 
        sudo chown $(id -un):$(id -gn) "${htm_file_out}" 
        sudo chown $(id -un):$(id -gn) "${man_file_out}" 
    else
        printf "Cannot find a way to run pandoc. Cannot continue.\n"
        exit 1
    fi
done

