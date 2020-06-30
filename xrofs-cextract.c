#include "xrofs.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <argp.h>
#include <libgen.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define err_handler(label, msg) {perror(msg); goto label;}

const char *argp_program_version = "xrofs-cextract";
const char *argp_program_bug_address = "Telegram : thodnev";
static char argp_doc[] = "Tool for extracting xrofs image contents";

static struct argp_option argp_options[] = {
    {"list", 'l', 0, 0, "List xrofs image contents and exit"},
	{"all", 'a', 0, 0, "Extracts every file if set"},
	{"img", 'i', "IMGFILE", 0, "Xrofs image file"},
	{"dir", 'd', "OUTDIR", OPTION_ARG_OPTIONAL, "Output dir where to place extracted files"},
	{"quiet", 'q', 0, 0, "Don't produce verbose output"},
	{ 0 }
};

struct arguments {
    char *imgname;
	char *dirname;
	char **filenames;
	int filenames_cnt;
    bool isverbose;
	bool isall;
	bool islist;
};


static error_t argp_opthandle(int key, char *arg, struct argp_state *state)
{
	/* Get arguments through passed-in value */
	struct arguments *args = state->input;

	switch (key) {
    case 'l':		// list image contents and exit
    	args->islist = true;
    	break;
	case 'a':		// extract every file from image
		args->isall = true;
		break;
    case 'q':		// no verbose output
    	args->isverbose = false;
    	break;
	case 'i':		// image file name
		if (NULL == arg || *arg == '\0')
			argp_error(state, "No xrofs image filename provided");
		args->imgname = arg;
		break;
	case 'd':		// output directory for extraction
		if (NULL == arg || *arg == '\0')
			argp_error(state, "No output directory name provided");
		args->dirname = arg;
		break;
	case ARGP_KEY_NO_ARGS:
		if (!args->isall && !args->islist)
			argp_error(state, "Either --list, --all or FILEs to extract must be provided");
	case ARGP_KEY_ARGS:		// consume all arguments left
		args->filenames_cnt = state->argc - state->next;
		args->filenames = state->argv + state->next;
		break;
    default:
      return ARGP_ERR_UNKNOWN;
    }

  	return 0;
}

static struct argp argp = {argp_options, argp_opthandle, "FILE1 FILE2 ...", argp_doc};


static char *human_size(double size, char *buf)
{
    int i = 0;
    const char* units[] = {" B", "KB", "MB", "GB"};
    while (size > 1024) {
        size /= 1024;
        i++;
    }
    sprintf(buf, "%.*f %s", i, size, units[i]);
    return buf;
}


// implementation by Jonathan Leffler : https://stackoverflow.com/a/675193/5750172
static int do_mkdir(const char *path, mode_t mode)
{
    struct stat st;
    int status = 0;

    if (stat(path, &st) != 0) {
        /* Directory does not exist. EEXIST for race condition */
        if (mkdir(path, mode) != 0 && errno != EEXIST)
            status = -1;
    } else if (!S_ISDIR(st.st_mode)) {
        errno = ENOTDIR;
        status = -1;
    }

    return status;
}


// implementation by Jonathan Leffler : https://stackoverflow.com/a/675193/5750172
/**
 * mkpath - ensure all directories in path exist
 * Algorithm takes the pessimistic view and works top-down to ensure
 * each directory in path exists, rather than optimistically creating
 * the last element and working backwards.
*/
static int mkpath(const char *path, mode_t mode)
{
    char *sp;
    int status = 0;
    char *copypath = strdup(path);
    char *pp = copypath;

    while (status == 0 && (sp = strchr(pp, '/')) != 0) {
        if (sp != pp) {
            /* Neither root nor double slash in path */
            *sp = '\0';
            status = do_mkdir(copypath, mode);
            *sp = '/';
        }
        pp = sp + 1;
    }
    if (0 == status)
        status = do_mkdir(path, mode);
    free(copypath);
    return status;
}


static FILE *path_fopen(const char *path, const char *mode)
{
	char *cpy = strdup(path);
	char *pth = dirname(cpy);
	if (mkpath(pth, 0755)) {
		free(cpy);
		return NULL;
	}
	free(cpy);
	return fopen(path, mode);
}


static int file_extract(xrofs_dev img, const char *filename, const char *dirname)
{
	if (img == NULL || filename == NULL || dirname == NULL)
		return -EINVAL;

	int dlen = strlen(dirname) + strlen("/") + strlen(filename);
	char dbase[dlen+1];
	strcpy(dbase, dirname);
	strcat(dbase, "/");
	strcat(dbase, filename);

	xrofs_file src = xrofs_open(img, filename);
	if (XROFS_ISFNULL(src))
		return -EBADF;
	int flen = xrofs_toend(&src);

	FILE *dst = path_fopen(dbase, "w");
	if (NULL == dst)
		return -EIO;

	uint8_t buf[16 * 1024];
	int cnt;
	while(cnt = xrofs_read(&src, buf, sizeof(buf))) {
		flen -= fwrite(buf, sizeof *buf, cnt, dst);
	}
	fclose(dst);
	return flen >= 0 ? flen : -flen;
}


