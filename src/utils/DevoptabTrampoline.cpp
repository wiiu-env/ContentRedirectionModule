
#include "logger.h"


#include <content_redirection/defines.h>

#include <sys/iosupport.h>

#include <array>
#include <coreinit/debug.h>
#include <mutex>
#include <string.h>

namespace DevoptabTrampoline {
    constexpr int MAX_REDIRECTION_DEVICES = STD_MAX - 3;

    struct RedirectionDeviceSlot {
        bool inUse                         = false;
        devoptab_t hostDevoptab            = {};
        ContentRedirectionDeviceABI device = {};
    };

    std::array<RedirectionDeviceSlot, MAX_REDIRECTION_DEVICES> sDevices;
    std::recursive_mutex sDevicesMutex;


    inline void convert(struct stat *dst, const CR_Stat *src) {
        if (!dst || !src) return;
        dst->st_dev     = src->dev;
        dst->st_ino     = src->ino;
        dst->st_mode    = src->mode;
        dst->st_nlink   = src->nlink;
        dst->st_uid     = src->uid;
        dst->st_gid     = src->gid;
        dst->st_rdev    = src->rdev;
        dst->st_size    = (off_t) src->size;
        dst->st_atime   = (time_t) src->atime;
        dst->st_mtime   = (time_t) src->mtime;
        dst->st_ctime   = (time_t) src->ctime;
        dst->st_blksize = src->blksize;
        dst->st_blocks  = src->blocks;
    }

    inline void convert(struct statvfs *dst, const CR_Statvfs *src) {
        if (!dst || !src) return;
        dst->f_bsize   = src->bsize;
        dst->f_frsize  = src->frsize;
        dst->f_blocks  = src->blocks;
        dst->f_bfree   = src->bfree;
        dst->f_bavail  = src->bavail;
        dst->f_files   = src->files;
        dst->f_ffree   = src->ffree;
        dst->f_favail  = src->favail;
        dst->f_fsid    = src->fsid;
        dst->f_flag    = src->flag;
        dst->f_namemax = src->namemax;
    }

