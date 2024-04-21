FROM ghcr.io/wiiu-env/devkitppc:20231112

COPY --from=ghcr.io/wiiu-env/libfunctionpatcher:20230621 /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/wiiumodulesystem:0.3.2-dev-20231203-2e5832b /artifacts $DEVKITPRO
COPY --from=ghcr.io/wiiu-env/libcontentredirection:1.1-dev-20240421-50d6722 /artifacts $DEVKITPRO

WORKDIR project
