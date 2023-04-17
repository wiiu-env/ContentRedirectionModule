FROM ghcr.io/wiiu-env/devkitppc:20230417

COPY --from=ghcr.io/wiiu-env/libfunctionpatcher:20230417 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiumodulesystem:20230417 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libromfs_wiiu:20230417 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libcontentredirection:20230417 /artifacts $DEVKITPRO

WORKDIR project
