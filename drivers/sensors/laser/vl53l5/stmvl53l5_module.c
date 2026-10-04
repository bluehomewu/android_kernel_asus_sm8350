/**************************************************************************
 * Copyright (c) 2016, STMicroelectronics - All Rights Reserved

 License terms: BSD 3-clause "New" or "Revised" License.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 3. Neither the name of the copyright holder nor the names of its contributors
 may be used to endorse or promote products derived from this software
 without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ****************************************************************************/

/*
 * Picasso uses the SPI transport only. Preserve the stock userspace transfer
 * ABI and wire protocol; firmware and calibration uploads remain HAL-owned.
 */
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/kref.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/spi/spi-msm-geni.h>
#include <linux/uaccess.h>

#include "stmvl53l5_spi.h"

#define STMVL53L5_DRV_NAME "stmvl53l5"
#define VL53L5_COMMS_CHUNK_SIZE 1024

/* The stock ioctl encodes sizeof(void *), not sizeof the request structure. */
#define ST_TOF_IOCTL_TRANSFER _IOWR('a', 0x1, void *)

struct stmvl53l5_comms_struct {
	__u16 len;
	__u16 reg_index;
	__u8 __user *buf;
	__u8 write_not_read;
};

struct stmvl53l5_data {
	struct spi_data_t spi_data;
	struct spi_geni_qcom_ctrl_data delays;
	struct miscdevice miscdev;
	struct mutex lock;
	struct kref ref;
	struct regulator_bulk_data supplies[2];
	u8 *buffer;
	unsigned int configured;
	unsigned int enabled;
	void *saved_controller_data;
	u16 saved_mode;
	u8 saved_bits_per_word;
};

static const struct {
	const char *name;
	int min_uv;
	int max_uv;
} stmvl53l5_rails[] = {
	{ "laser_vdd", 2800000, 3300000 },
	{ "laser_bus", 1800000, 1800000 },
};

static void stmvl53l5_free(struct kref *ref)
{
	struct stmvl53l5_data *data =
		container_of(ref, struct stmvl53l5_data, ref);

	kfree(data->spi_data.index_bytes);
	kfree(data->buffer);
	kfree(data);
}

static void stmvl53l5_power_off(struct stmvl53l5_data *data)
{
	int i, ret;

	for (i = ARRAY_SIZE(stmvl53l5_rails) - 1; i >= 0; i--) {
		if (data->enabled & BIT(i)) {
			ret = regulator_disable(data->supplies[i].consumer);
			if (ret)
				pr_warn("stmvl53l5: disable %s failed: %d\n",
					stmvl53l5_rails[i].name, ret);
			data->enabled &= ~BIT(i);
		}
		if (data->configured & BIT(i)) {
			regulator_set_load(data->supplies[i].consumer, 0);
			regulator_set_voltage(data->supplies[i].consumer, 0,
					      stmvl53l5_rails[i].max_uv);
			data->configured &= ~BIT(i);
		}
	}
}

static int stmvl53l5_power_on(struct stmvl53l5_data *data)
{
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(stmvl53l5_rails); i++) {
		ret = regulator_set_voltage(data->supplies[i].consumer,
					   stmvl53l5_rails[i].min_uv,
					   stmvl53l5_rails[i].max_uv);
		if (ret)
			goto fail;
		data->configured |= BIT(i);
		ret = regulator_set_load(data->supplies[i].consumer, 0);
		if (ret < 0)
			goto fail;
	}
	/* Preserve the stock VDD-before-bus enable order and 200 mA votes. */
	for (i = 0; i < ARRAY_SIZE(stmvl53l5_rails); i++) {
		ret = regulator_set_load(data->supplies[i].consumer, 200000);
		if (ret < 0)
			goto fail;
		ret = regulator_enable(data->supplies[i].consumer);
		if (ret)
			goto fail;
		data->enabled |= BIT(i);
	}
	return 0;

fail:
	stmvl53l5_power_off(data);
	return ret;
}

