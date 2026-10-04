/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef _CAM_EEPROM_PICASSO_H_
#define _CAM_EEPROM_PICASSO_H_

#include <linux/types.h>

struct cam_eeprom_ctrl_t;

void cam_eeprom_picasso_invalidate(struct cam_eeprom_ctrl_t *e_ctrl);
bool cam_eeprom_picasso_ois_calibration_allowed(void);
int cam_eeprom_picasso_prepare(struct cam_eeprom_ctrl_t *e_ctrl);

#endif /* _CAM_EEPROM_PICASSO_H_ */
