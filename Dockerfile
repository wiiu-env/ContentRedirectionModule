FROM ghcr.io/wiiu-env/devkitppc:20241128

COPY --from=ghcr.io/wiiu-env/libfunctionpatcher:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiumodulesystem:20240424 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libcontentredirection:1.2-dev-20250207-b11c3c8 /artifacts $DEVKITPRO

WORKDIR project
