/*
 * lxc: linux Container library
 *
 * (C) Copyright IBM Corp. 2007, 2008
 *
 * Authors:
 * Daniel Lezcano <daniel.lezcano at free.fr>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#define _GNU_SOURCE
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>

#include "log.h"

lxc_log_define(lxc_utils, lxc);

static int mount_fs(const char *source, const char *target, const char *type)
{
	/* the umount may fail */
	if (umount(target))
		WARN("failed to unmount %s : %s", target, strerror(errno));

	if (mount(source, target, type, 0, NULL)) {
		ERROR("failed to mount %s : %s", target, strerror(errno));
		return -1;
	}

	DEBUG("'%s' mounted on '%s'", source, target);

	return 0;
}

extern int lxc_setup_fs(void)
{
	if (mount_fs("proc", "/proc", "proc"))
		return -1;

	/* if we can't mount /dev/shm, continue anyway */
	if (mount_fs("shmfs", "/dev/shm", "tmpfs"))
		INFO("failed to mount /dev/shm");

	/* If we were able to mount /dev/shm, then /dev exists */
	/* Sure, but it's read-only per config :) */
	if (access("/dev/mqueue", F_OK) && mkdir("/dev/mqueue", 0666)) {
		DEBUG("failed to create '/dev/mqueue'");
		return 0;
	}

	if (mount_fs("mqueue", "/dev/mqueue", "mqueue"))
		return -1;

	return 0;
}

/* borrowed from iproute2 */
extern int get_u16(unsigned short *val, const char *arg, int base)
{
	unsigned long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;

	res = strtoul(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > 0xFFFF)
		return -1;

	*val = res;

	return 0;
}

static int is_all_slashes(char *path)
{
	while (*path && *path == '/')
		path++;
	if (*path)
		return 0;
	return 1;
}

extern int mkdir_p(char *dir, mode_t mode)
{
        int ret;
        char *d;

	if (is_all_slashes(dir))
                return 0;

        d = strdup(dir);
        if (!d)
                return -1;

        ret = mkdir_p(dirname(d), mode);
        free(d);
        if (ret)
                return -1;

        if (!access(dir, F_OK))
                return 0;

        if (mkdir(dir, mode)) {
                SYSERROR("failed to create directory '%s'\n", dir);
                return -1;
        }

        return 0;
}

static char *copypath(char *p)
{
	int len = strlen(p);
	char *retbuf;

	if (len < 1)
		return NULL;
	if (p[len-1] == '\n') {
		p[len-1] = '\0';
		len--;
	}
	retbuf = malloc(len+1);
	if (!retbuf)
		return NULL;
	strcpy(retbuf, p);
	return retbuf;
}

char *default_lxcpath;

const char *default_lxc_path(void)
{
	char buf[1024], *p;
	FILE *fin;

	if (default_lxcpath)
		return default_lxcpath;

	fin = fopen(LXC_GLOBAL_CONF, "r");
	if (fin) {
		while (fgets(buf, 1024, fin)) {
			if (buf[0] == '#')
				continue;
			p = strstr(buf, "lxcpath");
			if (!p)
				continue;
			p = strchr(p, '=');
			if (!p)
				continue;
			p++;
			while (*p && (*p == ' ' || *p == '\t')) p++;
			if (!*p)
				continue;
			default_lxcpath = copypath(p);
			goto out;
		}
	}
	/* we couldn't open the file, or didn't find a lxcpath
	 * entry there.  Return @LXCPATH@ */
	default_lxcpath = LXCPATH;

out:
	if (fin)
		fclose(fin);
	return default_lxcpath;
}
