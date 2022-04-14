FROM wiiuenv/devkitppc:20220303

COPY --from=wiiuenv/libfunctionpatcher:20220211 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiumodulesystem:20220204 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libromfs_wiiu:20220414 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libcontentredirection:20220414 /artifacts $DEVKITPRO

WORKDIR project