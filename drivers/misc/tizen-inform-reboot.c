/*
 * Tizen reboot parameter passing notifier
 *
 * Written by: Junghoon Kim <jhoon20.kim@samsung.com>
 *
 * Copyright (C) 2017 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>

static int inform_reboot_notifier(struct notifier_block *nb,
						unsigned long val, void *buf)
{
	char *cmd = buf;
	char *filename = CONFIG_TIZEN_INFORM_PATH;
	struct file *file;
	int fd;
	loff_t pos = 0;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	fd = sys_open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);
	if (fd >= 0) {
		file = fget(fd);
		if (file) {
			struct super_block *sb = file->f_path.dentry->d_sb;

			if (cmd) {
				if (!strncmp(cmd, "fota", 4))
					cmd = "upgr";
				else if (!strncmp(cmd, "recovery", 8))
					cmd = "rcvr";
				else if (!strncmp(cmd, "download", 8))
					cmd = "dwnl";
				else
					cmd = "ndef";
			} else
				cmd = "norm";

			vfs_write(file, cmd, strlen(cmd), &pos);

			down_read(&sb->s_umount);
			sync_filesystem(sb);
			up_read(&sb->s_umount);

			fput(file);
		}
		sys_close(fd);
	} else {
		pr_err("Reboot parameter passing is failed.\n"
				"Inform file path should be described correctly in config.\n");
	}

	set_fs(old_fs);

	return NOTIFY_DONE;
}

static struct notifier_block nb_inform_reboot_block = {
	.notifier_call = inform_reboot_notifier,
	.priority = 256,
};

static int __init inform_reboot_init(void)
{
	/* to support reboot parameter passing */
	register_reboot_notifier(&nb_inform_reboot_block);
	return 0;
}

subsys_initcall(inform_reboot_init);
