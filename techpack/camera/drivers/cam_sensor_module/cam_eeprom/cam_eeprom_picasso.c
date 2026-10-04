// SPDX-License-Identifier: GPL-2.0-only
/* Assemble the ASUS userspace calibration layout without changing EEPROM. */

#include <linux/ctype.h>
#include <linux/compiler.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>

#include "cam_debug_util.h"
#include "cam_eeprom_dev.h"
#include "cam_eeprom_picasso.h"

#define PICASSO_EEPROM_MAX_SIZE 8192
#define PICASSO_DIT_HEADER_SIZE 0x471
#define PICASSO_AF_OFFSET 0x45d
#define PICASSO_AF_CHECK 0x465
#define PICASSO_MODULE_ID_OFFSET 8
#define PICASSO_SERIAL_OFFSET 10
#define PICASSO_SERIAL_BYTES 12

/* Unknown, failed or replaced modules must not receive old gyro calibration. */
static bool picasso_imx686_factory_allowed;

static bool picasso_is_main_eeprom(struct cam_eeprom_ctrl_t *e_ctrl)
{
	return e_ctrl && e_ctrl->soc_info.dev &&
		e_ctrl->soc_info.index == 0 &&
		of_device_is_compatible(e_ctrl->soc_info.dev->of_node,
					"asus,picasso-eeprom");
}

void cam_eeprom_picasso_invalidate(struct cam_eeprom_ctrl_t *e_ctrl)
{
	if (picasso_is_main_eeprom(e_ctrl))
		WRITE_ONCE(picasso_imx686_factory_allowed, false);
}

bool cam_eeprom_picasso_ois_calibration_allowed(void)
{
	return READ_ONCE(picasso_imx686_factory_allowed);
}

struct picasso_calibration_layout {
	u8 module_id;
	const char *name;
	u16 template_min_size;
	bool has_af;
	u16 lrc_start;
	u16 lrc_size;
	u16 pdaf_start;
	u16 pdaf_size;
	u16 remosaic_start;
	u16 remosaic_size;
};

/* Offsets and lengths are the shipping ASUS EEPROM-to-DIT layout. */
static const struct picasso_calibration_layout picasso_layouts[] = {
	{ 0x6b, "IMX686_P", 0x471, true,
		0x21, 0x201, 0x222, 0x3e1, 0x821, 0xbd2 },
	{ 0x6c, "IMX363_P", 0x471, true,
		0, 0, 0x21, 0x3e1, 0, 0 },
	{ 0x6e, "OV08A10_P", 0x471, true,
		0, 0, 0x21, 0x3e1, 0, 0 },
	{ 0x72, "OV24B1Q_P", 0x45d, false,
		0, 0, 0, 0, 0, 0xbd2 },
};

static bool picasso_range_fits(size_t size, size_t offset, size_t length)
{
	return offset <= size && length <= size - offset;
}

/* Only fixed, module-specific calibration paths reach this reader. */
static int picasso_read_file(const char *path, u8 *buffer, size_t capacity,
			     size_t minimum, bool prefix)
{
	struct file *file;
	loff_t file_size, position = 0;
	size_t length, done = 0;
	ssize_t count;
	int rc;

	file = filp_open(path, O_RDONLY | O_NOFOLLOW, 0);
	if (IS_ERR(file))
		return PTR_ERR(file);

	if (!S_ISREG(file_inode(file)->i_mode)) {
		rc = -EINVAL;
		goto close_file;
	}

	file_size = i_size_read(file_inode(file));
	if (file_size < 0 || file_size < minimum) {
		rc = -EINVAL;
		goto close_file;
	}
	if (!prefix && file_size > capacity) {
		rc = -EFBIG;
		goto close_file;
	}
	length = min_t(loff_t, file_size, capacity);
	while (done < length) {
		count = kernel_read(file, buffer + done, length - done,
				    &position);
		if (count <= 0) {
			rc = count < 0 ? count : -EIO;
			goto close_file;
		}
		done += count;
	}
	if (i_size_read(file_inode(file)) != file_size) {
		rc = -EIO;
		goto close_file;
	}
	rc = done;

close_file:
	filp_close(file, NULL);
	return rc;
}

static bool picasso_use_factory(const struct picasso_calibration_layout *layout,
				const u8 *raw)
{
	char path[80];
	u8 serial[128];
	size_t start = 0, i;
	int length, high, low;

	snprintf(path, sizeof(path), "/vendor/factory/cali_%s.txt",
		 layout->name);
	length = picasso_read_file(path, serial, sizeof(serial),
				   PICASSO_SERIAL_BYTES * 2, true);
	/* An absent serial record did not disable factory data in ASUS code. */
	if (length == -ENOENT)
		return true;
	if (length < 0)
		return false;

	while (start < length && isspace(serial[start]))
		start++;
	if (length - start < PICASSO_SERIAL_BYTES * 2)
		return false;

	for (i = 0; i < PICASSO_SERIAL_BYTES; i++) {
		high = hex_to_bin(serial[start + i * 2]);
		low = hex_to_bin(serial[start + i * 2 + 1]);
		if (high < 0 || low < 0 ||
		    ((high << 4) | low) != raw[PICASSO_SERIAL_OFFSET + i])
			return false;
	}
	start += PICASSO_SERIAL_BYTES * 2;
	return start == length || isspace(serial[start]);
}

