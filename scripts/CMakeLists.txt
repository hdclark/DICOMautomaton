
# Install the script multiple times so at least one of them gets sourced properly.
#
# The script properly handles multiple binaries, but bash-completion will selectively source the script that matches an
# executable by name. So we have to spam the script to cover each executable name. Ideally all completion scripts would
# be sourced prospectively, but that would be unfrtunately slow.
#

install(PROGRAMS
    bash_completion.sh
  DESTINATION ${BASH_COMPLETION_INSTALL_DIR}
  RENAME dicomautomaton_dispatcher
)

install(PROGRAMS
    bash_completion.sh
  DESTINATION ${BASH_COMPLETION_INSTALL_DIR}
  RENAME portable_dcma
)

