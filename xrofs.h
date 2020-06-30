#include <stdint.h>
#include <stddef.h>

typedef uint8_t xrofs_byte_t;
typedef uint16_t xrofs_smalluint_t;
typedef int32_t xrofs_fastint_t;
typedef uint32_t xrofs_fastuint_t;
typedef uint64_t xrofs_uint64_t;

#if defined(XROFS_USE_ALWAYSINLINE) && XROFS_USE_ALWAYSINLINE
#define xrofs_inline inline __attribute__((always_inline))
#else
#define xrofs_inline inline
#endif

#pragma pack(push)
/**
 * struct xrofs_drive - represents xrofs disk mount point
 * @magic: some number used for backward-compatability, checksums, etc
 * @entnum: number of entries in table
 */
#pragma pack(1)
struct xrofs_drive {
    xrofs_smalluint_t magic;
    xrofs_smalluint_t entnum;
};


/**
 * typedef xrofs_entry - contains file table information
 * @fsize: size of the actual file data (in bytes)
 * @foffset: offset of the node in filesystem.
 *           This gets added to the fs base address resulting in effective ptr to node
 *
 * Nodes have form of (data_bytes, filename_string). We get to filename_string since we
 * know the file size. This allows to save save space in entry table
 */
#pragma pack(1)
typedef struct xrofs_entry {
    xrofs_uint64_t fsize   : 24;
    xrofs_uint64_t foffset : 32;
} xrofs_entry;


/**
 * typedef xrofs_file - file descriptor-like object
 * @rdptr: current read position address
 * @nentry: entry number
 */
#pragma pack(1)
typedef struct xrofs_file_s {
    xrofs_byte_t *rdptr;
    struct xrofs_drive *dev;
    xrofs_smalluint_t nentry;   
} xrofs_file;

#pragma pack(pop)


/**
 * typedef xrofs_dev - pointer to xrofs disk mount point
 * This is primarily intended to be assigned to some address
 * where the disk image begins
 */
typedef struct xrofs_drive *xrofs_dev;


xrofs_inline xrofs_fastint_t xrofs_numentries(xrofs_dev dev)
{
    return dev->entnum;
}

xrofs_inline xrofs_fastint_t xrofs_magic(xrofs_dev dev)
{
    return dev->magic;
}

xrofs_inline xrofs_entry *xrofs_entries(xrofs_dev dev)
{
    return (void *)dev + sizeof(struct xrofs_drive);
}

xrofs_inline xrofs_byte_t *xrofs_entry_fstart(xrofs_entry *entry, xrofs_dev dev)
{
    xrofs_entry e = *entry;
    return (void *)dev + e.foffset;
}

xrofs_inline char *xrofs_entry_fname(xrofs_entry *entry, xrofs_dev dev)
{
    xrofs_entry e = *entry;
    return (void *)xrofs_entry_fstart(entry, dev) + e.fsize;
}

xrofs_inline xrofs_fastint_t xrofs_toend(xrofs_file *file)
{
    xrofs_entry *entries = xrofs_entries(file->dev);

	return ((void *)xrofs_entry_fname(&(entries[file->nentry]), file->dev) - (void *)file->rdptr);
}

#define XROFS_FNULL ((xrofs_file){0})
#define XROFS_ISFNULL(file) ((file).dev == XROFS_FNULL.dev)

xrofs_inline void xrofs_close(xrofs_file *file)
{
    *file = XROFS_FNULL;
}

xrofs_file xrofs_open(xrofs_dev dev, const char *filename);

xrofs_fastint_t xrofs_lseek(xrofs_file *file, xrofs_fastint_t offset, int whence);

xrofs_fastint_t xrofs_read(xrofs_file *file, void *buf, xrofs_fastuint_t nbyte);

void *xrofs_map(xrofs_file *file, xrofs_fastuint_t *setsize);
