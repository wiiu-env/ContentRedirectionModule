FROM wiiuenv/devkitppc:20221228

COPY --from=wiiuenv/libfunctionpatcher:20220904 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiumodulesystem:20230106 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libromfs_wiiu:20220904 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libcontentredirection:20221010 /artifacts $DEVKITPRO

WORKDIR project
