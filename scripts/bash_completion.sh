#!/bin/bash

# This script is used to inject tab completion into bash for DICOMautomaton.
#
# Source this file to enable tab completion:
#  $> source $0
# or
#  $> . $0
#
# It can also be installed to /usr/share/bash-completion/completions/ (or your distribution's equivalent).
# 
# See 'https://debian-administration.org/article/317/An_introduction_to_bash_completion_part_2' (accessed 20190130) for
# a guide.
#
# Written by Hal Clark, 2019.
#

_dicomautomaton_dispatcher () {
  local cur # The current (incomplete) completion 'word.'
  local prev # The previous 'word.'
  cur="${COMP_WORDS[COMP_CWORD]}"
  prev="${COMP_WORDS[COMP_CWORD-1]}"
  dcma="${COMP_WORDS[0]}" # Can be '{,/path/to/}dicomautomaton_dispatcher' or '{,/path/to/}portable_dcma'.
  dcma="${dcma/#~/$HOME}" # Iff the first char is a '~', expand it to the user's home directory

  COMPREPLY=() # Possible completions passed back.

  case "${cur}" in
    # List all available options.
    -*)
        COMPREPLY=( $( compgen -W '-h -u -l -d -f -n -s -v -m -o -x -p -z \
                                   --help --detailed-usage --lexicon --database-parameters \
                                   --filter-query-file --next-group --standalone \
                                   --virtual-data --metadata --operation --disregard \
                                   --parameter --ignore ' -- "${cur}" ) )
        return 0
    ;;

    # Mid-way through specifying an operation using 'Opname:ABC=XYZ' notation.
    # Note that the ':' is considered a token by itself.
    # Provide a list of all options available to the operation.
    :*)
        # Determine which operation was most recently specified.
        most_recent_opname=""
        for i in `seq $COMP_CWORD -1 1` ; do
            local p="${COMP_WORDS[$(($i-1))]}"  # 'prev'
            local c="${COMP_WORDS[$i]}"         # 'curr'
            if [ "${p}" == '-o' ] || 
               [ "${p}" == '--operation' ] ||
               [ "${p}" == '-x' ] ||
               [ "${p}" == '--disregard' ] ; then
                most_recent_opname="${c}"
                break
            fi
            #printf '\n    "%s" and "%s"\n' "$c" "$p"
        done
        if [ -z "${most_recent_opname}" ] ; then
            return 1
        fi
        #printf '\n\nFound operation "%s".\n\n' "${most_recent_opname}"

        # Isolate the subset of documentation for the operation.
        local parameters=$( ${dcma} -u | 
                              sed -n '/# Operations/,/# Known Issues and Limitations/p' |
                              sed -n "/^## ${most_recent_opname}/,/^## /p" |
                              sed -n '/^### Parameters/,/^### /p' |
                              grep '^#### ' |
                              sed -e 's/^#### //' )

        #COMPREPLY=( $( compgen -W "${parameters}" -- "${cur}" ) )
        #COMPREPLY=( $( compgen -W "${parameters}" -- "${cur#:}" ) )
        # Do not bother filtering the output since this will only match on the ':' character.
        COMPREPLY=( $( compgen -W "${parameters}" ) )
        return 0
    ;;
  esac


  # List possibilities for specific options.
  case "${prev}" in

    # SQL filter files.
    -f | --filter-query-file)
        COMPREPLY=( $( compgen -f -X '!*sql' -- "${cur}" ) )
        return 0
    ;;

    # Stand-alone files (e.g., DICOM files).
    -s | --standalone)
        COMPREPLY=( $( compgen -f -- "${cur}" ) )
        return 0
    ;;

    # Operation names.
    -o | --operation | -x | --disregard)
        # Extract a list of all supported operations.
        local operations=$( ${dcma} -u | 
                              sed -n '/# Operations/,/# Known Issues and Limitations/p' |
                              grep '^## ' |
                              sed -e 's/^## //' )
        COMPREPLY=( $( compgen -W "${operations}" -- "${cur}" ) )
        return 0
    ;;

    # Parameter names.
    -p | --parameter | -z | --ignore)
        # Extract a list of the supported parameters for the previously specified operation.
        local operations=$( ${dcma} -u | 
                              sed -n '/# Operations/,/# Known Issues and Limitations/p' |
                              grep '^## ' |
                              sed -e 's/^## //' )
        COMPREPLY=( $( compgen -W "${operations}" -- "${cur}" ) )
        return 0
    ;;

    # If nothing else applies, the user can usually specify a filename...
    *)
        COMPREPLY=( $( compgen -f -- "${cur}" ) )
        return 0
    ;;

  esac

  #printf -- "${COMPREPLY[@]}\n"

  return 0
}

#complete -F _dicomautomaton_dispatcher -o filenames dicomautomaton_dispatcher
complete -F _dicomautomaton_dispatcher dicomautomaton_dispatcher
complete -F _dicomautomaton_dispatcher portable_dcma

