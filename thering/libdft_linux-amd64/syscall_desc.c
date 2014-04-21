/*-
 * Copyright (c) 2010, 2011, 2012, 2013, Columbia University
 * All rights reserved.
 *
 * This software was developed by Vasileios P. Kemerlis <vpk@cs.columbia.edu>
 * at Columbia University, New York, NY, USA, in June 2010.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *   * Neither the name of Columbia University nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * 02/23/2011:
 * 	some conflict arises when numaif.h is included before syscall_desc.h
 * 	the proposed fix was done by Georgios Portokalidis
 * 	(porto@cs.columbia.edu)
 */

/*
 * TODO:
 * 	- add ioctl() handler
 * 	- add nfsservctl() handler
 */

#include <sys/epoll.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/uio.h>

#include <asm/fcntl.h>
#include <asm/stat.h>
#include <linux/sysctl.h>

#include <err.h>
#include <poll.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "syscall_desc.h"
#include "tagmap.h"
#include <linux/mempolicy.h>


/* callbacks declaration */
static void post_read_hook(syscall_ctx_t*);
static void post_fcntl_hook(syscall_ctx_t*);
static void post_getgroups16_hook(syscall_ctx_t*);
static void post_mmap_hook(syscall_ctx_t*);
static void post_socketcall_hook(syscall_ctx_t*);
static void post_syslog_hook(syscall_ctx_t*);
static void post_ipc_hook(syscall_ctx_t*);
static void post_modify_ldt_hook(syscall_ctx_t*);
static void post_quotactl_hook(syscall_ctx_t *ctx);
static void post_readv_hook(syscall_ctx_t*);
static void post__sysctl_hook(syscall_ctx_t*);
static void post_poll_hook(syscall_ctx_t *ctx);
static void post_rt_sigpending_hook(syscall_ctx_t *ctx);
static void post_getcwd_hook(syscall_ctx_t *ctx);
static void post_getgroups_hook(syscall_ctx_t*);
static void post_mincore_hook(syscall_ctx_t *ctx);
static void post_getdents_hook(syscall_ctx_t *ctx);
static void post_getxattr_hook(syscall_ctx_t *ctx);
static void post_listxattr_hook(syscall_ctx_t *ctx);
static void post_io_getevents_hook(syscall_ctx_t *ctx);
static void post_get_mempolicy_hook(syscall_ctx_t *ctx);
static void post_lookup_dcookie_hook(syscall_ctx_t *ctx);
static void post_mq_timedreceive_hook(syscall_ctx_t *ctx);
static void post_readlinkat_hook(syscall_ctx_t*);
static void post_epoll_wait_hook(syscall_ctx_t *ctx);
static void post_shmctl_hook(syscall_ctx_t *ctx);
static void post_nanosleep_hook(syscall_ctx_t *ctx);
static void post_accept_hook(syscall_ctx_t *ctx);
static void post_recvfrom_hook(syscall_ctx_t *ctx);
static void post_recvmsg_hook(syscall_ctx_t *ctx);
static void post_socketpair_hook(syscall_ctx_t *ctx);
static void post_getsockopt_hook(syscall_ctx_t *ctx);
static void post_semctl_hook(syscall_ctx_t *ctx);
static void post_msgrcv_hook(syscall_ctx_t *ctx);
static void post_msgctl_hook(syscall_ctx_t *ctx);
static void post_prctl_hook(syscall_ctx_t *ctx);
static void post_arch_prctl_hook(syscall_ctx_t *ctx);
static void post_sendmmsg_hook(syscall_ctx_t *ctx);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
static void post_recvmmsg_hook(syscall_ctx_t *ctx);
#endif

