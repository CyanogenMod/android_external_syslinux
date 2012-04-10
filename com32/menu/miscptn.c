
#include <com32.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslinux/rawio.h>

#include "miscptn.h"

#define error(...) fprintf(stderr, __VA_ARGS__)

/* Persistent area written by recovery mechanism for communication
 * with the bootloader */
struct bootloader_message {
    char command[32];
    char status[32];
    char recovery[1024];
};

/* Struct to write to misc partition to tell bootloader that it needs
 * to boot into the specified label on next reboot. This is always
 * oneshot; bootloader should clear this on each boot, and should
 * gracefully handle it containing garbage.
 *
 * This is separate from the recovery Bootloader Control Block, which
 * is written at offset 0 and is persistent until specifically cleared */
struct bootloader_oneshot_cmd {
    char boot_target[32];
};

/* Offset in misc partition to read/write command */
#define BOOTLOADER_ONESHOT_SECTOR_OFFSET   4

static struct bootloader_message *get_bootloader_message(
	    struct disk_part_iter *cur_part)
{
    struct bootloader_message *msg;

    msg = rawio_read_sectors(cur_part->lba_data, size_to_sectors(sizeof(*msg)));
    if (!msg) {
	error("rawio_read_sectors failed to get bootloader_message");
	return NULL;
    }
    /* Null terminate everything, could be garbage */
    msg->command[sizeof(msg->command) - 1] = '\0';
    msg->status[sizeof(msg->status) - 1] = '\0';
    msg->recovery[sizeof(msg->recovery) - 1] = '\0';

    printf("command: '%s'", msg->command);

    return msg;
}

static struct bootloader_oneshot_cmd *get_oneshot_boot_target(
	    struct disk_part_iter *cur_part)
{
    struct bootloader_oneshot_cmd *cmd = NULL;
    char *data = NULL;
    uint64_t lba;

    lba = cur_part->lba_data + BOOTLOADER_ONESHOT_SECTOR_OFFSET;
    data = rawio_read_sectors(lba, 1);
    if (!data) {
	error("rawio_read_sectors failed to get oneshot target");
	goto bail;
    }

    cmd = malloc(sizeof(*cmd));
    if (!cmd)
	goto bail;
    memcpy(cmd, data, sizeof(*cmd));

    /* Could be garbage; ensure it's null terminated */
    cmd->boot_target[sizeof(cmd->boot_target) - 1] = '\0';

    /* Empty string means no command; return NULL */
    if (cmd->boot_target[0] == '\0') {
	free(cmd);
	cmd = NULL;
    }
    /* Historical: 'reboot bootloader' means enter Fastboot mode
     * (which is not implemented in the bootloader but another boot
     * image) */
    if (!strcmp(cmd->boot_target, "bootloader"))
        strncpy(cmd->boot_target, "fastboot", sizeof(cmd->boot_target));

    /* This mechanism is oneshot; unconditionally zero it out */
    memset(data, 0, sizeof(*cmd));
    rawio_write_sector(lba, data);
bail:
    if (data)
	free(data);
    return cmd;
}

char *read_misc_partition(int disk, int partnum)
{
    struct disk_part_iter *cur_part;
    struct bootloader_oneshot_cmd *oneshot_cmd = NULL;
    struct bootloader_message *bcb = NULL;
    char *boot_target = NULL;

    cur_part = rawio_get_partition(disk, partnum);
    if (!cur_part) {
	error("rawio_get_partition(%d,%d) failed\n", disk, partnum);
	return NULL;
    }

    /* Try reading bootloader control block first */
    bcb = get_bootloader_message(cur_part);
    if (bcb && !strncmp(bcb->command, "boot-", 5)) {
	boot_target = strdup(bcb->command + 5);
	goto out;
    }

    /* Failing that, check for a oneshot boot target */
    oneshot_cmd = get_oneshot_boot_target(cur_part);
    if (oneshot_cmd)
	boot_target = strdup(oneshot_cmd->boot_target);

out:
    if (oneshot_cmd)
	free(oneshot_cmd);
    if (bcb)
	free(bcb);
    if (cur_part) {
	free(cur_part->block);
	free(cur_part);
    }
    return boot_target;
}
