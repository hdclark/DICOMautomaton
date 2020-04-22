#!/bin/bash

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

set -eu

if [ "$EUID" -eq 0 ] ; then
    printf "This script should not be run as root. Refusing to continue.\n" 1>&2
    exit 1
fi

portable_bins_dir="$HOME/portable_dcma/"
if [ ! -d "$portable_bins_dir" ] ; then
    printf "Unable to access portable binaries directory. Refusing to continue.\n" 1>&2
    exit 1
fi

# Create a simple 'init' script that can be run from inside the container.
#
# All arguments sent to the present script are forwarded to this internal script.
#
internal_run_script="$(mktemp)"
trap "{ rm '${internal_run_script}' ; }" EXIT  # Remove the temp file on exit.

# The calling user's username on the host, which we will re-use inside the container.
uname=$(id -u -n)
gname=$(id -g -n)
uid=$(id -u)
gid=$(id -g)

cat > "${internal_run_script}" <<EOF
#!/bin/bash

# Copy host files to facilitate easier interoperation.
#cp /etc/passwd_host /etc/passwd
#cp /etc/group_host  /etc/group
#cp /etc/shadow_host /etc/shadow

# Create a user with the same credentials as the host user.
groupadd -g ${gid} -f ${gname} || true
useradd -g ${gname} -u ${uid} -G video -m -d '/home/container_${uname}' -s /bin/bash ${uname}

# Merge group files so that any necessary groups (e.g., filesystem access) carry over.
cp /etc/group /etc/group_container
awk -F: -vOFS=':' '{ 
  if( !(\$1 in gname || \$3 in gid) ){
      print \$1,"x",\$3,\$4
  };
  gname[\$1]=1;
  gid[\$3]=1;
}' /etc/group_container /etc/group_host > /etc/group

# Copy host Xauth file.
cp /etc/Xauthority_host /home/container_${uname}/.Xauthority || true
chown ${uname}:${gname} /home/container_${uname}/.Xauthority || true

# Install some convenience packages.
apt-get -y update
apt-get -y install vim dcmtk dialog ncurses-term zenity gedit patchelf meld

# Copy the portable_dcma binaries into the container.
rsync -r /root/portable_dcma/ /usr/bin/

# Remove any existing dcma binaries, to avoid confusion.
rm /usr/bin/dicomautomaton_dispatcher || true

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
    -v "$HOME"/.Xauthority:/etc/Xauthority_host:ro \
    -v /dev/null:/dev/input/js0:ro \
    -v /dev/null:/dev/input/js1:ro \
    -v /dev/null:/dev/input/js2:ro \
    -v "${internal_run_script}":/x11_launch_script.sh:ro \
    \
    `# Map the DCMA binary locations into the container prospectively. ` \
    -v "$HOME"/portable_dcma/:/root/portable_dcma/:ro \
    -v "$HOME"/portable_dcma/:/home/${uname}/portable_dcma/:ro \
    -v "$HOME"/portable_dcma/:/home/container_${uname}/portable_dcma/:ro \
    \
    `# Map various locations from host into the container. ` \
    -v /etc/group:/etc/group_host:ro \
    -v /:/host_root/:ro \
    -v /media/:/media/:ro \
    -v "$(pwd)":/start/:rw \
    -w /start/ \
    \
    -v /media/sf_U_DRIVE/Profile/Desktop:/home/${uname}/Desktop/:ro \
    -v /mnt/scratch_A/Research_Archives:/home/${uname}/Research_Archives/:rw \
    \
    dcma_build_base_debian_stable:latest \
    \
    /x11_launch_script.sh

#    debian:stable \
#    -v /etc/passwd:/etc/passwd_host:ro \
#    -v /etc/group:/etc/group_host:ro \
#    -v /etc/shadow:/etc/shadow_host:ro \
