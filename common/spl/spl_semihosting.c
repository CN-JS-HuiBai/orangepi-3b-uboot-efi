// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2022 Sean Anderson <sean.anderson@seco.com>
 */

#include <common.h>
#include <image.h>
#include <log.h>
#include <semihosting.h>
#include <spl.h>

static int smh_read_full(long fd, void *memp, size_t len)
{
	long read;

	read = smh_read(fd, memp, len);
	if (read < 0)
		return read;
	if (read != len)
		return -EIO;
	return 0;
}

static ulong smh_fit_read(struct spl_load_info *load, ulong file_offset,
			  ulong size, void *buf)
{
	long fd;
	ulong ret;

	fd = smh_open(load->filename, MODE_READ | MODE_BINARY);
	if (fd < 0) {
		log_debug("could not open %s: %ld\n", load->filename, fd);
		return 0;
	}
	ret = smh_read(fd, buf, size);
	smh_close(fd);

	return ret;
}

static int spl_smh_load_image(struct spl_image_info *spl_image,
			      struct spl_boot_device *bootdev)
{
	const char *filename = CONFIG_SPL_FS_LOAD_PAYLOAD_NAME;
	int ret;
	long fd, len;
	struct legacy_img_hdr *header =
		spl_get_load_buffer(-sizeof(*header), sizeof(*header));

	fd = smh_open(filename, MODE_READ | MODE_BINARY);
	if (fd < 0) {
		log_debug("could not open %s: %ld\n", filename, fd);
		return fd;
	}

	ret = smh_flen(fd);
	if (ret < 0) {
		log_debug("could not get length of image: %d\n", ret);
		goto out;
	}
	len = ret;

	ret = smh_read_full(fd, header, sizeof(struct legacy_img_hdr));
	if (ret) {
		log_debug("could not read image header: %d\n", ret);
		goto out;
	}

	if (IS_ENABLED(CONFIG_SPL_LOAD_FIT) &&
	    image_get_magic(header) == FDT_MAGIC) {
		struct spl_load_info load;

		debug("Found FIT\n");
		load.read = smh_fit_read;
		load.bl_len = 1;
		load.filename = filename;
		load.priv = NULL;
		smh_close(fd);

		return spl_load_simple_fit(spl_image, &load, 0, header);
	}

	ret = spl_parse_image_header(spl_image, bootdev, header);
	if (ret) {
		log_debug("failed to parse image header: %d\n", ret);
		goto out;
	}

	ret = smh_seek(fd, 0);
	if (ret) {
		log_debug("could not seek to start of image: %d\n", ret);
		goto out;
	}

	ret = smh_read_full(fd, (void *)spl_image->load_addr, len);
	if (ret)
		log_debug("could not read %s: %d\n", filename, ret);
out:
	smh_close(fd);
	return ret;
}
SPL_LOAD_IMAGE_METHOD("SEMIHOSTING", 0, BOOT_DEVICE_SMH, spl_smh_load_image);
