#define FUSE_USE_VERSION 26

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif

#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <sys/stat.h>

// file struct
typedef struct FileInfo FileInfo;
struct FileInfo {
	char *name;
	char *data;
	unsigned int size; // in bytes
	bool isFile;

	// pointers for directory structure
	FileInfo *parent;
	FileInfo *children; // first child
	FileInfo *next; // next sibling
};

// global variables
static int disk_size;
static FileInfo *root = NULL;

// utilities
static void* smalloc(size_t size) { // safe malloc
	void *ptr = malloc(size);
	if(ptr == NULL) {
		fprintf(stderr, "Failed to allocate memory with malloc()!\n");
		exit(1);
	}
	return ptr;
}

// BEGIN file functions
static void File_update_size(FileInfo *file, int delta);

static FileInfo* File_create(const char *name, bool isFile) {
	FileInfo *fi = (FileInfo*) smalloc(sizeof(FileInfo));
	memset(fi, 0, sizeof(FileInfo));
	fi->name = smalloc(strlen(name) + 1);
	strcpy(fi->name, name);
	fi->isFile = isFile;
	return fi;
}

static void File_destroy(FileInfo *fi, bool destroySiblings) {
	if(fi == NULL) return;
	if(destroySiblings) File_destroy(fi->next, destroySiblings);
	File_destroy(fi->children, true);
	free(fi->name);
	if(fi->data != NULL) {
		free(fi->data);
		File_update_size(fi->parent, -fi->size);
	}
	free(fi);
}

static FileInfo* File_find(const char *path, FileInfo *fi) {
	// base cases
	if(fi == NULL) return NULL;
	if(!strcmp(path, fi->name)) return fi;

	// recursive find
	if(strstr(path, fi->name) == path) {
		FileInfo *res = File_find(path, fi->children);
		if(res != NULL) return res;
	}
	return File_find(path, fi->next);
}

static FileInfo* File_find_parent(const char *path) {
	char tmp[strlen(path) + 1];
	strcpy(tmp, path);
	return File_find(dirname(tmp), root);
}

static void File_stat(FileInfo *fi, struct stat *stbuf) {
	stbuf->st_mode = fi->isFile ? S_IFREG | 0644 : S_IFDIR | 0755;
	stbuf->st_nlink = fi->isFile ? 1 : 2;
	stbuf->st_uid = getuid();
	stbuf->st_gid = getgid();
	stbuf->st_size = fi->size;
	stbuf->st_blocks = 0;
	stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);
}

static void File_remove_child(FileInfo *parent, FileInfo *child, bool destroy) {
	FileInfo *prevChild = NULL;
	FileInfo *currChild = parent->children;
	if(currChild == child) parent->children = child->next;
	else {
		while(currChild->next != NULL) {
			prevChild = currChild;
			currChild = currChild->next;
			if(currChild == child) {
				prevChild->next = child->next;
				break;
			}
		}
	}
	if(destroy) File_destroy(child, false);
	else {
		child->parent = child->next = NULL;
	}
}

static void File_rename(FileInfo *file, const char *newname) {
	char *oldname = file->name;
	size_t oldlen = strlen(oldname);
	size_t newlen = strlen(newname);
	int delta = newlen - oldlen;

	// Copy new name
	file->name = smalloc(newlen + 1);
	strcpy(file->name, newname);

	// Update children name
	FileInfo *child = file->children;
	while(child != NULL) {
		char child_newname[strlen(child->name) + delta + 1];
		memcpy(child_newname, newname, newlen);
		strcpy(child_newname + newlen, child->name + oldlen);
		File_rename(child, child_newname);
		child = child->next;
	}

	free(oldname);
}

static void File_add_child(FileInfo *parent, FileInfo *child) {
	child->parent = parent;
	child->next = parent->children;
	parent->children = child;
}

static int File_resize(FileInfo *file, size_t size) {
	if(file == NULL || !file->isFile) return -EINVAL;
	if(file->size == size) return 0;
	
	int delta = size - file->size;
	if(root->size + delta > disk_size)
		return -ENOSPC;	
	File_update_size(file->parent, delta);

	file->size = size;
	if(size == 0) {
		free(file->data);
		file->data = NULL;
	} else {
		file->data = realloc(file->data, file->size);
		if(file->data == NULL) {
			fprintf(stderr, "Failed to allocate memory with realloc()!\n");
			exit(1);
		}
	}
	return 0;
}