static int stmvl53l5_open(struct inode *inode, struct file *file)
{
	struct miscdevice *miscdev = file->private_data;
	struct stmvl53l5_data *data =
		container_of(miscdev, struct stmvl53l5_data, miscdev);
	int ret = 0;

	mutex_lock(&data->lock);
	if (!data->spi_data.device) {
		ret = -ENODEV;
	} else {
		kref_get(&data->ref);
		file->private_data = data;
	}
	mutex_unlock(&data->lock);

	return ret ?: nonseekable_open(inode, file);
}

static int stmvl53l5_release(struct inode *inode, struct file *file)
{
	struct stmvl53l5_data *data = file->private_data;

	kref_put(&data->ref, stmvl53l5_free);
	return 0;
}

static long stmvl53l5_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	struct stmvl53l5_data *data = file->private_data;
	struct stmvl53l5_comms_struct request;
	unsigned int index, count;
	int ret;

	if (cmd != ST_TOF_IOCTL_TRANSFER)
		return -ENOTTY;
	if (copy_from_user(&request, (void __user *)arg, sizeof(request)))
		return -EFAULT;
	/* Do not allow the 16-bit register cursor to wrap during a transfer. */
	if ((u32)request.reg_index + request.len > 0x10000)
		return -EINVAL;
	if (request.len && !access_ok(request.buf, request.len))
		return -EFAULT;

	ret = mutex_lock_interruptible(&data->lock);
	if (ret)
		return ret;
	if (!data->spi_data.device) {
		ret = -ENODEV;
		goto out;
	}

	for (index = 0; index < request.len; index += count) {
		count = min_t(unsigned int, request.len - index,
			      VL53L5_COMMS_CHUNK_SIZE);
		if (request.write_not_read) {
			if (copy_from_user(data->buffer, request.buf + index,
					   count)) {
				ret = -EFAULT;
				goto out;
			}
			ret = stmvl53l5_spi_write(&data->spi_data,
						 request.reg_index + index,
						 data->buffer, count);
		} else {
			ret = stmvl53l5_spi_read(&data->spi_data,
						request.reg_index + index,
						data->buffer, count);
			if (!ret && copy_to_user(request.buf + index,
						 data->buffer, count))
				ret = -EFAULT;
		}
		if (ret)
			goto out;
	}
	ret = 0;
out:
	mutex_unlock(&data->lock);
	return ret;
}

static const struct file_operations stmvl53l5_fops = {
	.owner = THIS_MODULE,
	.open = stmvl53l5_open,
	.release = stmvl53l5_release,
	.unlocked_ioctl = stmvl53l5_ioctl,
	/* Stock Picasso uses the native 64-bit ABI; compat returns ENOTTY. */
	.llseek = no_llseek,
};