int main(int argc, char *argv[])
{
	struct arguments args = {
		.isverbose = true,
		.imgname = NULL,
		.dirname = NULL,
		.filenames = NULL
		// others are init to 0 per C std, while NULL is not 0 on every platform
	};

	error_t argperr = argp_parse(&argp, argc, argv, 0, NULL, &args);

/*
	printf("Arguments:\n"
		   "> isverbose = %c\n"
		   "> isall = %c\n"
		   "> islist = %c\n"
		   "> imgname = %s\n"
		   "> dirname = %s\n",
		   args.isverbose ? 'T' : 'F',
		   args.isall ? 'T' : 'F',
		   args.islist ? 'T' : 'F',
		   args.imgname == NULL ? "(NULL)" : args.imgname,
           args.dirname == NULL ? "(NULL)" : args.dirname);
	printf("> filenames (%d):\n", args.filenames_cnt);
	for (long long j = 0; j < args.filenames_cnt; j++)
		printf("|- '%s'\n", args.filenames[j]);
	printf("\n");
*/

	if (NULL == args.imgname) {
		fprintf(stderr, "No xrofs image file provided\n");
		exit(-EINVAL);
	}

	// image contents listing logic
	if (args.islist && ((args.filenames_cnt != 0) || (args.dirname != NULL))) {
		fprintf(stderr, "To list image contents NO filenames or dirname should be given\n");
		exit(-EINVAL);
	}

	if (args.isall && (args.filenames_cnt != 0)) {
		fprintf(stderr, "Ambiguous arguments: either --all OR "
						"separate filenames should be given\n");
		exit(-EINVAL);
	}

	if (!args.islist && (args.dirname == NULL)) {
		fprintf(stderr, "No output directory provided for extaction\n");
		exit(-EINVAL);
	}

	if ((args.dirname != NULL) && mkdir(args.dirname, 0755)) {
		err_handler(cln_start, "Resulting directory should NOT exist before extraction");
	}

    int fd = open(args.imgname, O_RDONLY);
    if (fd < 0)
        err_handler(cln_start, "File open failed");

    struct stat st;
    if (fstat(fd, &st))
        perror(NULL);
    long size = st.st_size;

    xrofs_dev img = mmap(0, size, PROT_READ, MAP_SHARED, fd, 0);
    if (MAP_FAILED == img)
        err_handler(cln_fopen, "MMAP failed");

    long long nfiles = xrofs_numentries(img);
    if (args.isverbose && !args.islist) {
		char buf[50];
        printf("* Discovered %lld files totalling %s\n", nfiles, human_size(size, buf)); 
    }

    xrofs_entry *entries = xrofs_entries(img);

	if (args.islist) {
		for (long long i = 0; args.islist && (i < nfiles); i++) {
	        char buf[50];
	        human_size(entries[i].fsize, buf);

	        char *name = xrofs_entry_fname(&entries[i], img);
			// listing logic
	        if (args.islist)
	            printf("%-67s %11s\n", name, buf);
	        else
	            printf("%s\n", name);
	    }
		goto cln_mmap;
	}

	// files extraction logic
	if (args.isall || (args.filenames_cnt != 0)) {
		if (args.isverbose)
			printf("* Extracting \"%s\" to \"%s\"\n", args.imgname, args.dirname);

		int dbaselen = strlen(args.dirname) + strlen("/");
		char dbase[dbaselen + 1];
		strcpy(dbase, args.dirname);
		strcpy(dbase, "/");
		
		long long cnt = args.isall ? nfiles : args.filenames_cnt;
		for (long long i = 0; i < cnt; i++) {
			char *name = args.isall ? xrofs_entry_fname(&entries[i], img) : args.filenames[i];
			int namelen = strlen(name);
			char path[dbaselen + namelen + 1];
			strcpy(path, dbase);
			strcat(path, name);

			long long size;
			if (args.isall) {
				size = entries[i].fsize;
			} else {
				xrofs_file fd = xrofs_open(img, name);
				if (XROFS_ISFNULL(fd)) {
					fprintf(stderr, "File \"%s\" not found\n", name);
					break;
				}
				size = xrofs_toend(&fd);
			}

			if (args.isverbose) {
				char szfmt[50];
				human_size(size, szfmt);
				printf("[%lld/%lld %9s] %s\n", i+1, cnt, szfmt, name);
			}

			int st = file_extract(img, name, args.dirname);
			switch (st) {
			case 0:			break;
			case -EBADF:	fprintf(stderr, "Abnormal library behaviour\n"); break;
			case -EIO:		fprintf(stderr, "Destination file open error\n"); break;
			default:		fprintf(stderr, "I/O Error\n");
			}
		}
		goto cln_mmap;
	}

    cln_mmap:;
        munmap(img, size);
    cln_fopen:
        close(fd);
    cln_start:

	// following GNU coding standards
	if (errno)
		exit(errno);
	else
    	return 0;
}
