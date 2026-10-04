/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _CAM_CSIPHY_1_2_3_PICASSO_HWREG_H_
#define _CAM_CSIPHY_1_2_3_PICASSO_HWREG_H_

#include "../cam_csiphy_dev.h"

/*
 * ASUS Picasso CPHY tuning from the working 5.4.295 kernel. Keep these
 * separate from the generic v1.2.3 tables used by other devices.
 */
#define PICASSO_CSIPHY_3PH_CONFIG_ARRAY_SIZE 31

static struct csiphy_reg_t
csiphy_3ph_v1_2_3_picasso_reg[MAX_LANES][MAX_SETTINGS_PER_LANE] = {
	{
		{0x0990, 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0994, 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0998, 0x1A, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x098C, 0xAF, 0x64, CSIPHY_DEFAULT_PARAMS},
		{0x0168, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x016C, 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x015C, 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0104, 0x06, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x010C, 0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
		{0x0108, 0x00, 0x00, CSIPHY_SETTLE_CNT_HIGHER_BYTE},
		{0x0114, 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0150, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0188, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x018C, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0190, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0118, 0x3E, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x011C, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0120, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0124, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0128, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x012C, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0144, 0xA2, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0160, 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x01CC, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0164, 0x33, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x01DC, 0x50, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0984, 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0988, 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0980, 0x61, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x09B0, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0800, 0x0E, 0x00, CSIPHY_DEFAULT_PARAMS},
	},
	{
		{0x0A90, 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0A94, 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0A98, 0x1A, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0A8C, 0xAF, 0x64, CSIPHY_DEFAULT_PARAMS},
		{0x0368, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x036C, 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x035C, 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0304, 0x06, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x030C, 0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
		{0x0308, 0x00, 0x00, CSIPHY_SETTLE_CNT_HIGHER_BYTE},
		{0x0314, 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0350, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0388, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x038C, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0390, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0318, 0x3E, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x031C, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0320, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0324, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0328, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x032C, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0344, 0xA2, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0360, 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x03CC, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0364, 0x33, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x03DC, 0x50, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0A84, 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0A88, 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0A80, 0x61, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0AB0, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0800, 0x0E, 0x00, CSIPHY_DEFAULT_PARAMS},
	},
	{
		{0x0B90, 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0B94, 0x08, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0B98, 0x1A, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0B8C, 0xAF, 0x64, CSIPHY_DEFAULT_PARAMS},
		{0x0568, 0x80, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x056C, 0x25, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x055C, 0x46, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0504, 0x06, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x050C, 0x08, 0x00, CSIPHY_SETTLE_CNT_LOWER_BYTE},
		{0x0508, 0x00, 0x00, CSIPHY_SETTLE_CNT_HIGHER_BYTE},
		{0x0514, 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0550, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0588, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x058C, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0590, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0518, 0x3E, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x051C, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0520, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0524, 0x7F, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0528, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x052C, 0x00, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0544, 0xA2, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0560, 0x02, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x05CC, 0x41, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0564, 0x33, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x05DC, 0x50, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0B84, 0x20, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0B88, 0x05, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0B80, 0x61, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0BB0, 0x01, 0x00, CSIPHY_DEFAULT_PARAMS},
		{0x0800, 0x0E, 0x00, CSIPHY_DEFAULT_PARAMS},
	},
};

static struct data_rate_settings_t data_rate_delta_table_1_2_3_picasso = {
	.num_data_rate_settings = 4,
	.data_rate_settings = {
		{
			/* (2.5 * 10**3 * 2.28) rounded value */
			.bandwidth = 5700000000,
			.data_rate_reg_array_size = 1,
			.per_lane_info = {
				{
					.lane_identifier = CPHY_LANE_0,
					.csiphy_data_rate_regs = {
						{0x9B4, 0x08, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
				{
					.lane_identifier = CPHY_LANE_1,
					.csiphy_data_rate_regs = {
						{0xAB4, 0x08, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
				{
					.lane_identifier = CPHY_LANE_2,
					.csiphy_data_rate_regs = {
						{0xBB4, 0x08, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
			},
		},
		{
			/* (3 * 10**3 * 2.28) rounded value */
			.bandwidth = 6840000000,
			.data_rate_reg_array_size = 1,
			.per_lane_info = {
				{
					.lane_identifier = CPHY_LANE_0,
					.csiphy_data_rate_regs = {
						{0x9B4, 0x08, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
				{
					.lane_identifier = CPHY_LANE_1,
					.csiphy_data_rate_regs = {
						{0xAB4, 0x08, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
				{
					.lane_identifier = CPHY_LANE_2,
					.csiphy_data_rate_regs = {
						{0xBB4, 0x08, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
			},
		},
		{
			/* (3.5 * 10**3 * 2.28) rounded value */
			.bandwidth = 7980000000,
			.data_rate_reg_array_size = 1,
			.per_lane_info = {
				{
					.lane_identifier = CPHY_LANE_0,
					.csiphy_data_rate_regs = {
						{0x9B4, 0x04, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
				{
					.lane_identifier = CPHY_LANE_1,
					.csiphy_data_rate_regs = {
						{0xAB4, 0x04, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
				{
					.lane_identifier = CPHY_LANE_2,
					.csiphy_data_rate_regs = {
						{0xBB4, 0x04, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
			},
		},
		{
			/* (4.5 * 10**3 * 2.28) rounded value */
			.bandwidth = 10260000000,
			.data_rate_reg_array_size = 1,
			.per_lane_info = {
				{
					.lane_identifier = CPHY_LANE_0,
					.csiphy_data_rate_regs = {
						{0x9B4, 0x04, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
				{
					.lane_identifier = CPHY_LANE_1,
					.csiphy_data_rate_regs = {
						{0xAB4, 0x04, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
				{
					.lane_identifier = CPHY_LANE_2,
					.csiphy_data_rate_regs = {
						{0xBB4, 0x04, 0x0A,
							CSIPHY_DEFAULT_PARAMS},
					},
				},
			},
		},
	},
};

#endif /* _CAM_CSIPHY_1_2_3_PICASSO_HWREG_H_ */
