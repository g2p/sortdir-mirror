#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <limits.h>

struct dirlist {
	int pos;
	int max;
	struct dirent *entries;
	struct dirent64 *entries64;
};

static struct dirlist dirlist[1024];

// Ezt miért nem szedi ki a dlfcn.h-ból???
#define RTLD_NEXT ((void *) -1l)

static struct dirent *(*next_readdir)(DIR *dir);
static struct dirent64 *(*next_readdir64)(DIR *dir);
static DIR *(*next_opendir)(const char *name);
static int (*next_closedir)(DIR *dir);

static int do_wrap = 0;

void sortdir_init (void) __attribute((constructor));
void sortdir_init (void)
{
	next_readdir   = dlsym(RTLD_NEXT, "readdir");
	next_readdir64 = dlsym(RTLD_NEXT, "readdir64");
	next_opendir   = dlsym(RTLD_NEXT, "opendir");
	next_closedir  = dlsym(RTLD_NEXT, "closedir");
	do_wrap = 1;
}

static int compare (const void *a, const void *b)
{
	const struct dirent *a2 = a;
	const struct dirent *b2 = b;
	return strcmp(a2->d_name, b2->d_name);
} 

static int compare64 (const void *a, const void *b)
{
	const struct dirent64 *a2 = a;
	const struct dirent64 *b2 = b;
	return strcmp(a2->d_name, b2->d_name);
} 

struct dirent *readdir (DIR *dir)
{
	int fd;
	struct dirent *dirent;
	dirent = next_readdir(dir);
	if (!do_wrap) return (struct dirent *) dirent;
	do_wrap = 0;
	fd = dirfd(dir);
	if (dirlist[fd].pos == dirlist[fd].max) {
		do_wrap = 1;
		return NULL;
	}
	dirent = &dirlist[fd].entries[dirlist[fd].pos];
	dirlist[fd].pos++;
	do_wrap = 1;
	return (struct dirent *) dirent;
}

struct dirent64 *readdir64 (DIR *dir)
{
	int fd;
	struct dirent64 *dirent;
	dirent = next_readdir64(dir);
	if (!do_wrap) return (struct dirent64 *) dirent;
	do_wrap = 0;
	fd = dirfd(dir);
	if (dirlist[fd].pos == dirlist[fd].max) {
		do_wrap = 1;
		return NULL;
	}
	dirent = &dirlist[fd].entries64[dirlist[fd].pos];
	dirlist[fd].pos++;
	do_wrap = 1;
	return (struct dirent64 *) dirent;
}

DIR *opendir (const char *name)
{
	DIR *dir;
	struct dirent *dirent;
	int fd;
	int i = 0;
	int alloc = 64;
	dir = next_opendir(name);
	if (dir == NULL) return dir;
	if (!do_wrap) return dir;
	do_wrap = 0;
	fd = dirfd(dir);
	dirlist[fd].entries   = malloc(alloc * sizeof(struct dirent));
	dirlist[fd].entries64 = malloc(alloc * sizeof(struct dirent64));
	while ((dirent = next_readdir(dir)) != NULL) {
		if (i >= alloc) {
			alloc *= 2;
			dirlist[fd].entries   = realloc(dirlist[fd].entries,   alloc * sizeof(struct dirent));
			dirlist[fd].entries64 = realloc(dirlist[fd].entries64, alloc * sizeof(struct dirent64));
		}
		memcpy(&dirlist[fd].entries[i], dirent, sizeof(struct dirent));
		dirlist[fd].entries64[i].d_ino    = dirent->d_ino;
		dirlist[fd].entries64[i].d_off    = dirent->d_off;
		dirlist[fd].entries64[i].d_reclen = dirent->d_reclen;
		dirlist[fd].entries64[i].d_type   = dirent->d_type;
		strcpy(dirlist[fd].entries64[i].d_name, dirent->d_name);
		i++;
	}
	qsort(dirlist[fd].entries,   i, sizeof(struct dirent),   compare);
	qsort(dirlist[fd].entries64, i, sizeof(struct dirent64), compare64);
	dirlist[fd].pos = 0;
	dirlist[fd].max = i;
	do_wrap = 1;
	rewinddir(dir);
	return dir;
}

int closedir (DIR *dir)
{
	int fd;
	int retval;
	if (!do_wrap) return next_closedir(dir);
	do_wrap = 0;
	fd = dirfd(dir);
	free(dirlist[fd].entries64);
	free(dirlist[fd].entries);
	retval = next_closedir(dir);
	do_wrap = 1;
	return retval;
}

