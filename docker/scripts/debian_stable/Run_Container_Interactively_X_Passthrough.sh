#!/bin/bash

set -eu

# This script launches a Docker image that can pass-through X11 and OpenGL windows.
#
# It might fail! Especially if .Xauthority files are used.
#
# Idea re-implemented from http://fabiorehm.com/blog/2014/09/11/running-gui-apps-with-docker/ .
# See the comments on that page for ideas if this fails.
# Other options include VNC and ssh forwarding.
#
# See https://github.com/jessfraz/dockerfiles/issues/359 for info about the MIT-SHM (X_ShmPutImage) error fix.
#

# Create a script that can be run from inside the container.
#
# All arguments sent to the present script are forwarded to the internal script.
#
internal_run_script="$(mktemp)"
trap "{ rm '${internal_run_script}' ; }" EXIT  # Remove the temp file on exit.

# The calling user's username, which we will re-use inside the container.
uname=$(id -u -n)

cat > "${internal_run_script}" <<EOF
#!/bin/bash

# The following will enable OpenGL acceleration inside a docker container.
#
# Note: The following seems to NOT be needed for OpenGL with SFML.
#
#apt-get -y install firefox-esr
#apt-get -y install mesa-va-drivers mesa-vdpau-drivers va-driver-all vdpau-driver-all libva-drm2 libva-x11-2 libva2 libvdpau-va-gl1 libvdpau1 
#apt-get -y install va-driver-all vdpau-driver-all libva-drm2 libva-x11-2 libva2 libvdpau-va-gl1 libvdpau1 
apt-get -y update
apt-get -y install dcmtk dialog ncurses-term zenity

# Copy the portable_dcma binaries into the container.
rsync -r /root/portable_dcma/ /usr/bin/

# Remove the existing dcma binary to avoid confusion.
rm /usr/bin/dicomautomaton_dispatcher

# Add a user to use X.
#
# Note: Need to use a non-root user.
#
#useradd --system -g users -G video -m -s /bin/bash auser 2>/dev/null
#groupadd -f -o -g 1000 users
#useradd -g users -u 1000 -G video -m -s /bin/bash auser

printf 'export PS1="${uname}@container> "\n' >> /home/${uname}/.bashrc

# Pass-through any arguments sent to the run script (not this script, the script that created this script).
all_args='$@'

if [ -z "\$all_args" ] ; then 
    printf '### Entering interactive bash shell now... ###\n'
    runuser -u ${uname} -- /bin/bash -i

else # Pass the command through.
    #runuser -u ${uname} -- /home/${uname}/portable_dcma/portable_dcma /tmp/CT*dcm
    runuser -u ${uname} -- \$all_args

fi

EOF
chmod 777 "${internal_run_script}"


# Launch the container.
sudo \
  docker run \
    -it --rm \
    --network=host \
    `# Avoid a (potential) X error involving the MIT-SHM extension (X_ShmPutImage) by disabling IPC namespacing.` \
    --ipc=host \
    -e DISPLAY \
    \
    `# Map X-related host locations into the container. ` \
    `# -v /tmp/.X11-unix:/tmp/.X11-unix:rw ` \
    -v /tmp/:/tmp/:rw \
    -v "${internal_run_script}":/x11_launch_script.sh:ro \
    -v /dev/null:/dev/input/js0:ro \
    -v /dev/null:/dev/input/js1:ro \
    -v /dev/null:/dev/input/js2:ro \
    \
    `# Map all host users into the container. ` \
    -v /etc/passwd:/etc/passwd:ro \
    -v /etc/group:/etc/group:ro \
    -v /etc/shadow:/etc/shadow:ro \
    \
    `# Map the DCMA binary locations into the container. ` \
    -v /home/hal/portable_dcma/:/root/portable_dcma/:ro \
    -v /home/hal/portable_dcma/:/home/${uname}/portable_dcma/:ro \
    \
    `# Map various locations from host into the container. ` \
    -v /media/:/media/:ro \
    -v /media/sf_U_DRIVE/Profile/Desktop:/home/${uname}/Desktop/:ro \
    -v /mnt/scratch_A/Research_Archives:/home/${uname}/Research_Archives/:rw \
    -v /:/host_root/:ro \
    \
    -v "$(pwd)":/start/:rw \
    -w /start/ \
    \
    dcma_build_base_debian_stable:latest \
    \
    /x11_launch_script.sh