static void File_update_size(FileInfo *file, int delta) {
	while(file != NULL) {
		file->size += delta;
		file = file->parent;
	}
}
// END file functions

static int ramdisk_getattr(const char *path, struct stat *stbuf)
{
	FileInfo *fi = File_find(path, root);
	if(fi != NULL) {
		File_stat(fi, stbuf);
		return 0;
	} else {
		return -ENOENT;
	}
}

static int ramdisk_access(const char *path, int mask)
{
	FileInfo *fi = File_find(path, root);
	if(fi == NULL) return -ENOENT;
	else return 0;
}

static int ramdisk_readlink(const char *path, char *buf, size_t size)
{
	fprintf(stderr, "ramdisk_readlink is not implemented!\n");
	return -EPERM;
}


static int ramdisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi)
{
	struct stat stbuf;
	FileInfo *finf = File_find(path, root);
	if(finf == NULL) return -ENOENT;
	else if(finf->isFile) {
		File_stat(finf, &stbuf);
		char tmp[strlen(finf->name) + 1];
		strcpy(tmp, finf->name);
  		filler (buf, basename(tmp), &stbuf, 0);
	} else {
		filler (buf, ".", NULL, 0);
	  	filler (buf, "..", NULL, 0);
	  	FileInfo *child = finf->children;
	  	while(child != NULL) {
	  		File_stat(child, &stbuf);
			char tmp[strlen(child->name) + 1];
			strcpy(tmp, child->name);
	  		filler (buf, basename(tmp), &stbuf, 0);
	  		child = child->next;
	  	}
	}
	return 0;
}

static int ramdisk_mkentry(const char *path, bool isFile) {
	if(File_find(path, root) != NULL) return -EEXIST;
	FileInfo *fi = File_find_parent(path);
	if(fi == NULL || fi->isFile) {
		return fi == NULL ? -ENOENT : -ENOTDIR;
	} else {
		File_add_child(fi, File_create(path, isFile));
		return 0;
	}
}

static int ramdisk_mknod(const char *path, mode_t mode, dev_t rdev)
{
	return ramdisk_mkentry(path, true);
}

static int ramdisk_mkdir(const char *path, mode_t mode)
{
	return ramdisk_mkentry(path, false);
}

static int ramdisk_unlink(const char *path)
{
	FileInfo *fi = File_find(path, root);
	if(fi == NULL) return -ENOENT;
	if(!fi->isFile) return -EISDIR;
	File_remove_child(fi->parent, fi, true);
	return 0;
}

static int ramdisk_rmdir(const char *path)
{
	FileInfo *fi = File_find(path, root);
	if(fi == NULL) return -ENOENT;
	if(fi == root) return -EBUSY;
	if(fi->isFile) return -ENOTDIR;
	if(fi->children != NULL) return -ENOTEMPTY;
	File_remove_child(fi->parent, fi, true);
	return 0;
}

static int ramdisk_symlink(const char *from, const char *to)
{
	fprintf(stderr, "ramdisk_symlink is not implemented!\n");
	return -EPERM;
}

static int ramdisk_rename(const char *from, const char *to)
{
	if(!strcmp(from, to)) return 0; // nothing to do
	if(strstr(to, from) == to) return -EINVAL; // can't move to a sub directory of itself

	FileInfo *fromfile = File_find(from, root);
	FileInfo *tofile = File_find(to, root);
	FileInfo *parentfile = File_find_parent(to);

	if(fromfile == root || tofile == root) return -EBUSY; // neither old or new can point to the root dir
	if(fromfile == NULL || parentfile == NULL) return -ENOENT; // origin or destination's parent folder does not exist
	if(parentfile->isFile) return -ENOTDIR; // destination's parent is not a folder
	if(tofile != NULL) { // target exists
		if(!tofile->isFile) { // target is a directory
			if(tofile->children != NULL) return -ENOTEMPTY; // can't override a non-empty folder
			if(fromfile->isFile) return -EISDIR; // origin is a file
		} else { // target is a file
			if(!fromfile->isFile) return -ENOTDIR;
		}
	}

	// Done with error checking, now move it
	if(tofile != NULL) File_remove_child(parentfile, tofile, true);
	File_remove_child(fromfile->parent, fromfile, false);
	File_rename(fromfile, to);
	File_add_child(parentfile, fromfile);

	return 0;
}

