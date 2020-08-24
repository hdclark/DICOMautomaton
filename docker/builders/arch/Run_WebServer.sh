#!/usr/bin/env bash

# This script invokes the DICOMautomaton Web Server. It is meant to be called inside a container or
# VM. Since headless SFML rendering depends on an OpenGL context, a dummy X instance that supports 
# GLX extensions is initiated.


# This is the document root. All static files served by the server must originate from this directory.
# Note that symlinks can be used to expose other files to the server without having to duplicate them.
export DCMAWS_DOC_ROOT="/etc/DICOMautomaton"


# This is the temporary cache disk space for client uploads. It does not need to exist; it will be 
# created if necessary.
export WT_TMP_DIR="/client_uploads"


# Ensure the libwt resources installed on the machine can be found by the server somewhere in the root
# directory.
#
# Note: the name of the directory Wt looks for to find these resources is controlled in the config
# file. The default is '$DOC_ROOT/resources'.
#
# Note: if root is not available, consider changing the document root to something owned by a non-
# root owner and then symlinking the desired (root user's) document root and resources into the 
# new document root.
if [ -d "$DCMAWS_DOC_ROOT"/resources ] ; then
    printf "Using existing Wt resources directory at '$DCMAWS_DOC_ROOT/resources'.\n"

elif [ -d /usr/share/Wt/resources ] ; then # Default on Arch Linux.
    ln -s /usr/share/Wt/resources "$DCMAWS_DOC_ROOT"/resources

elif [ -d /usr/local/share/Wt/resources ] ; then # Default on Debian.
    ln -s /usr/local/share/Wt/resources "$DCMAWS_DOC_ROOT"/resources
else
    printf "Cannot locate Wt resources. Query your package manager for the install location.\n" 1>&2 
    exit 1
fi

mkdir -p "${WT_TMP_DIR}" || {
    printf "Unable to create client file upload disk cache directory. Cannot continue.\n" 1>&2
    exit 1
}


rm -f '/tmp/.X1-lock' '/root/.Xauth' 2>/dev/null || true  # In case a lock exists from terminating the container.
/usr/bin/Xorg \
  -noreset \
  +extension GLX \
  +extension RANDR \
  +extension RENDER \
  -depth 24 \
  -logfile ./xdummy.log \
  -config /etc/X11/xorg.conf \
  :1 &
xpid=$!

sleep 5
export DISPLAY=:1


dicomautomaton_webserver \
  --config "${DCMAWS_DOC_ROOT}/"webserver.conf \
  --docroot="${DCMAWS_DOC_ROOT}/" \
  --http-address 0.0.0.0 \
  --http-port 80 

kill $xpid

