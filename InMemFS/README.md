## Implement an in-memory file system (i.e., RAMDISK) using FUSE

### FUSE
Modern operating systems support multiple file systems.
The operating system directs each file system operation to the appropriate implementation of the routine.
It multiplexes the file system calls across many distinct
implementations.
For example, on a read system call the operation system uses NFS code if it is an NFS file but uses ext3 code if it is an ext3 file.

[FUSE](https://github.com/libfuse/libfuse)
(Filesystem in Userspace) is an interface that exports file system operations to user-space.
Thus file system routines are executed in user-space.
Continuing the example, if the file is a FUSE file then operating system up-calls into user space in order to invoke the code associated with read.

### RAMDISK
Instead of reading and writing disk blocks, the RAMDISK file system will use main memory for storage.

Externally, the RAMDISK appears as a standard Unix FS.
Notably, it is hierarchical (has directories) and supports standard accesses, such as read, write, and append.

However, the file system is not persistent. The data and metadata are lost when the process terminates, which is
also when the process frees the memory it has allocated.

#### Basics for RAMDISK
Be able to run the following system calls:
  * open, close
  * read, write
  * creat [sic], mkdir
  * unlink, rmdir
  * opendir, readdir

#### Limitations for RAMDISK
RAMDISK is **not** expected to support the following:
  * Access control
  * Links
  * Symbolic links
