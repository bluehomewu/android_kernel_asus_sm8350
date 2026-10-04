// SPDX-License-Identifier: GPL-2.0-only
/* Read the optional, module-matched main-camera gyro gains without writes. */

#include <linux/ctype.h>
#include <linux/fs.h>
#include <linux/string.h>

#include "cam_eeprom_picasso.h"
#include "cam_ois_dev.h"
#include "cam_ois_picasso.h"

#define PICASSO_GYRO_GAIN_PATH "/vendor/factory/ois_gyro_gain_cali.txt"
#define PICASSO_GYRO_GAIN_MAX_SIZE 254

static int picasso_parse_gain(const char **cursor, const char *end,
			      const char *prefix, u32 *gain)
{
	const char *pos = *cursor;
	size_t prefix_len = strlen(prefix);
	unsigned int digits = 0;
	u32 value = 0;
	int digit;

	if (end - pos < prefix_len || memcmp(pos, prefix, prefix_len))
		return -EINVAL;
	pos += prefix_len;
	while (pos < end && isxdigit(*pos)) {
		if (++digits > 8)
			return -ERANGE;
		digit = hex_to_bin(*pos++);
		value = (value << 4) | digit;
	}
	if (!digits)
		return -EINVAL;
	*cursor = pos;
	*gain = value;
	return 0;
}

static int picasso_parse_gyro_gain(const char *buffer, size_t length,
				   u32 gains[2])
{
	const char *pos = buffer, *end = buffer + length;
	u32 values[2];
	int rc;

	if (!length || length > PICASSO_GYRO_GAIN_MAX_SIZE ||
	    memchr(buffer, '\0', length))
		return -EINVAL;
	/* The original scanf format also accepted leading whitespace. */
	while (pos < end && isspace(*pos))
		pos++;
	rc = picasso_parse_gain(&pos, end, "GyroGainX:0x", &values[0]);
	if (rc)
		return rc;
	if (pos == end || !isspace(*pos))
		return -EINVAL;
	while (pos < end && isspace(*pos))
		pos++;
	rc = picasso_parse_gain(&pos, end, "GyroGainY:0x", &values[1]);
	if (rc)
		return rc;
	while (pos < end && isspace(*pos))
		pos++;
	if (pos != end)
		return -EINVAL;

	gains[0] = values[0];
	gains[1] = values[1];
	return 0;
}

static int picasso_read_gyro_gain(u32 gains[2])
{
	char buffer[PICASSO_GYRO_GAIN_MAX_SIZE + 1];
	struct file *file;
	loff_t length, position = 0;
	size_t done = 0;
	ssize_t count;
	int rc;

	file = filp_open(PICASSO_GYRO_GAIN_PATH, O_RDONLY | O_NOFOLLOW, 0);
	if (IS_ERR(file))
		return PTR_ERR(file);
	length = i_size_read(file_inode(file));
	if (!S_ISREG(file_inode(file)->i_mode) || length <= 0 ||
	    length > PICASSO_GYRO_GAIN_MAX_SIZE) {
		rc = -EINVAL;
		goto close_file;
	}
	while (done < length) {
		count = kernel_read(file, buffer + done, length - done, &position);
		if (count <= 0) {
			rc = count < 0 ? count : -EIO;
			goto close_file;
		}
		done += count;
	}
	if (i_size_read(file_inode(file)) != length) {
		rc = -EIO;
		goto close_file;
	}
	buffer[done] = '\0';
	rc = picasso_parse_gyro_gain(buffer, done, gains);

close_file:
	filp_close(file, NULL);
	return rc;
}

int cam_ois_picasso_get_gyro_gain(struct cam_ois_ctrl_t *o_ctrl,
				  struct cam_sensor_i2c_reg_array *regs)
{
	u32 gains[2];
	int rc;

	if (!o_ctrl || !regs)
		return -EINVAL;
	/* This factory file belongs to the IMX686 OIS, never the tele/front. */
	if (!IS_ENABLED(CONFIG_MACH_ASUS_PICASSO) || o_ctrl->soc_info.index ||
	    o_ctrl->io_master_info.master_type != CCI_MASTER ||
	    !o_ctrl->io_master_info.cci_client ||
	    o_ctrl->io_master_info.cci_client->sid != 0x24 ||
	    !cam_eeprom_picasso_ois_calibration_allowed())
		return 0;
	rc = picasso_read_gyro_gain(gains);
	if (rc)
		return rc == -ENOENT ? 0 : rc;
	if (!cam_eeprom_picasso_ois_calibration_allowed())
		return 0;

	memset(regs, 0, sizeof(*regs) * 2);
	regs[0].reg_addr = 0x8890;
	regs[0].reg_data = gains[0];
	regs[1].reg_addr = 0x88c4;
	regs[1].reg_data = gains[1];
	return 1;
}