/* syscall descriptors */
syscall_desc_t syscall_desc[SYSCALL_MAX] = {
        /* __NR_read            0 */
        {3, 1, 0, {0, 0, 0, 0, 0, 0}, NULL, post_read_hook},
	/* __NR_write           1 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_open            2 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_close           3 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_stat            4 */
	{ 2, 0, 1, { 0, sizeof(struct __old_kernel_stat), 0, 0, 0, 0 }, NULL,
	NULL },
	/* __NR_fstat           5 */
	{ 2, 0, 1, { 0, sizeof(struct __old_kernel_stat), 0, 0, 0, 0 }, NULL,
	NULL },
	/* __NR_lstat           6 */
	{ 2, 0, 1, { 0, sizeof(struct __old_kernel_stat), 0, 0, 0, 0 }, NULL,
	NULL },
	/* __NR_poll            7 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_poll_hook },
	/* __NR_lseek           8 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mmap2           9 */
	{ 6, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_mmap_hook },
	/* __NR_mprotect        10 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_munmap          11 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_brk             12*/
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigaction    13 */
	{ 4, 0, 1, { 0, 0, sizeof(struct sigaction), 0, 0, 0 }, NULL, NULL },
	/* __NR_sigprocmask     14 */
	{ 3, 0, 1, { 0, 0, sizeof(old_sigset_t), 0, 0, 0 }, NULL, NULL },
	/* __NR_sigreturn       15 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ioctl; TODO     16 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pread64         17 */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_read_hook },
	/* __NR_pwrite64        18 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_readv           19 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_readv_hook },
	/* __NR_writev          20 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_access          21 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pipe            22 */
	{ 1, 0, 1, { sizeof(int) * 2, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_select          23 */
	{ 5, 0, 1, { 0, sizeof(fd_set), sizeof(fd_set), sizeof(fd_set), 
	sizeof(struct timeval), 0 }, NULL, NULL },
	/* __NR_sched_yield     24 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mremap          25 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_msync           26 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mincore         27 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_mincore_hook },
	/* __NR_madvise         28 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_shmget          29 */
        { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_shmat           30 */
        { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL  },
        /* __NR_shmctl          31 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_shmctl_hook },
	/* __NR_dup             32 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_dup2            33 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pause           34 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_nanosleep       35 */
	{ 2, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_nanosleep_hook },
	/* __NR_getitimer       36 */
	{ 2, 0, 1, { 0, sizeof(struct itimerval), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_alarm           37 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setitimer       38 */
	{ 3, 0, 1, { 0, 0, sizeof(struct itimerval), 0, 0, 0 }, NULL, NULL },
	/* __NR_getpid          39 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },	
	/* __NR_sendfile        40 */
	{ 4, 0, 1, { 0, 0, sizeof(off_t), 0, 0, 0 }, NULL, NULL },
        /* __NR_socket          41  */
        { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_connect         42 */
        { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_accept          43 */
        { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_accept_hook },
        /* __NR_sendto          44 */
        { 6, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_recvfrom        45 */
        { 6, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_recvfrom_hook },
        /* __NR_sendmsg         46 */
        { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_recvmsg         47 */
        { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_recvmsg_hook },
        /* __NR_shutdown        48 */
        { 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_bind            49 */
        { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_listen          50 */
        { 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_getsockname     51 */
        { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_accept_hook },
        /* __NR_getpeername     52 */
        { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_accept_hook },
        /* __NR_socketpair      53 */
        { 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_socketpair_hook },
        /* __NR_setsockopt      54 */
        { 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_getsockopt      55 */
        { 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getsockopt_hook },
	/* __NR_clone           56 */
	{ 5, 0, 1, { 0, 0, sizeof(int), 0, 0, 0 }, NULL, NULL },
	/* __NR_fork            57 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_vfork           58 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_execve          59 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_exit            60 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_wait4           61 */
	{ 4, 0, 1, { 0, sizeof(int), 0, sizeof(struct rusage), 0, 0 },
	NULL, NULL },
	/* __NR_kill            62 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_uname           63 */
	{ 1, 0, 1, { sizeof(struct new_utsname), 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_semget          64 */
        { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_semop           65 */
        { 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_semctl          66 */
        { 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_semctl_hook },
        /* __NR_shmdt           67 */
        { 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_msgget          68 */
        { 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_msgsnd          69 */
        { 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_msgrcv          70 */
        { 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_msgrcv_hook },
        /* __NR_msgctl          71 */
        { 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_msgctl_hook },
	/* __NR_fcntl           72 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_fcntl_hook },
	/* __NR_flock           73 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fsync           74 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fdatasync       75 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_truncate        76 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ftruncate       77 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getdents        78 */
	{ 3, 0, 1, { 0, sizeof(struct linux_dirent), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getcwd          79 */
	{ 2, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getcwd_hook },
	/* __NR_chdir           80 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fchdir          81 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rename          82 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mkdir           83 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rmdir           84 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_creat           85 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_link            86 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_unlink          87 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_symlink         88 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_readlink        89 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_read_hook },
	/* __NR_chmod           90 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fchmod          91 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_chown           92 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fchown          93 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_lchown          94 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_umask           95 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_gettimeofday    96 */
	{ 2, 0, 1, { sizeof(struct timeval), sizeof(struct timezone), 0,
	 0, 0, 0 }, NULL, NULL },
	/* __NR_getrlimit       97 */
	{ 2, 0, 1, { 0, sizeof(struct rlimit), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getrusage       98 */
	{ 2, 0, 1, { 0, sizeof(struct rusage), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sysinfo         99 */
	{ 1, 0, 1, { sizeof(struct sysinfo), 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_times           100 */
	{ 1, 0, 1, { sizeof(struct tms), 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ptrace          101 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getuid          102 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_syslog          103 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_syslog_hook },
	/* __NR_getgid          104 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setuid          105 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setgid          106 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_geteuid         107 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getegid         108 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setpgid         109 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getppid         110 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getpgrp         111 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setsid          112 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setreuid        113 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setregid        114 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getgroups       115 */
	{ 2, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getgroups_hook },
	/* __NR_setgroups       116 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setresuid       117 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getresuid       118 */
	{ 3, 0, 1, { sizeof(uid_t), sizeof(uid_t), sizeof(uid_t), 0, 0, 0 },
	NULL, NULL },
	/* __NR_setresgid       119 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getresgid       120 */
	{ 3, 0, 1, { sizeof(gid_t), sizeof(gid_t), sizeof(gid_t), 0, 0, 0 },
	NULL, NULL },
	/* __NR_getpgid         121 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setfsuid        122 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setfsgid        123 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getsid          124 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_capget          125 */
	{ 2, 0, 1, { sizeof(cap_user_header_t), sizeof(cap_user_data_t), 0, 0,
	0, 0 }, NULL, NULL },
	/* __NR_capset          126 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigpending   127 */
	{ 2, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_rt_sigpending_hook },
	/* __NR_rt_sigtimedwait 128 */
	{ 4, 0, 1, { 0, sizeof(siginfo_t), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigqueueinfo 129 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_rt_sigsuspend   130 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sigaltstack     131 */
	{ 2, 0, 1, { 0, sizeof(stack_t), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_utime           132 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mknod           133 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_uselib          134 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_personality     135 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ustat           136 */
	{ 2, 0, 1, { 0, sizeof(struct ustat), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_statfs          137 */
	{ 2, 0, 1, { 0, sizeof(struct statfs), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fstatfs         138 */
	{ 2, 0, 1, { 0, sizeof(struct statfs), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sysfs           139 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getpriority     140 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setpriority     141 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_setparam  142 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_getparam  143 */
	{ 2, 0, 1, { 0, sizeof(struct sched_param), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_setscheduler 144 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_getscheduler 145 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_get_priority_max 146 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_get_priority_min 147 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_rr_get_interval 148 */
	{ 2, 0, 1, { 0, sizeof(struct timespec), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mlock           149 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_munlock         150 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mlockall        151 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_munlockall      152 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_vhangup         153 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_modify_ldt      154 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_modify_ldt_hook },
	/* __NR_pivot_root      155 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR__sysctl         156 */
	{ 1, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post__sysctl_hook },
	/* __NR_prctl           157 */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_prctl_hook },
	/* __NR_arch_prctl      158 */
	{ 2, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_arch_prctl_hook },
	/* __NR_adjtimex        159 */
	{ 1, 0, 1, { sizeof(struct timex), 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setrlimit       160 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_chroot          161 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sync            162 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_acct            163 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_settimeofday    164 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mount           165 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_umount2         166 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_swapon          167 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_swapoff         168 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_reboot          169 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sethostname     170 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setdomainname   171 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_iopl            172 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ioperm          173 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_create_module; not implemented 174 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_init_module     175 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_delete_module   176 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_get_kernel_syms; not implemented 177*/
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_query_module; not implemented   178 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_quotactl        179 */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_quotactl_hook },
	/* __NR_nfsservctl; TODO 180 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_getpmsg; not implemented        181 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_putpmsg; not implemented        182 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_afs_syscall; not implemented        183 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_tuxcall; not implemented        184 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_security; not implemented        185 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_gettid          186 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_readahead       187 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_setxattr        188 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_lsetxattr       189 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fsetxattr       190 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getxattr        191 */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getxattr_hook },
	/* __NR_lgetxattr       192 */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getxattr_hook },
	/* __NR_fgetxattr       193 */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getxattr_hook },
	/* __NR_listxattr       194 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_listxattr_hook },
	/* __NR_llistxattr      195 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_listxattr_hook },
	/* __NR_flistxattr      196 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_listxattr_hook },
	/* __NR_removexattr     197 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_lremovexattr    198 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fremovexattr    199 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_tkill           200 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_time            201 */
	{ 1, 0, 1, { sizeof(time_t), 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_futex           202 */
        { 6, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_sched_setaffinity       203*/
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sched_getaffinity       204 */
	{ 3, 0, 1, { 0, 0, sizeof(cpu_set_t), 0, 0, 0 }, NULL, NULL },
	/* __NR_set_thread_area; not implemented 205 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_io_setup        206 */
	{ 2, 0, 1, { 0, sizeof(aio_context_t), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_io_destroy      207 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_io_getevents    208 */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_io_getevents_hook },
	/* __NR_io_submit       209 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_io_cancel       210 */
	{ 3, 0, 1, { 0, 0, sizeof(struct io_event), 0, 0, 0 }, NULL, NULL },
	/* __NR_get_thread_area; not implemented 211 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_lookup_dcookie          212 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_lookup_dcookie_hook },
	/* __NR_epoll_create    213 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_epoll_ctl_old; not implemented  214 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_epoll_wait_old; not implemented  215 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_remap_file_pages        216 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_getdents64              217 */
	{ 3, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_getdents_hook },
	/* __NR_set_tid_address         218 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_restart_syscall         219 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_semtimedop              220 */
        { 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fadvise64               221 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timer_create            222 */
	{ 3, 0, 1, { 0, 0, sizeof(timer_t), 0, 0, 0 }, NULL, NULL },
	/* __NR_timer_settime           223 */
	{ 4, 0, 1, { 0, 0, 0, sizeof(struct itimerspec), 0, 0 }, NULL, NULL },
	/* __NR_timer_gettime           224 */
	{ 2, 0, 1, { 0, sizeof(struct itimerspec), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timer_getoverrun        225 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timer_delete            226 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_settime           227 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_gettime           228 */
	{ 2, 0, 1, { 0, sizeof(struct timespec), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_getres            229 */
	{ 2, 0, 1, { 0, sizeof(struct timespec), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_nanosleep         230 */
	{ 4, 0, 1, { 0, 0, 0, sizeof(struct timespec), 0, 0 }, NULL, NULL },
	/* __NR_exit_group              231 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_epoll_wait              232 */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_epoll_wait_hook },
	/* __NR_epoll_ctl               233 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_tgkill                  234 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_utimes                  235 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_vserver; not implemented        236 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mbind                   237 */
	{ 6, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_set_mempolicy           238 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_get_mempolicy           239 */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_get_mempolicy_hook },
	/* __NR_mq_open                 240 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mq_unlink               241 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mq_timedsend            242 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },	
	/* __NR_mq_timedreceive         243 */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_mq_timedreceive_hook },
	/* __NR_mq_notify               244 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mq_getsetattr           245 */
	{ 3, 0, 1, { 0, 0, sizeof(struct mq_attr), 0, 0, 0 }, NULL, NULL },
	/* __NR_kexec_load              246 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_waitid                  247 */
	{ 4, 0, 1, { 0, 0, sizeof(siginfo_t), 0, sizeof(struct rusage), 0 },
	NULL, NULL },
	/* __NR_add_key                 248 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_request_key             249 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_keyctl                  250 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ioprio_set              251 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_ioprio_get              252 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_inotify_init            253 */
	{ 0, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_inotify_add_watch       254 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_inotify_rm_watch        255 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_migrate_pages           256 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_openat                  257 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mkdirat                 258 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_mknodat                 259 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fchownat                260 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_futimesat               261 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_newfstatat              262 */
	{ 4, 0, 0, { 0, 0, sizeof(struct stat), 0, 0, 0 }, NULL, NULL },
	/* __NR_unlinkat                263 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_renameat                264 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_linkat                  265 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_symlinkat               266 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_readlinkat              267 */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_readlinkat_hook },
	/* __NR_fchmodat                268 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_faccessat               269 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pselect6                270 */
	{ 6, 0, 1, { 0, sizeof(fd_set), sizeof(fd_set), sizeof(fd_set), 0, 0 }, 
	NULL, NULL },
	/* __NR_ppoll                   271 */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_poll_hook },
	/* __NR_unshare                 272 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_set_robust_list         273 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_get_robust_list         274 */
	{ 3, 0, 1, { 0, sizeof(struct robust_list_head*), sizeof(size_t), 0, 0,
        0 }, NULL, NULL },
	/* __NR_splice                  275 */
	{ 6, 0, 1, { 0, sizeof(loff_t), 0, sizeof(loff_t), 0, 0 }, NULL, NULL },
	/* __NR_tee                     276 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_sync_file_range         277 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_vmsplice                278 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_move_pages              279 */
	{ 6, 0, 1, { 0, 0, 0, 0, sizeof(int), 0 }, NULL, NULL },
	/* __NR_utimensat               280 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_epoll_pwait             281 */
	{ 6, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_epoll_wait_hook },
	/* __NR_signalfd                282 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timerfd_create          283 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_eventfd                 284 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fallocate               285 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_timerfd_settime         286 */
	{ 4, 0, 1, { 0, 0, 0, sizeof(struct itimerspec), 0, 0 }, NULL, NULL },
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,26)
	/* __NR_timerfd_gettime         287 */
	{ 2, 0, 1, { 0, sizeof(struct itimerspec), 0, 0, 0, 0 }, NULL, NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,27)
        /* __NR_accept4                 288 */
        { 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_accept_hook },
	/* __NR_signalfd4               289 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_eventfd2                290 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_epoll_create1           291 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_dup3                    292 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_pipe2                   293 */
	{ 2, 0, 1, { sizeof(int) * 2, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_inotify_init1           294 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
	/* __NR_preadv                  295 */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_readv_hook },
	/* __NR_pwritev                 296 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,31)
	/* __NR_rt_tgsigqueueinfo       297 */
	{ 4, 0, 1, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_perf_event_open         298 */
	{ 5, 0, 1, { sizeof(struct perf_event_attr), 0, 0, 0, 0, 0 }, NULL,
	NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
	/* __NR_recvmmsg                299 */
	{ 5, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_recvmmsg_hook },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	/* __NR_fanotify_init           300 */
	{ 2, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_fanotify_mark           301 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_prlimit64               302 */
	{ 4, 0, 1, { 0, 0, 0, sizeof(struct rlimit64), 0, 0 }, NULL, NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,39)
	/* __NR_name_to_handle_at       303 */
	{ 5, 0, 1, { 0, 0, sizeof(struct file_handle), sizeof(int), 0, 0 },
	NULL, NULL },
	/* __NR_open_by_handle_at       304 */
	{ 3, 0, 1, { 0, sizeof(struct file_handle), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_clock_adjtime           305 */
	{ 2, 0, 1, { 0, sizeof(struct timex), 0, 0, 0, 0 }, NULL, NULL },
	/* __NR_syncfs                  306 */
	{ 1, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,0)
        /* __NR_sendmmsg                307 */
	{ 4, 1, 0, { 0, 0, 0, 0, 0, 0 }, NULL, post_sendmmsg_hook },
        /* __NR_setns                   308 */
	{ 4, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,1,0)
	/* __NR_getcpu                  309 */
	{ 3, 0, 1, { sizeof(unsigned), sizeof(unsigned),
	0, 0, 0, 0 }, NULL, NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,2,0)
        /* __NR_process_vm_readv        310 */
	{ 6, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
        /* __NR_process_vm_writev       311 */
	{ 6, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,5,0)
        /* __NR_kcmp                    312 */
	{ 5, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,8,0)
	/* __NR_finit_module            313 */
	{ 3, 0, 0, { 0, 0, 0, 0, 0, 0 }, NULL, NULL },
#endif
};


/*
 * add a new pre-syscall callback into a syscall descriptor
 *
 * @desc:	the syscall descriptor
 * @pre:	function pointer to the pre-syscall handler
 *
 * returns:	0 on success, 1 on error
 */
int
syscall_set_pre(syscall_desc_t *desc, void (* pre)(syscall_ctx_t*))
{
	/* sanity checks; optimized branch */
	if (unlikely((desc == NULL) | (pre == NULL)))
		/* return with failure */
		return 1;

	/* update the pre-syscall callback */
	desc->pre = pre;

	/* set the save arguments flag */
	desc->save_args = 1;

	/* success */
	return 0;
}

/*
 * add a new post-syscall callback into a syscall descriptor
 *
 * @desc:	the syscall descriptor
 * @post:	function pointer to the post-syscall handler
 *
 * returns:	0 on success, 1 on error
 */
int
syscall_set_post(syscall_desc_t *desc, void (* post)(syscall_ctx_t*))
{
	/* sanity checks; optimized branch */
	if (unlikely((desc == NULL) | (post == NULL)))
		/* return with failure */
		return 1;

	/* update the post-syscall callback */
	desc->post = post;
	
	/* set the save arguments flag */
	desc->save_args = 1;

	/* success */
	return 0;
}

/*
 * remove the pre-syscall callback from a syscall descriptor
 *
 * @desc:       the syscall descriptor
 *
 * returns:     0 on success, 1 on error
 */
int
syscall_clr_pre(syscall_desc_t *desc)
{
	/* sanity check; optimized branch */
	if (unlikely(desc == NULL))
		/* return with failure */
		return 1;

	/* clear the pre-syscall callback */
	desc->pre = NULL;

	/* check if we need to clear the save arguments flag */
	if (desc->post == NULL)
		/* clear */
		desc->save_args = 0;

	/* return with success */
	return 0;
}

/*
 * remove the post-syscall callback from a syscall descriptor
 *
 * @desc:       the syscall descriptor
 *
 * returns:     0 on success, 1 on error
 */
int
syscall_clr_post(syscall_desc_t *desc)
{
	/* sanity check; optimized branch */
	if (unlikely(desc == NULL))
		/* return with failure */
		return 1;

	/* clear the post-syscall callback */
	desc->post = NULL;

	/* check if we need to clear the save arguments flag */
	if (desc->pre == NULL)
		/* clear */
		desc->save_args = 0;

	/* return with success */
	return 0;
}

/* __NR_(p)read(64) and __NR_readlink post syscall hook */
static void
post_read_hook(syscall_ctx_t *ctx)
{
	/* read()/readlink() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;
	
	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

#if 0
/* __NR_getgroups16 post syscall_hook */
static void
post_getgroups16_hook(syscall_ctx_t *ctx)
{
	/* getgroups16() was not successful */
	if ((long)ctx->ret <= 0 || (old_gid_t *)ctx->arg[SYSCALL_ARG1] == NULL)
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1],
			(sizeof(old_gid_t) * (size_t)ctx->ret));
}
#endif

/* __NR_getgroups post syscall_hook */
static void
post_getgroups_hook(syscall_ctx_t *ctx)
{
	/* getgroups() was not successful */
	if ((long)ctx->ret <= 0 || ctx->arg[SYSCALL_ARG0] == 0)
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1],
			(sizeof(gid_t) * (size_t)ctx->ret));
}

/* __NR_readlinkat post syscall hook */
static void
post_readlinkat_hook(syscall_ctx_t *ctx)
{
	/* readlinkat() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;
	
	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG2], (size_t)ctx->ret);
}

/* __NR_mmap post syscall hook */
static void
post_mmap_hook(syscall_ctx_t *ctx)
{
	/* the map offset */
	size_t offset = (size_t)ctx->arg[SYSCALL_ARG1];

	/* mmap() was not successful; optimized branch */
	if (unlikely((void *)ctx->ret == MAP_FAILED))
		return;

	/* estimate offset; optimized branch */
	if (unlikely((offset % PAGE_SZ) != 0))
		offset += (PAGE_SZ - (offset % PAGE_SZ));

	/* grow downwards; optimized branch */
	if (unlikely((int)ctx->arg[SYSCALL_ARG3] & MAP_GROWSDOWN))
		/* fix starting address */
		ctx->ret -= (offset - 1);
	
	/* emulate the clear_tag() call */
	tagmap_clrn((size_t)ctx->ret, offset);
}

/* __NR_prctl post syscall hook */
static void
post_prctl_hook(syscall_ctx_t *ctx)
{
        /* prctl() was not successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;

        switch ((int)ctx->arg[SYSCALL_ARG0]) {
                case PR_GET_NAME:
                        /* clear tag bits */
                        tagmap_clrn(ctx->arg[SYSCALL_ARG1],
                                        (size_t) (index((char*) ctx->arg[SYSCALL_ARG1], '\0') -
                                        (char*) ctx->arg[SYSCALL_ARG1] + 1));
                        break;
                case PR_GET_PDEATHSIG:
                        /* clear tag bits */
                        tagmap_clrn(ctx->arg[SYSCALL_ARG1], sizeof(int));
                        break;
                default:
                        /* nothing to do */
                        return;
        }
}

/* __NR_prctl post syscall hook */
static void
post_arch_prctl_hook(syscall_ctx_t *ctx)
{
        /* prctl() was not successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;

        switch ((int)ctx->arg[SYSCALL_ARG0]) {
                case ARCH_GET_FS:
                case ARCH_GET_GS:
                        /* clear tag bits */
                        tagmap_clrn(ctx->arg[SYSCALL_ARG1], sizeof(unsigned long));
                        break;
                default:
                        /* nothing to do */
                        return;
        }
}

/* __NR_readv and __NR_preadv post syscall hook */
static void
post_readv_hook(syscall_ctx_t *ctx)
{
	/* iterators */
	int	i;
	struct	iovec *iov;
	
	/* bytes copied in a iovec structure */
	size_t	iov_tot;

	/* total bytes copied */
	size_t	tot = (size_t)ctx->ret;

	/* (p)readv() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;
	
	/* iterate the iovec structures */
	for (i = 0; i < (int)ctx->arg[SYSCALL_ARG2] && tot > 0; i++) {
		/* get an iovec  */
		iov = ((struct iovec *)ctx->arg[SYSCALL_ARG1]) + i;

		/* get the length of the iovec */
		iov_tot = (tot >= (size_t)iov->iov_len) ?
				(size_t)iov->iov_len : tot;
	
		/* clear the tag bits */
		tagmap_clrn((size_t)iov->iov_base, iov_tot);

		/* housekeeping */
		tot -= iov_tot;
	}
}

/* __NR_epoll_pwait post syscall hook */
static void
post_epoll_wait_hook(syscall_ctx_t *ctx)
{

	/* epoll_pwait() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1],
			sizeof(struct epoll_event) * (size_t)ctx->ret);
}

/* __NR_poll and __NR_ppoll post syscall hook */
static void
post_poll_hook(syscall_ctx_t *ctx)
{
	/* iterators */
	size_t	i;
	struct	pollfd *pfd;

	/* (p)poll() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* iterate the pollfd structures */
	for (i = 0; i < (size_t)ctx->arg[SYSCALL_ARG1]; i++) {
		/* get pollfd */
		pfd = ((struct pollfd *)ctx->arg[SYSCALL_ARG0]) + i;
	
		/* clear the tag bits */
		tagmap_clrn((size_t)&pfd->revents, sizeof(short));
	}
}

/* __NR_mq_timedreceive post syscall hook */
static void
post_mq_timedreceive_hook(syscall_ctx_t *ctx)
{
	/* mq_timedreceive() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
	
	/* priority argument is supplied */
	if ((size_t *)ctx->arg[SYSCALL_ARG3] != NULL)
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG3], sizeof(size_t));
}

/* __NR_get_mempolicy */
static void
post_get_mempolicy_hook(syscall_ctx_t *ctx)
{
	/* get_mempolicy() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;
	
	/* flags is zero */
	if ((unsigned long)ctx->arg[SYSCALL_ARG4] == 0) {
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG0], sizeof(int));
		tagmap_clrn(ctx->arg[SYSCALL_ARG1],
						sizeof(unsigned long));
		/* done */
		return;
	}

	/* MPOL_F_MEMS_ALLOWED is set on flags */
	if (((unsigned long)ctx->arg[SYSCALL_ARG4] &
				MPOL_F_MEMS_ALLOWED) != 0) {
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG1],
						sizeof(unsigned long));
		/* done */
		return;
	}
	
	/* MPOL_F_ADDR is set on flags */
	if (((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_ADDR) != 0 &&
		((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_NODE) == 0) {
		/* mode is provided */
		if ((int *)ctx->arg[SYSCALL_ARG0] != NULL)
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG0],
							sizeof(int));

		/* nodemask is provided */
		if ((unsigned long *)ctx->arg[SYSCALL_ARG1] != NULL)
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG1],
						sizeof(unsigned long));
		/* done */
		return;
	}
	
	/* MPOL_F_NODE & MPOL_F_ADDR is set on flags */
	if (((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_ADDR) != 0 && 
		((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_NODE) != 0) {
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG0], sizeof(int));
		/* done */
		return;
	}
	
	/* MPOL_F_NODE is set on flags */
	if (((unsigned long)ctx->arg[SYSCALL_ARG4] & MPOL_F_NODE) != 0) {
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG0], sizeof(int));
		/* done */
		return;
	}
}

/* __NR_lookup_dcookie post syscall hook */
static void
post_lookup_dcookie_hook(syscall_ctx_t *ctx)
{
	/* lookup_dcookie() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

/* __NR_io_getevents post syscall hook */
static void
post_io_getevents_hook(syscall_ctx_t *ctx)
{
	/* io_getevents() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG3],
				sizeof(struct io_event) * (size_t)ctx->ret);

	/* timespec is specified */
	if ((struct timespec *)ctx->arg[SYSCALL_ARG4] != NULL)
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG4],
						sizeof(struct timespec));
}

/* __NR_(f, l)listxattr post syscall hook */
static void
post_listxattr_hook(syscall_ctx_t *ctx)
{
	/* *listxattr() was not successful; optimized branch */
	if ((long)ctx->ret <= 0 || (void *)ctx->arg[SYSCALL_ARG1] == NULL)
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

/* __NR_(f, l)getxattr post syscall hook */
static void
post_getxattr_hook(syscall_ctx_t *ctx)
{
	/* *getxattr() was not successful; optimized branch */
	if ((long)ctx->ret <= 0 || (void *)ctx->arg[SYSCALL_ARG2] == NULL)
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG2], (size_t)ctx->ret);
}

/* __NR_getdents post syscall hook */
static void
post_getdents_hook(syscall_ctx_t *ctx)
{
	/* getdents() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

/* __NR_mincore post syscall hook */
static void
post_mincore_hook(syscall_ctx_t *ctx)
{
	/* mincore() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG2],
		(((size_t)ctx->arg[SYSCALL_ARG1] + PAGE_SZ - 1) / PAGE_SZ));
}

/* __NR_getcwd post syscall hook */
static void
post_getcwd_hook(syscall_ctx_t *ctx)
{
	/* getcwd() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG0], (size_t)ctx->ret);
}

/* __NR_rt_sigpending post syscall hook */
static void
post_rt_sigpending_hook(syscall_ctx_t *ctx)
{
	/* rt_sigpending() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG0], (size_t)ctx->arg[SYSCALL_ARG1]);
}

/* __NR_quotactl post syscall hook */
static void
post_quotactl_hook(syscall_ctx_t *ctx)
{
	/* offset */
	size_t off;

	/* quotactl() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;
	
	/* different offset ranges */
	switch ((int)ctx->arg[SYSCALL_ARG0]) {
		case Q_GETFMT:
			off = sizeof(__u32); 
			break;
		case Q_GETINFO:
			off = sizeof(struct if_dqinfo);
			break;
		case Q_GETQUOTA:
			off = sizeof(struct if_dqblk);
			break;
		case Q_XGETQSTAT:
			off = sizeof(struct fs_quota_stat);
			break;
		case Q_XGETQUOTA:
			off = sizeof(struct fs_disk_quota);
			break;
		default:
			/* nothing to do */
			return;
	}

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG3], off);
}

/* __NR_modify_ldt post syscall hook */
static void
post_modify_ldt_hook(syscall_ctx_t *ctx)
{
	/* modify_ldt() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* clear the tag bits */
	tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
}

/* __NR_nanosleep post syscall hook */
static void
post_nanosleep_hook(syscall_ctx_t *ctx)
{
        /* nanosleep() was successful; optimized branch */
        if (likely((long)ctx->ret == 0))
                return;

        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG1], sizeof(struct timespec));
}

/* __NR_getsockopt post syscall hook */
static void
post_getsockopt_hook(syscall_ctx_t *ctx)
{
        /* getsockopt() was successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;

        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG3],
                        *((int *)ctx->arg[SYSCALL_ARG4]));

        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG4], sizeof(int));
}

/* __NR_shmctl post syscall hook */
static void
post_shmctl_hook(syscall_ctx_t *ctx)
{
        /* shmctl() was not successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;

        /* differentiate based on the cmd */
        switch ((int)ctx->arg[SYSCALL_ARG1]) {
                case IPC_STAT:
                case SHM_STAT:
                        /* clear the tag bits */
                        tagmap_clrn(ctx->arg[SYSCALL_ARG2],
                                        sizeof(struct shmid_ds));
                        break;
                case IPC_INFO:
                case SHM_INFO:
                        /* clear the tag bits */
                        tagmap_clrn(ctx->arg[SYSCALL_ARG2],
                                        sizeof(struct shminfo));
                        break;
                default:
                        /* nothing to do */
                        return;
        }
}


/* __NR_semctl post syscall hook */
static void
post_semctl_hook(syscall_ctx_t *ctx)
{
	/* semaphore union */
	union semun *su;

        /* semctl() was not successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;

        /* get the semun structure */	
        su = (union semun *)ctx->arg[SYSCALL_ARG3];

        /* fix the cmd parameter */
//        ctx->arg[SYSCALL_ARG3] -= IPC_FIX;

        /* differentiate based on the cmd */
        switch ((int)ctx->arg[SYSCALL_ARG2]) {
                case IPC_STAT:
                case SEM_STAT:
                        /* clear the tag bits */
                        tagmap_clrn((size_t)su,
                                        sizeof(struct semid_ds));
                        break;
                case IPC_INFO:
                case SEM_INFO:
                        /* clear the tag bits */
                        tagmap_clrn((size_t)su,
                                        sizeof(struct seminfo));
                        break;
                default:
                        /* nothing to do */
                        return;
        }
}

/* __NR_msgctl post syscall hook */
static void
post_msgctl_hook(syscall_ctx_t *ctx)
{
        /* msgctl() was not successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;

        /* fix the cmd parameter */
//        ctx->arg[SYSCALL_ARG2] -= IPC_FIX;

        /* differentiate based on the cmd */
        switch ((int)ctx->arg[SYSCALL_ARG1]) {
                case IPC_STAT:
                case MSG_STAT:
                        /* clear the tag bits */
                        tagmap_clrn(ctx->arg[SYSCALL_ARG2],
                                        sizeof(struct msqid_ds));
                        break;
                case IPC_INFO:
                case MSG_INFO:
                        /* clear the tag bits */
                        tagmap_clrn(ctx->arg[SYSCALL_ARG2],
                                        sizeof(struct msginfo));
                        break;
                default:
                        /* nothing to do */
                        return;
        }
}

/* __NR_ipc post syscall hook */
static void
post_ipc_hook(syscall_ctx_t *ctx)
{
	/* semaphore union */
	union semun *su;

	/* ipc() is a demultiplexer for all SYSV IPC calls */
	switch ((int)ctx->arg[SYSCALL_ARG0]) {
		/* msgctl() */
		case MSGCTL:
			/* msgctl() was not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;
			
			/* fix the cmd parameter */
			ctx->arg[SYSCALL_ARG2] -= IPC_FIX;

			/* differentiate based on the cmd */
			switch ((int)ctx->arg[SYSCALL_ARG2]) {
				case IPC_STAT:
				case MSG_STAT:
					/* clear the tag bits */
					tagmap_clrn(ctx->arg[SYSCALL_ARG4],
						sizeof(struct msqid_ds));
					break;
				case IPC_INFO:
				case MSG_INFO:
					/* clear the tag bits */
					tagmap_clrn(ctx->arg[SYSCALL_ARG4],
						sizeof(struct msginfo));
					break;
				default:
					/* nothing to do */
					return;
			}
			break;
		/* shmctl() */
		case SHMCTL:
			/* shmctl() was not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;
			
			/* fix the cmd parameter */
			ctx->arg[SYSCALL_ARG2] -= IPC_FIX;

			/* differentiate based on the cmd */
			switch ((int)ctx->arg[SYSCALL_ARG2]) {
				case IPC_STAT:
				case SHM_STAT:
					/* clear the tag bits */
					tagmap_clrn(ctx->arg[SYSCALL_ARG4],
						sizeof(struct shmid_ds));
					break;
				case IPC_INFO:
				case SHM_INFO:
					/* clear the tag bits */
					tagmap_clrn(ctx->arg[SYSCALL_ARG4],
						sizeof(struct shminfo));
					break;
				default:
					/* nothing to do */
					return;
			}
			break;
		/* semctl() */
		case SEMCTL:
			/* semctl() was not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;
			
			/* get the semun structure */	
			su = (union semun *)ctx->arg[SYSCALL_ARG4];
			
			/* fix the cmd parameter */
			ctx->arg[SYSCALL_ARG3] -= IPC_FIX;

			/* differentiate based on the cmd */
			switch ((int)ctx->arg[SYSCALL_ARG3]) {
				case IPC_STAT:
				case SEM_STAT:
					/* clear the tag bits */
					tagmap_clrn((size_t)su->buf,
						sizeof(struct semid_ds));
					break;
				case IPC_INFO:
				case SEM_INFO:
					/* clear the tag bits */
					tagmap_clrn((size_t)su->buf,
						sizeof(struct seminfo));
					break;
				default:
					/* nothing to do */
					return;
			}
			break;
		/* msgrcv() */
		case MSGRCV:
			/* msgrcv() was not successful; optimized branch */
			if (unlikely((long)ctx->ret <= 0))
				return;
			
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG4],
					(size_t)ctx->ret + sizeof(long));
			break;
		default:
			/* nothing to do */
			return;
	}
}

/* __NR_fcntl post syscall hook */
static void
post_fcntl_hook(syscall_ctx_t *ctx)
{
	/* fcntl() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;
	
	/* differentiate based on the cmd argument */
	switch((int)ctx->arg[SYSCALL_ARG1]) {
		/* F_GETLK */
		case F_GETLK:
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG2],
					sizeof(struct flock));
			break;
		/* F_GETLK64 */
		case F_GETLK64:
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG2],
					sizeof(struct flock64));
			break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,32)
		/* F_GETOWN_EX */
		case F_GETOWN_EX:
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG2],
					sizeof(struct f_owner_ex));
			break;
#endif
		default:
			/* nothing to do */
			break;
	}
}

/* __NR_accept post syscall hook */
static void
post_accept_hook(syscall_ctx_t *ctx)
{
        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;

        /* addr argument is provided */
        if ((void *)ctx->arg[SYSCALL_ARG1] != NULL) {
                /* clear the tag bits */
                tagmap_clrn(ctx->arg[SYSCALL_ARG1],
                                *((int *)ctx->arg[SYSCALL_ARG2]));

                /* clear the tag bits */
                tagmap_clrn(ctx->arg[SYSCALL_ARG2], sizeof(int));
        }

}

/* __NR_msgrecv post syscall hook */
static void
post_msgrcv_hook(syscall_ctx_t *ctx)
{
        struct msgbuf *msgp;
        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret <= 0))
                return;

        msgp = (struct msgbuf *) ctx->arg[SYSCALL_ARG1];

        /* clear the tag bits */
        tagmap_clrn((size_t)msgp->mtext, (size_t)ctx->ret);
}

/* __NR_recvfrom post syscall hook */
static void
post_recvfrom_hook(syscall_ctx_t *ctx)
{
        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret <= 0))
                return;

        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);

        /* sockaddr argument is specified */
        if ((void *)ctx->arg[SYSCALL_ARG4] != NULL) {
                /* clear the tag bits */
                tagmap_clrn(ctx->arg[SYSCALL_ARG4],
                                *((int *)ctx->arg[SYSCALL_ARG5]));

                /* clear the tag bits */
                tagmap_clrn(ctx->arg[SYSCALL_ARG5], sizeof(int));
        }
}

/* __NR_sendmmsg post syscall hook */
static void
post_sendmmsg_hook(syscall_ctx_t *ctx)
{
	/* message headers; sendmmsg(2) */
	struct	mmsghdr *msg;

	/* iterators */
	size_t	i;

	/* sendmmsg() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;
	
	/* iterate the mmsghdr structures */
	for (i = 0; i < (size_t)ctx->ret; i++) {
		/* get the next mmsghdr structure */
		msg = ((struct mmsghdr *)ctx->arg[SYSCALL_ARG1]) + i;
	
                /* clear the tag bits */
		tagmap_clrn((size_t)&msg->msg_len, sizeof(unsigned int));
        }
}
/* __NR_recvmsg post syscall hook */
static void
post_recvmsg_hook(syscall_ctx_t *ctx)
{
	/* message header; recvmsg(2) */
	struct	msghdr *msg;

	/* iov bytes copied; recvmsg(2) */
	size_t	iov_tot;

	/* iterators */
	size_t	i;
	struct	iovec *iov;
	
	/* total bytes received */
	size_t	tot;
	
        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret <= 0))
                return;

        /* extract the message header */
        msg = (struct msghdr *)ctx->arg[SYSCALL_ARG1];

        /* source address specified */
        if (msg->msg_name != NULL) {
                /* clear the tag bits */
                tagmap_clrn((size_t)msg->msg_name,
                                msg->msg_namelen);

                /* clear the tag bits */
                tagmap_clrn((size_t)&msg->msg_namelen,
                                sizeof(int));
        }

        /* ancillary data specified */
        if (msg->msg_control != NULL) {
                /* clear the tag bits */
                tagmap_clrn((size_t)msg->msg_control,
                                msg->msg_controllen);

                /* clear the tag bits */
                tagmap_clrn((size_t)&msg->msg_controllen,
                                sizeof(int));
        }

        /* flags; clear the tag bits */
        tagmap_clrn((size_t)&msg->msg_flags, sizeof(int));

        /* total bytes received */	
        tot = (size_t)ctx->ret;

        /* iterate the iovec structures */
        for (i = 0; i < msg->msg_iovlen && tot > 0; i++) {
                /* get the next I/O vector */
                iov = &msg->msg_iov[i];

                /* get the length of the iovec */
                iov_tot = (tot > (size_t)iov->iov_len) ?
                        (size_t)iov->iov_len : tot;

                /* clear the tag bits */
                tagmap_clrn((size_t)iov->iov_base, iov_tot);

                /* housekeeping */
                tot -= iov_tot;
        }
}

/* __NR_socketcall post syscall hook */
static void
post_socketpair_hook(syscall_ctx_t *ctx)
{
        /* not successful; optimized branch */
        if (unlikely((long)ctx->ret < 0))
                return;

        /* clear the tag bits */
        tagmap_clrn(ctx->arg[SYSCALL_ARG3], (sizeof(int) * 2));
}

/* __NR_socketcall post syscall hook */
static void
post_socketcall_hook(syscall_ctx_t *ctx)
{
	/* message header; recvmsg(2) */
	struct	msghdr *msg;

	/* iov bytes copied; recvmsg(2) */
	size_t	iov_tot;

	/* iterators */
	size_t	i;
	struct	iovec *iov;
	
	/* total bytes received */
	size_t	tot;
	
	/* socket call arguments */
	unsigned long	*args = (unsigned long *)ctx->arg[SYSCALL_ARG1];

	/* demultiplex the socketcall */
	switch ((int)ctx->arg[SYSCALL_ARG0]) {
		case SYS_ACCEPT:
		case SYS_ACCEPT4:
		case SYS_GETSOCKNAME:
		case SYS_GETPEERNAME:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;

			/* addr argument is provided */
			if ((void *)args[SYSCALL_ARG1] != NULL) {
				/* clear the tag bits */
				tagmap_clrn(args[SYSCALL_ARG1],
					*((int *)args[SYSCALL_ARG2]));
				
				/* clear the tag bits */
				tagmap_clrn(args[SYSCALL_ARG2], sizeof(int));
			}
			break;
		case SYS_SOCKETPAIR:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;
	
			/* clear the tag bits */
			tagmap_clrn(args[SYSCALL_ARG3], (sizeof(int) * 2));
			break;
		case SYS_RECV:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret <= 0))
				return;
	
			/* clear the tag bits */
			tagmap_clrn(args[SYSCALL_ARG1], (size_t)ctx->ret);
			break;
		case SYS_RECVFROM:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret <= 0))
				return;
	
			/* clear the tag bits */
			tagmap_clrn(args[SYSCALL_ARG1], (size_t)ctx->ret);

			/* sockaddr argument is specified */
			if ((void *)args[SYSCALL_ARG4] != NULL) {
				/* clear the tag bits */
				tagmap_clrn(args[SYSCALL_ARG4],
					*((int *)args[SYSCALL_ARG5]));
				
				/* clear the tag bits */
				tagmap_clrn(args[SYSCALL_ARG5], sizeof(int));
			}
			break;
		case SYS_GETSOCKOPT:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret < 0))
				return;
	
			/* clear the tag bits */
			tagmap_clrn(args[SYSCALL_ARG3],
					*((int *)args[SYSCALL_ARG4]));
			
			/* clear the tag bits */
			tagmap_clrn(args[SYSCALL_ARG4], sizeof(int));
			break;
		case SYS_RECVMSG:
			/* not successful; optimized branch */
			if (unlikely((long)ctx->ret <= 0))
				return;

			/* extract the message header */
			msg = (struct msghdr *)args[SYSCALL_ARG1];

			/* source address specified */
			if (msg->msg_name != NULL) {
				/* clear the tag bits */
				tagmap_clrn((size_t)msg->msg_name,
					msg->msg_namelen);
				
				/* clear the tag bits */
				tagmap_clrn((size_t)&msg->msg_namelen,
						sizeof(int));
			}
			
			/* ancillary data specified */
			if (msg->msg_control != NULL) {
				/* clear the tag bits */
				tagmap_clrn((size_t)msg->msg_control,
					msg->msg_controllen);
				
				/* clear the tag bits */
				tagmap_clrn((size_t)&msg->msg_controllen,
						sizeof(int));
			}

			/* flags; clear the tag bits */
			tagmap_clrn((size_t)&msg->msg_flags, sizeof(int));

			/* total bytes received */	
			tot = (size_t)ctx->ret;

			/* iterate the iovec structures */
			for (i = 0; i < msg->msg_iovlen && tot > 0; i++) {
				/* get the next I/O vector */
				iov = &msg->msg_iov[i];

				/* get the length of the iovec */
				iov_tot = (tot > (size_t)iov->iov_len) ?
						(size_t)iov->iov_len : tot;
	
				/* clear the tag bits */
				tagmap_clrn((size_t)iov->iov_base, iov_tot);
		
				/* housekeeping */
				tot -= iov_tot;
			}
			break;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
		case SYS_RECVMMSG:
			/* fix the syscall context */
			ctx->arg[SYSCALL_ARG0] = args[SYSCALL_ARG0];
			ctx->arg[SYSCALL_ARG1] = args[SYSCALL_ARG1];
			ctx->arg[SYSCALL_ARG2] = args[SYSCALL_ARG2];
			ctx->arg[SYSCALL_ARG3] = args[SYSCALL_ARG3];
			ctx->arg[SYSCALL_ARG4] = args[SYSCALL_ARG4];

			/* invoke __NR_recvmmsg post syscall hook */
			post_recvmmsg_hook(ctx);
			break;
#endif
		default:
			/* nothing to do */
			return;
	}
}

/* 
 * __NR_syslog post syscall hook
 *
 * NOTE: this is not related to syslog(3)
 * see klogctl(3)/syslog(2)
 */
static void
post_syslog_hook(syscall_ctx_t *ctx)
{
	/* syslog() was not successful; optimized branch */
	if (unlikely((long)ctx->ret <= 0))
		return;

	/* differentiate based on the type */
	switch ((int)ctx->arg[SYSCALL_ARG0]) {
		case 2:
		case 3:
		case 4:
			/* clear the tag bits */
			tagmap_clrn(ctx->arg[SYSCALL_ARG1], (size_t)ctx->ret);
			break;
		default:
			/* nothing to do */
			return;
	}
}

/* __NR__sysctl post syscall hook */
static void
post__sysctl_hook(syscall_ctx_t *ctx)
{
	/* _sysctl arguments */
	struct __sysctl_args *sa;

	/* _sysctl() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;

	/* _sysctl arguments */
	sa = (struct __sysctl_args *)ctx->arg[SYSCALL_ARG0];

	/* clear the tag bits */
	tagmap_clrn((size_t)sa->newval, sa->newlen);

	/* save old value is specified */
	if (sa->oldval != NULL) {
		/* clear the tag bits */
		tagmap_clrn((size_t)sa->oldval, *sa->oldlenp);
		
		/* clear the tag bits */
		tagmap_clrn((size_t)sa->oldlenp, sizeof(size_t));
	}
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,33)
/* __NR_recvmmsg post syscall hook */
static void
post_recvmmsg_hook(syscall_ctx_t *ctx)
{
	/* message headers; recvmsg(2) recvmmsg(2) */
	struct	mmsghdr *msg;
	struct	msghdr *m;

	/* iov bytes copied; recvmsg(2) */
	size_t	iov_tot;

	/* iterators */
	size_t	i, j;
	struct	iovec *iov;
	
	/* total bytes received */
	size_t	tot;
	
	/* recvmmsg() was not successful; optimized branch */
	if (unlikely((long)ctx->ret < 0))
		return;
	
	/* iterate the mmsghdr structures */
	for (i = 0; i < (size_t)ctx->ret; i++) {
		/* get the next mmsghdr structure */
		msg = ((struct mmsghdr *)ctx->arg[SYSCALL_ARG1]) + i;
	
		/* extract the message header */
		m = &msg->msg_hdr;

		/* source address specified */
		if (m->msg_name != NULL) {
			/* clear the tag bits */
			tagmap_clrn((size_t)m->msg_name, m->msg_namelen);
			
			/* clear the tag bits */
			tagmap_clrn((size_t)&m->msg_namelen, sizeof(int));
		}
			
		/* ancillary data specified */
		if (m->msg_control != NULL) {
			/* clear the tag bits */
			tagmap_clrn((size_t)m->msg_control, m->msg_controllen);
				
			/* clear the tag bits */
			tagmap_clrn((size_t)&m->msg_controllen, sizeof(int));
		}

		/* flags; clear the tag bits */
		tagmap_clrn((size_t)&m->msg_flags, sizeof(int));
		
		/* total bytes received; clear the tag bits */	
		tot = (size_t)msg->msg_len;
		tagmap_clrn((size_t)&msg->msg_len, sizeof(unsigned));
		
		/* iterate the iovec structures */
		for (j = 0; j < m->msg_iovlen && tot > 0; j++) {
			/* get the next I/O vector */
			iov = &m->msg_iov[j];

			/* get the length of the iovec */
			iov_tot = (tot > (size_t)iov->iov_len) ?
					(size_t)iov->iov_len : tot;
	
			/* clear the tag bits */
			tagmap_clrn((size_t)iov->iov_base, iov_tot);
	
			/* housekeeping */
			tot -= iov_tot;
		}
	}

	/* timespec structure specified */
	if ((struct timespec *)ctx->arg[SYSCALL_ARG4] != NULL)
		/* clear the tag bits */
		tagmap_clrn(ctx->arg[SYSCALL_ARG4], sizeof(struct timespec));
}

#endif
