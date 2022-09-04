FROM wiiuenv/devkitppc:20220806

COPY --from=wiiuenv/libfunctionpatcher:20220904 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiumodulesystem:20220904 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libromfs_wiiu:20220904 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libcontentredirection:20220903 /artifacts $DEVKITPRO

WORKDIR project
