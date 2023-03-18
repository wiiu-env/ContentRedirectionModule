FROM ghcr.io/wiiu-env/devkitppc:20221228

COPY --from=ghcr.io/wiiu-env/libfunctionpatcher:20230106 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiumodulesystem:20230106 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libromfs_wiiu:20220904 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libcontentredirection:20221010 /artifacts $DEVKITPRO

WORKDIR project