    static int open_r(_reent *r, void *fileStruct, const char *path, int flags, int mode) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.open) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.open(slot->device.deviceData, fileStruct, path, flags, (uint32_t) mode);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int close_r(_reent *r, void *fd) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.close) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.close(slot->device.deviceData, fd);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static ssize_t write_r(_reent *r, void *fd, const char *ptr, size_t len) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.write) {
            r->_errno = ENOSYS;
            return -1;
        }

        const ssize_t res = slot->device.write(slot->device.deviceData, fd, ptr, len);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static ssize_t read_r(_reent *r, void *fd, char *ptr, size_t len) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.read) {
            r->_errno = ENOSYS;
            return -1;
        }

        const ssize_t res = slot->device.read(slot->device.deviceData, fd, ptr, len);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static off_t seek_r(_reent *r, void *fd, off_t pos, int dir) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.seek) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int64_t res = slot->device.seek(slot->device.deviceData, fd, (int64_t) pos, dir);

        if (res < 0) {
            r->_errno = -(int) res;
            return -1;
        }
        return (off_t) res;
    }

    static int fstat_r(_reent *r, void *fd, struct stat *st) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.fstat) {
            r->_errno = ENOSYS;
            return -1;
        }

        CR_Stat abstract_st{};
        const int res = slot->device.fstat(slot->device.deviceData, fd, &abstract_st);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        convert(st, &abstract_st);
        return res;
    }

    static int stat_r(_reent *r, const char *file, struct stat *st) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.stat) {
            r->_errno = ENOSYS;
            return -1;
        }

        CR_Stat abstract_st{};
        const int res = slot->device.stat(slot->device.deviceData, file, &abstract_st);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        convert(st, &abstract_st);
        return res;
    }

    static int link_r(_reent *r, const char *existing, const char *newLink) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.link) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.link(slot->device.deviceData, existing, newLink);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int unlink_r(_reent *r, const char *name) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.unlink) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.unlink(slot->device.deviceData, name);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int chdir_r(_reent *r, const char *name) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.chdir) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.chdir(slot->device.deviceData, name);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int rename_r(_reent *r, const char *oldName, const char *newName) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.rename) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.rename(slot->device.deviceData, oldName, newName);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int mkdir_r(_reent *r, const char *path, int mode) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.mkdir) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.mkdir(slot->device.deviceData, path, (uint32_t) mode);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static DIR_ITER *diropen_r(_reent *r, DIR_ITER *dirState, const char *path) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.diropen) {
            r->_errno = ENOSYS;
            return nullptr;
        }

        const int res = slot->device.diropen(slot->device.deviceData, dirState->dirStruct, path);

        if (res < 0) {
            r->_errno = -res;
            return nullptr;
        }
        return dirState;
    }

    static int dirreset_r(_reent *r, DIR_ITER *dirState) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.dirreset) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.dirreset(slot->device.deviceData, dirState->dirStruct);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int dirnext_r(_reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.dirnext) {
            r->_errno = ENOSYS;
            return -1;
        }

        CR_Stat abstract_st{};
        const int res = slot->device.dirnext(slot->device.deviceData, dirState->dirStruct, filename, &abstract_st);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        convert(filestat, &abstract_st);
        return res;
    }

    static int dirclose_r(_reent *r, DIR_ITER *dirState) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.dirclose) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.dirclose(slot->device.deviceData, dirState->dirStruct);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int statvfs_r(_reent *r, const char *path, struct statvfs *buf) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.statvfs) {
            r->_errno = ENOSYS;
            return -1;
        }

        CR_Statvfs abstract_buf{};
        const int res = slot->device.statvfs(slot->device.deviceData, path, &abstract_buf);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        convert(buf, &abstract_buf);
        return res;
    }

    static int ftruncate_r(_reent *r, void *fd, off_t len) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.ftruncate) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.ftruncate(slot->device.deviceData, fd, (int64_t) len);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int fsync_r(_reent *r, void *fd) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.fsync) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.fsync(slot->device.deviceData, fd);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int chmod_r(_reent *r, const char *path, mode_t mode) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.chmod) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.chmod(slot->device.deviceData, path, (uint32_t) mode);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int fchmod_r(_reent *r, void *fd, mode_t mode) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.fchmod) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.fchmod(slot->device.deviceData, fd, (uint32_t) mode);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int rmdir_r(_reent *r, const char *name) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.rmdir) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.rmdir(slot->device.deviceData, name);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static int lstat_r(_reent *r, const char *file, struct stat *st) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.lstat) {
            r->_errno = ENOSYS;
            return -1;
        }

        CR_Stat abstract_st{};
        const int res = slot->device.lstat(slot->device.deviceData, file, &abstract_st);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        convert(st, &abstract_st);
        return res;
    }

    static int utimes_r(_reent *r, const char *filename, const struct timeval times[2]) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.utimes) {
            r->_errno = ENOSYS;
            return -1;
        }

        int res;
        if (times) {
            CR_Timeval cr_times[2];
            cr_times[0].tv_sec  = times[0].tv_sec;
            cr_times[0].tv_usec = times[0].tv_usec;
            cr_times[1].tv_sec  = times[1].tv_sec;
            cr_times[1].tv_usec = times[1].tv_usec;
            res                 = slot->device.utimes(slot->device.deviceData, filename, cr_times);
        } else {
            res = slot->device.utimes(slot->device.deviceData, filename, nullptr);
        }

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static long fpathconf_r(_reent *r, void *fd, int name) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.fpathconf) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int64_t res = slot->device.fpathconf(slot->device.deviceData, fd, name);

        if (res < 0) {
            r->_errno = -(int) res;
            return -1;
        }
        return (long) res;
    }

    static long pathconf_r(_reent *r, const char *path, int name) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.pathconf) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int64_t res = slot->device.pathconf(slot->device.deviceData, path, name);

        if (res < 0) {
            r->_errno = -(int) res;
            return -1;
        }
        return (long) res;
    }

    static int symlink_r(_reent *r, const char *target, const char *linkpath) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.symlink) {
            r->_errno = ENOSYS;
            return -1;
        }

        const int res = slot->device.symlink(slot->device.deviceData, target, linkpath);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static ssize_t readlink_r(_reent *r, const char *path, char *buf, size_t bufsiz) {
        const auto *slot = static_cast<RedirectionDeviceSlot *>(r->deviceData);
        if (!slot || !slot->device.readlink) {
            r->_errno = ENOSYS;
            return -1;
        }

        const ssize_t res = slot->device.readlink(slot->device.deviceData, path, buf, bufsiz);

        if (res < 0) {
            r->_errno = -res;
            return -1;
        }
        return res;
    }

    static void assign(devoptab_t *dev) {
        dev->open_r      = open_r;
        dev->close_r     = close_r;
        dev->write_r     = write_r;
        dev->read_r      = read_r;
        dev->seek_r      = seek_r;
        dev->fstat_r     = fstat_r;
        dev->stat_r      = stat_r;
        dev->link_r      = link_r;
        dev->unlink_r    = unlink_r;
        dev->chdir_r     = chdir_r;
        dev->rename_r    = rename_r;
        dev->mkdir_r     = mkdir_r;
        dev->diropen_r   = diropen_r;
        dev->dirreset_r  = dirreset_r;
        dev->dirnext_r   = dirnext_r;
        dev->dirclose_r  = dirclose_r;
        dev->statvfs_r   = statvfs_r;
        dev->ftruncate_r = ftruncate_r;
        dev->fsync_r     = fsync_r;
        dev->chmod_r     = chmod_r;
        dev->fchmod_r    = fchmod_r;
        dev->rmdir_r     = rmdir_r;
        dev->lstat_r     = lstat_r;
        dev->utimes_r    = utimes_r;
        dev->fpathconf_r = fpathconf_r;
        dev->pathconf_r  = pathconf_r;
        dev->symlink_r   = symlink_r;
        dev->readlink_r  = readlink_r;
    }

    devoptab_t *CreateDevoptab(const ContentRedirectionDeviceABI *device) {
        if (!device ||
            device->magic != CONTENT_REDIRECTION_DEVICE_MAGIC ||
            device->version != CONTENT_REDIRECTION_DEVICE_VERSION) {
            return nullptr;
        }

        std::lock_guard lock(sDevicesMutex);

        int slot = -1;
        for (int i = 0; i < MAX_REDIRECTION_DEVICES; i++) {
            if (!sDevices[i].inUse) {
                slot              = i;
                sDevices[i].inUse = true;
                break;
            }
        }

        if (slot == -1) {
            return nullptr;
        }

        sDevices[slot].device = *device;

        devoptab_t *host_dev = &sDevices[slot].hostDevoptab;
        *host_dev            = {};

        host_dev->name = strdup(device->name);
        if (!host_dev->name) {
            sDevices[slot].inUse = false;
            return nullptr;
        }

        host_dev->structSize   = device->structSize;
        host_dev->dirStateSize = device->dirStateSize;

        host_dev->deviceData = &sDevices[slot];

        assign(host_dev);

        return host_dev;
    }

    void ClearDevoptab(const devoptab_t *devoptab) {
        if (!devoptab) {
            return;
        }

        std::lock_guard lock(sDevicesMutex);

        for (size_t i = 0; i < MAX_REDIRECTION_DEVICES; i++) {
            if (sDevices[i].inUse && &sDevices[i].hostDevoptab == devoptab) {
                if (sDevices[i].hostDevoptab.name) {
                    free((void *) sDevices[i].hostDevoptab.name);
                }

                sDevices[i].inUse = false;

                sDevices[i].hostDevoptab = {};
                sDevices[i].device       = {};
                break;
            }
        }
    }

    bool RemoveDevoptab(const char *deviceName, int *resultOut) {
        if (!deviceName || !resultOut) {
            return false;
        }

        DEBUG_FUNCTION_LINE_INFO("Trying to remove devoptab \"%s\"", deviceName);

        const char *separator = strchr(deviceName, ':');
        size_t requested_len  = (separator != nullptr) ? (separator - deviceName) : strlen(deviceName);

        std::lock_guard lock(sDevicesMutex);
        for (size_t i = 0; i < MAX_REDIRECTION_DEVICES; i++) {
            if (sDevices[i].inUse &&
                sDevices[i].hostDevoptab.name) {

                size_t namelen = strlen(sDevices[i].hostDevoptab.name);

                if (requested_len == namelen) {
                    if (strncmp(sDevices[i].hostDevoptab.name, deviceName, requested_len) == 0) {
                        char device_with_colon[64] = {};

                        strncpy(device_with_colon, deviceName, sizeof(device_with_colon) - 2);
                        strncat(device_with_colon, ":", sizeof(device_with_colon) - strlen(device_with_colon) - 1);

                        // Remove Device first because clearing will make it invalid instead.
                        *resultOut = ::RemoveDevice(device_with_colon);

                        ClearDevoptab(&sDevices[i].hostDevoptab);

                        return true;
                    }
                }
            }
        }

        DEBUG_FUNCTION_LINE_WARN("Removing devoptab \"%s\" failed. Not found", deviceName);

        return false;
    }
} // namespace DevoptabTrampoline