static int ramdisk_link(const char *from, const char *to)
{
	fprintf(stderr, "ramdisk_link is not implemented!\n");
	return -EPERM;
}

static int ramdisk_chmod(const char *path, mode_t mode)
{
	fprintf(stderr, "ramdisk_chmod is not implemented!\n");
	return -EPERM;
}

static int ramdisk_chown(const char *path, uid_t uid, gid_t gid)
{
	fprintf(stderr, "ramdisk_chown is not implemented!\n");
	return -EPERM;
}

static int ramdisk_truncate(const char *path, off_t size)
{
	if(size < 0) return -EINVAL;
	FileInfo *file = File_find(path, root);
	if(file == NULL) return -ENOENT;
	if(!file->isFile) return -EISDIR;

	int oldsize = file->size;
	int res = File_resize(file, size);
	if(res == 0 && size > oldsize) { // success, memset new bytes to 0
		memset(file->data + oldsize, 0, size - oldsize);
	}
	return res;
}

#ifdef HAVE_UTIMENSAT
static int ramdisk_utimens(const char *path, const struct timespec ts[2])
{
	fprintf(stderr, "ramdisk_utimens is not implemented!\n");
	return -EPERM;
}
#endif

static int ramdisk_open(const char *path, struct fuse_file_info *fi)
{
	FileInfo *finf = File_find_parent(path);
	if(finf == NULL || finf->isFile) return -ENOENT;
	else return 0;
}

static int ramdisk_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	FileInfo *file = File_find(path, root);
	if(file == NULL) return -ENOENT;
	if(!file->isFile) return -EISDIR;

	// reading
	int max = file->size - offset;
	if(max < 0) return -EINVAL;
	if(size > max) size = max;
	if(size > 0) memcpy(buf, file->data + offset, size);
	return size;
}

static int ramdisk_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	FileInfo *file = File_find(path, root);
	if(file == NULL) return -ENOENT;
	if(!file->isFile) return -EINVAL;

	// check if additional space is needed
	int additionalBytes = offset + size - file->size;
	if(additionalBytes > 0) {
		int res = File_resize(file, file->size + additionalBytes);
		if(res != 0) return res;
	}

	// do the writing
	memcpy(file->data + offset, buf, size);
	return size;
}

static int ramdisk_statfs(const char *path, struct statvfs *stbuf)
{
	fprintf(stderr, "ramdisk_statfs is not implemented!\n");
	return -EPERM;
}

#ifdef HAVE_POSIX_FALLOCATE
static int ramdisk_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	fprintf(stderr, "ramdisk_fallocate is not implemented!\n");
	return -EPERM;
}
#endif

static struct fuse_operations ramdisk_oper = {
	.getattr	= ramdisk_getattr,
	.access		= ramdisk_access,
	.readlink	= ramdisk_readlink,
	.readdir	= ramdisk_readdir,
	.mknod		= ramdisk_mknod,
	.mkdir		= ramdisk_mkdir,
	.symlink	= ramdisk_symlink,
	.unlink		= ramdisk_unlink,
	.rmdir		= ramdisk_rmdir,
	.rename		= ramdisk_rename,
	.link		= ramdisk_link,
	.chmod		= ramdisk_chmod,
	.chown		= ramdisk_chown,
	.truncate	= ramdisk_truncate,
#ifdef HAVE_UTIMENSAT
	.utimens	= ramdisk_utimens,
#endif
	.open		= ramdisk_open,
	.read		= ramdisk_read,
	.write		= ramdisk_write,
	.statfs		= ramdisk_statfs,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= ramdisk_fallocate,
#endif
};

int main(int argc, char *argv[])
{
	if(argc != 3) {
		printf("Usage: %s <mount-path> <size-in-MB>\n", argv[0]);
		return 1;
	}

	// get ramdisk size (in bytes)
	disk_size = atoi(argv[argc-1]);
	if(disk_size <= 0) {
		puts("Invalid disk size!");
		return 1;
	}
	disk_size = disk_size * 1024 * 1024;

	// setup root folder
	root = File_create("/", false);

	// mount with fuse
	umask(0);
	int res = fuse_main(argc-1, argv, &ramdisk_oper, NULL);

	// cleanup
	File_destroy(root, false);
	return res;
}
