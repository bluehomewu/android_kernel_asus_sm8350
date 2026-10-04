/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _CAM_OIS_PICASSO_H_
#define _CAM_OIS_PICASSO_H_

struct cam_ois_ctrl_t;
struct cam_sensor_i2c_reg_array;

/* Return 1 for two valid gains, 0 if unavailable, or a negative read error. */
int cam_ois_picasso_get_gyro_gain(struct cam_ois_ctrl_t *o_ctrl,
				  struct cam_sensor_i2c_reg_array *regs);

#endif /* _CAM_OIS_PICASSO_H_ */
