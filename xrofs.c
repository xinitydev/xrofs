#include "xrofs.h"
#include <stdio.h>

static xrofs_fastint_t xrofs_bstrcmp(const uint8_t *s1, const uint8_t *s2)
{
    int8_t c1, c2;
    while ((c1 = *s1++) == (c2 = *s2++)) {
        if (c1 == '\0')
            return c1;
    }
    return c1 - c2;
}

static xrofs_fastint_t xrofs_search(const char *filename, xrofs_dev dev)
{
    xrofs_byte_t *searched = (xrofs_byte_t *)filename;
    xrofs_entry *entries = xrofs_entries(dev);
    xrofs_fastint_t res;
    xrofs_fastint_t left = 0;
    xrofs_fastint_t right = xrofs_numentries(dev) - 1;
    xrofs_fastint_t mid;

	do {
		mid = (left + right) >> 1;
        char *fname = xrofs_entry_fname(&entries[mid], dev);
		res = xrofs_bstrcmp(fname, searched);
		if (0 == res)
			return mid;
        if (res < 0)
            left = mid + 1;
        else
            right = mid - 1;
    } while (left <= right);
    return -1;
}

static void xrofs_bcopy(uint8_t *from, uint8_t *to, xrofs_fastuint_t nbytes)
{
	while (nbytes--) {
		*to++ = *from++;
	}
}

xrofs_file xrofs_open(xrofs_dev dev, const char *filename)
{
    xrofs_fastint_t idx = xrofs_search(filename, dev);
    if (idx < 0)
        return XROFS_FNULL;
    xrofs_entry *entries = xrofs_entries(dev);
    xrofs_file file = {
        .rdptr = xrofs_entry_fstart(&entries[idx], dev),
        .dev = dev,
        .nentry = idx
    };
    return file;
}

xrofs_fastint_t xrofs_lseek(xrofs_file *file, xrofs_fastint_t offset, int whence)
{
    if (NULL == file || XROFS_ISFNULL(*file))
        return -1;

    xrofs_entry entry = xrofs_entries(file->dev)[file->nentry];
    xrofs_byte_t *start = xrofs_entry_fstart(&entry, file->dev);
    xrofs_byte_t *end = (void *)start + entry.fsize - 1;
    xrofs_byte_t *res;
    switch (whence) {
    case SEEK_SET:
        res = (void *)start + offset;
        break;
    case SEEK_CUR:
        res = (void *)(file->rdptr) + offset;
        break;
    case SEEK_END:
        res = (void *)end + offset;
        break;
    default:
        return -1;
    }
    if (res < start || res > end)
        return -1;
    file->rdptr = res;
    return res - start;
}

xrofs_fastint_t xrofs_read(xrofs_file *file, void *buf, xrofs_fastuint_t nbyte)
{
	if (NULL == buf || NULL == file || XROFS_ISFNULL(*file))
		return -1;		// error, per POSIX

	xrofs_fastint_t toend = xrofs_toend(file);

	nbyte = nbyte > toend ? toend : nbyte;
	xrofs_bcopy(file->rdptr, buf, nbyte);
	file->rdptr += nbyte;
	return nbyte;
}

void *xrofs_map(xrofs_file *file, xrofs_fastuint_t *setsize)
{
	if (NULL == file || XROFS_ISFNULL(*file))
		return NULL;

	if (NULL != setsize)
		*setsize = xrofs_toend(file);

	return file->rdptr;
}