static int stmvl53l5_probe(struct spi_device *spi)
{
	struct stmvl53l5_data *data;
	u8 device_id, revision_id;
	int i, ret;

	/* These are the stock arm64 HAL layout and ioctl command. */
	BUILD_BUG_ON(sizeof(struct stmvl53l5_comms_struct) != 24);
	BUILD_BUG_ON(offsetof(struct stmvl53l5_comms_struct, buf) != 8);
	BUILD_BUG_ON(offsetof(struct stmvl53l5_comms_struct, write_not_read) != 16);
	BUILD_BUG_ON(ST_TOF_IOCTL_TRANSFER != 0xc0086101);

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;
	kref_init(&data->ref);
	mutex_init(&data->lock);
	data->buffer = kmalloc(VL53L5_COMMS_CHUNK_SIZE, GFP_KERNEL);
	data->spi_data.index_bytes = kmalloc(2, GFP_KERNEL);
	if (!data->buffer || !data->spi_data.index_bytes) {
		ret = -ENOMEM;
		goto free_data;
	}
	for (i = 0; i < ARRAY_SIZE(stmvl53l5_rails); i++)
		data->supplies[i].supply = stmvl53l5_rails[i].name;
	ret = regulator_bulk_get(&spi->dev, ARRAY_SIZE(data->supplies),
				 data->supplies);
	if (ret)
		goto free_data;
	ret = stmvl53l5_power_on(data);
	if (ret)
		goto free_supplies;

	data->spi_data.device = spi;
	data->saved_mode = spi->mode;
	data->saved_bits_per_word = spi->bits_per_word;
	data->saved_controller_data = spi->controller_data;
	data->delays.spi_cs_clk_delay = 0;
	data->delays.spi_inter_words_delay = 1;
	spi->controller_data = &data->delays;
	spi->mode |= SPI_MODE_3;
	spi->bits_per_word = 8;
	ret = spi_setup(spi);
	if (ret)
		goto restore_spi;

	/* Use allocated transfer buffers, including the two-byte SPI header. */
	data->buffer[0] = 0;
	ret = stmvl53l5_spi_write(&data->spi_data, 0x7fff, data->buffer, 1);
	if (ret)
		goto restore_spi;
	ret = stmvl53l5_spi_read(&data->spi_data, 0, data->buffer, 1);
	if (ret)
		goto restore_spi;
	device_id = data->buffer[0];
	ret = stmvl53l5_spi_read(&data->spi_data, 1, data->buffer, 1);
	if (ret)
		goto restore_spi;
	revision_id = data->buffer[0];
	if (device_id != 0xf0 || revision_id != 0x02) {
		dev_err(&spi->dev, "unexpected device/revision ID %02x/%02x\n",
			device_id, revision_id);
		ret = -ENODEV;
		goto restore_spi;
	}

	data->miscdev.minor = MISC_DYNAMIC_MINOR;
	data->miscdev.name = STMVL53L5_DRV_NAME;
	data->miscdev.fops = &stmvl53l5_fops;
	data->miscdev.parent = &spi->dev;
	data->miscdev.mode = 0660;
	spi_set_drvdata(spi, data);
	ret = misc_register(&data->miscdev);
	if (ret) {
		spi_set_drvdata(spi, NULL);
		goto restore_spi;
	}
	dev_info(&spi->dev, "Picasso ToF ready, device/revision %02x/%02x\n",
		 device_id, revision_id);
	return 0;

restore_spi:
	spi->controller_data = data->saved_controller_data;
	spi->mode = data->saved_mode;
	spi->bits_per_word = data->saved_bits_per_word;
	stmvl53l5_power_off(data);
free_supplies:
	regulator_bulk_free(ARRAY_SIZE(data->supplies), data->supplies);
free_data:
	kref_put(&data->ref, stmvl53l5_free);
	return ret;
}

static int stmvl53l5_remove(struct spi_device *spi)
{
	struct stmvl53l5_data *data = spi_get_drvdata(spi);

	misc_deregister(&data->miscdev);
	mutex_lock(&data->lock);
	data->spi_data.device = NULL;
	stmvl53l5_power_off(data);
	regulator_bulk_free(ARRAY_SIZE(data->supplies), data->supplies);
	spi->controller_data = data->saved_controller_data;
	spi->mode = data->saved_mode;
	spi->bits_per_word = data->saved_bits_per_word;
	spi_set_drvdata(spi, NULL);
	mutex_unlock(&data->lock);
	/* Already-open files retain the allocation but cannot access hardware. */
	kref_put(&data->ref, stmvl53l5_free);
	return 0;
}

static const struct of_device_id stmvl53l5_of_match[] = {
	{ .compatible = "st,stmvl53l5" },
	{ }
};
MODULE_DEVICE_TABLE(of, stmvl53l5_of_match);

static const struct spi_device_id stmvl53l5_spi_id[] = {
	{ STMVL53L5_DRV_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, stmvl53l5_spi_id);

static struct spi_driver stmvl53l5_driver = {
	.driver = {
		.name = STMVL53L5_DRV_NAME,
		.of_match_table = stmvl53l5_of_match,
	},
	.probe = stmvl53l5_probe,
	.remove = stmvl53l5_remove,
	.id_table = stmvl53l5_spi_id,
};
module_spi_driver(stmvl53l5_driver);

MODULE_DESCRIPTION("Picasso VL53L5 ToF SPI transport");
MODULE_LICENSE("GPL");
