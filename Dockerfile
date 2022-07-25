FROM wiiuenv/devkitppc:20220724

COPY --from=wiiuenv/libfunctionpatcher:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/wiiumodulesystem:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libromfs_wiiu:20220724 /artifacts $DEVKITPRO
COPY --from=wiiuenv/libcontentredirection:20220724 /artifacts $DEVKITPRO

WORKDIR project
