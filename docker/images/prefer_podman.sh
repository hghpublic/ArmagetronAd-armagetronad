# prefer podman over docker, works rootless
if podman -h > /dev/null; then
    alias docker=podman
fi