static int picasso_validate_layout(const struct picasso_calibration_layout *layout,
				   size_t size)
{
	size_t output_size = PICASSO_DIT_HEADER_SIZE + layout->lrc_size +
		layout->pdaf_size + layout->remosaic_size;

	if (size > PICASSO_EEPROM_MAX_SIZE || size < output_size ||
	    !picasso_range_fits(size, layout->lrc_start, layout->lrc_size) ||
	    !picasso_range_fits(size, layout->pdaf_start, layout->pdaf_size) ||
	    !picasso_range_fits(size, layout->remosaic_start,
				layout->remosaic_size))
		return -EINVAL;
	return 0;
}

static void picasso_assemble(const struct picasso_calibration_layout *layout,
			     const u8 *raw, u8 *output)
{
	static const u8 golden_af[] = { 0x04, 0x07, 0x04, 0x87, 0x05, 0xd4 };
	const u8 *af = golden_af;
	size_t position = PICASSO_DIT_HEADER_SIZE;
	unsigned int i;

	if (layout->has_af) {
		for (i = 0; i <= 6; i += 2) {
			if (raw[i] != 0xff) {
				af = raw;
				break;
			}
		}
	}
	if (!output[PICASSO_AF_CHECK] && !output[PICASSO_AF_CHECK + 1]) {
		output[PICASSO_AF_OFFSET] = af[1];
		output[PICASSO_AF_OFFSET + 1] = af[0];
		output[PICASSO_AF_OFFSET + 2] = af[5];
		output[PICASSO_AF_OFFSET + 3] = af[4];
	}

	memcpy(output + position, raw + layout->lrc_start, layout->lrc_size);
	position += layout->lrc_size;
	memcpy(output + position, raw + layout->pdaf_start, layout->pdaf_size);
	position += layout->pdaf_size;
	memcpy(output + position, raw + layout->remosaic_start,
	       layout->remosaic_size);
}

int cam_eeprom_picasso_prepare(struct cam_eeprom_ctrl_t *e_ctrl)
{
	const struct picasso_calibration_layout *layout = NULL;
	struct cam_eeprom_memory_block_t *cal = &e_ctrl->cal_data;
	u8 *assembled;
	char path[80];
	bool factory = false, identity_matches;
	unsigned int i;
	int rc;

	cam_eeprom_picasso_invalidate(e_ctrl);
	if (!of_device_is_compatible(e_ctrl->soc_info.dev->of_node,
				     "asus,picasso-eeprom"))
		return 0;
	if (!cal->mapdata || cal->num_data <= PICASSO_MODULE_ID_OFFSET)
		return -EINVAL;

	for (i = 0; i < ARRAY_SIZE(picasso_layouts); i++) {
		if (picasso_layouts[i].module_id ==
			cal->mapdata[PICASSO_MODULE_ID_OFFSET]) {
			layout = &picasso_layouts[i];
			break;
		}
	}
	/* Preserve the original pass-through for unrecognized module IDs. */
	if (!layout) {
		CAM_WARN(CAM_EEPROM, "No Picasso layout for module 0x%02x",
			 cal->mapdata[PICASSO_MODULE_ID_OFFSET]);
		return 0;
	}
	rc = picasso_validate_layout(layout, cal->num_data);
	if (rc)
		return rc;

	/* Keep the fresh EEPROM bytes intact until the complete layout is ready. */
	assembled = kmemdup(cal->mapdata, cal->num_data, GFP_KERNEL);
	if (!assembled)
		return -ENOMEM;

	identity_matches = picasso_use_factory(layout, cal->mapdata);
	rc = -ENOENT;
	if (identity_matches) {
		snprintf(path, sizeof(path), "/vendor/factory/dut_%s.bin",
			 layout->name);
		rc = picasso_read_file(path, assembled, cal->num_data,
				       layout->template_min_size, false);
		factory = rc >= 0;
	}
	if (!factory) {
		/* Discard any partial factory read before trying the golden file. */
		memcpy(assembled, cal->mapdata, cal->num_data);
		snprintf(path, sizeof(path), "/vendor/lib64/camera/dut_%s.bin",
			 layout->name);
		rc = picasso_read_file(path, assembled, cal->num_data,
				       layout->template_min_size, false);
	}
	if (rc < 0) {
		CAM_ERR(CAM_EEPROM, "Picasso module 0x%02x template read failed: %d",
			layout->module_id, rc);
		goto free_buffer;
	}

	picasso_assemble(layout, cal->mapdata, assembled);
	if (picasso_is_main_eeprom(e_ctrl) && layout->module_id == 0x6b)
		WRITE_ONCE(picasso_imx686_factory_allowed, identity_matches);
	memcpy(cal->mapdata, assembled, cal->num_data);
	CAM_INFO(CAM_EEPROM, "Picasso module 0x%02x calibration assembled (%s)",
		 layout->module_id, factory ? "factory" : "golden");
	rc = 0;

free_buffer:
	kfree(assembled);
	return rc;
}

MODULE_IMPORT_NS(ANDROID_GKI_VFS_EXPORT_ONLY);
/* fs/Makefile remaps the export; MODULE_IMPORT_NS stringifies literally. */
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
