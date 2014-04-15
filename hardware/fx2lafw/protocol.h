/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2013 Bert Vermeulen <bert@biot.com>
 * Copyright (C) 2012 Joel Holdsworth <joel@airwebreathe.org.uk>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBSIGROK_HARDWARE_FX2LAFW_PROTOCOL_H
#define LIBSIGROK_HARDWARE_FX2LAFW_PROTOCOL_H

#include <glib.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <libusb.h>
#include "libsigrok.h"
#include "libsigrok-internal.h"

#define LOG_PREFIX "fx2lafw"

#define USB_INTERFACE		0
#define USB_CONFIGURATION	1
#define NUM_TRIGGER_STAGES	4
#define TRIGGER_TYPE 		"01"

#define MAX_RENUM_DELAY_MS	3000
#define NUM_SIMUL_TRANSFERS	32
#define MAX_EMPTY_TRANSFERS	(NUM_SIMUL_TRANSFERS * 2)

#define FX2LAFW_REQUIRED_VERSION_MAJOR	1

#define MAX_8BIT_SAMPLE_RATE	SR_MHZ(24)
#define MAX_16BIT_SAMPLE_RATE	SR_MHZ(12)

/* 6 delay states of up to 256 clock ticks */
#define MAX_SAMPLE_DELAY	(6 * 256)

/* Software trigger implementation: positive values indicate trigger stage. */
#define TRIGGER_FIRED          -1

#define DEV_CAPS_16BIT_POS	0

#define DEV_CAPS_16BIT		(1 << DEV_CAPS_16BIT_POS)

struct fx2lafw_profile {
	uint16_t vid;
	uint16_t pid;

	const char *vendor;
	const char *model;
	const char *model_version;

	const char *firmware;

	uint32_t dev_caps;

	const char *usb_manufacturer;
	const char *usb_product;
};

#define DSLOGIC_TRIGGER_STAGES 16
#define DSLOGIC_TRIGGER_PROBES 16

struct ds_trigger {
    uint16_t trigger_en;
    uint16_t trigger_mode;
    uint16_t trigger_pos;
    uint16_t trigger_stages;
    unsigned char trigger_logic[DSLOGIC_TRIGGER_STAGES + 1];
    unsigned char trigger0_inv[DSLOGIC_TRIGGER_STAGES + 1];
    unsigned char trigger1_inv[DSLOGIC_TRIGGER_STAGES + 1];
    char trigger0[DSLOGIC_TRIGGER_STAGES + 1][DSLOGIC_TRIGGER_PROBES];
    char trigger1[DSLOGIC_TRIGGER_STAGES + 1][DSLOGIC_TRIGGER_PROBES];
    uint16_t trigger0_count[DSLOGIC_TRIGGER_STAGES + 1];
    uint16_t trigger1_count[DSLOGIC_TRIGGER_STAGES + 1];
};

struct ds_trigger_pos {
    uint32_t real_pos;
    uint32_t ram_saddr;
    unsigned char first_block[504];
};

struct dev_context {
	const struct fx2lafw_profile *profile;
	/*
	 * Since we can't keep track of an fx2lafw device after upgrading
	 * the firmware (it renumerates into a different device address
	 * after the upgrade) this is like a global lock. No device will open
	 * until a proper delay after the last device was upgraded.
	 */
	int64_t fw_updated;

	/* Supported samplerates */
	const uint64_t *samplerates;
	int num_samplerates;

	/* Device/capture settings */
	uint64_t cur_samplerate;
	uint64_t limit_samples;

	/* Operational settings */
	gboolean sample_wide;
	uint16_t trigger_mask[NUM_TRIGGER_STAGES];
	uint16_t trigger_value[NUM_TRIGGER_STAGES];
	int trigger_stage;
	uint16_t trigger_buffer[NUM_TRIGGER_STAGES];

	int num_samples;
	int submitted_transfers;
	int empty_transfer_count;

	void *cb_data;
	unsigned int num_transfers;
	struct libusb_transfer **transfers;
	struct sr_context *ctx;

	/* Is this a DSLogic? */
	gboolean dslogic;

	/* DSLogic specific settings. */
	int dslogic_mode;
	uint16_t dslogic_test;
	gboolean dslogic_ext_clock;
	uint16_t dslogic_test_init;
	uint16_t dslogic_test_sample_value;
	int dslogic_status;
	struct ds_trigger trigger;
};

/** DSLogic device modes. */
enum {
	/* Logic analyzer (16 logic channels) */
	DSLOGIC_MODE_LOGIC,
	/* DSO (2 analog channels) */
	DSLOGIC_MODE_DSO,
	/* Data acquisition (9 analog channels) */
	DSLOGIC_MODE_ANALOG
};

/** DSLogic test modes. */
enum {
	/** None */
	DSLOGIC_TEST_NONE = 0,
	/** Internal pattern test mode */
	DSLOGIC_TEST_INTERNAL = 1,
	/** External pattern test mode */
	DSLOGIC_TEST_EXTERNAL = 2,
	/** SDRAM loopback test mode */
	DSLOGIC_TEST_LOOPBACK = 3,
};

/** DSLogic states. */
enum {
    DSLOGIC_ERROR = -1,
    DSLOGIC_INIT = 0,
    DSLOGIC_START = 1,
    DSLOGIC_TRIGGERED = 2,
    DSLOGIC_DATA = 3,
    DSLOGIC_STOP = 4,
};

struct dslogic_setting {
	uint32_t sync;
	uint16_t mode_header;
	uint16_t mode;
	uint32_t divider_header;
	uint32_t divider;
	uint32_t count_header;
	uint32_t count;
	uint32_t trig_pos_header;
	uint32_t trig_pos;
	uint16_t trig_glb_header;
	uint16_t trig_glb;
	uint32_t trig_adp_header;
	uint32_t trig_adp;
	uint32_t trig_sda_header;
	uint32_t trig_sda;
	uint32_t trig_mask0_header;
	uint16_t trig_mask0[DSLOGIC_TRIGGER_STAGES];
	uint32_t trig_mask1_header;
	uint16_t trig_mask1[DSLOGIC_TRIGGER_STAGES];
	uint32_t trig_value0_header;
	uint16_t trig_value0[DSLOGIC_TRIGGER_STAGES];
	uint32_t trig_value1_header;
	uint16_t trig_value1[DSLOGIC_TRIGGER_STAGES];
	uint32_t trig_edge0_header;
	uint16_t trig_edge0[DSLOGIC_TRIGGER_STAGES];
	uint32_t trig_edge1_header;
	uint16_t trig_edge1[DSLOGIC_TRIGGER_STAGES];
	uint32_t trig_count0_header;
	uint16_t trig_count0[DSLOGIC_TRIGGER_STAGES];
	uint32_t trig_count1_header;
	uint16_t trig_count1[DSLOGIC_TRIGGER_STAGES];
	uint32_t trig_logic0_header;
	uint16_t trig_logic0[DSLOGIC_TRIGGER_STAGES];
	uint32_t trig_logic1_header;
	uint16_t trig_logic1[DSLOGIC_TRIGGER_STAGES];
	uint32_t end_sync;
};

enum {
    DSLOGIC_TRIGGER_SIMPLE = 0,
	DSLOGIC_TRIGGER_ADVANCED,
};

SR_PRIV int ds_trigger_init(struct ds_trigger *trigger);
SR_PRIV int ds_trigger_stage_set_value(struct ds_trigger *trigger,
		uint16_t stage, uint16_t probes, char *trigger0, char *trigger1);
SR_PRIV int ds_trigger_stage_set_logic(struct ds_trigger *trigger,
		uint16_t stage, uint16_t probes, unsigned char trigger_logic);
SR_PRIV int ds_trigger_stage_set_inv(struct ds_trigger *trigger,
		uint16_t stage, uint16_t probes, unsigned char trigger0_inv,
		unsigned char trigger1_inv);
SR_PRIV int ds_trigger_stage_set_count(struct ds_trigger *trigger,
		uint16_t stage, uint16_t probes, uint16_t trigger0_count,
		uint16_t trigger1_count);
SR_PRIV int ds_trigger_probe_set(struct ds_trigger *trigger, uint16_t probe,
		unsigned char trigger0, unsigned char trigger1);
SR_PRIV int ds_trigger_set_stage(struct ds_trigger *trigger, uint16_t stages);
SR_PRIV int ds_trigger_set_pos(struct ds_trigger *trigger, uint16_t position);
SR_PRIV int ds_trigger_set_en(struct ds_trigger *trigger, uint16_t enable);
SR_PRIV int ds_trigger_set_mode(struct ds_trigger *trigger, uint16_t mode);
SR_PRIV uint64_t ds_trigger_get_mask0(struct ds_trigger *trigger, uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_mask1(struct ds_trigger *trigger, uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_value0(struct ds_trigger *trigger, uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_value1(struct ds_trigger *trigger, uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_edge0(struct ds_trigger *trigger, uint16_t stage);
SR_PRIV uint64_t ds_trigger_get_edge1(struct ds_trigger *trigger, uint16_t stage);

SR_PRIV int dslogic_command_stop_acquisition(libusb_device_handle *devhdl);
SR_PRIV int dslogic_command_fpga_config(libusb_device_handle *devhdl);
SR_PRIV int dslogic_command_fpga_setting(libusb_device_handle *devhdl,
			uint32_t setting_count);
SR_PRIV int dslogic_fpga_config(struct libusb_device_handle *hdl,
			const char *filename);
SR_PRIV int dslogic_fpga_setting(const struct sr_dev_inst *sdi);

SR_PRIV int fx2lafw_command_start_acquisition(const struct sr_dev_inst *sdi);
SR_PRIV gboolean fx2lafw_check_conf_profile(libusb_device *dev);
SR_PRIV int fx2lafw_dev_open(struct sr_dev_inst *sdi, struct sr_dev_driver *di);
SR_PRIV int fx2lafw_configure_channels(const struct sr_dev_inst *sdi);
SR_PRIV struct dev_context *fx2lafw_dev_new(void);
SR_PRIV void fx2lafw_abort_acquisition(struct dev_context *devc);
SR_PRIV void fx2lafw_free_transfer(struct libusb_transfer *transfer);
SR_PRIV void fx2lafw_receive_transfer(struct libusb_transfer *transfer);
SR_PRIV size_t fx2lafw_get_buffer_size(struct dev_context *devc);
SR_PRIV unsigned int fx2lafw_get_number_of_transfers(struct dev_context *devc);
SR_PRIV unsigned int fx2lafw_get_timeout(struct dev_context *devc);

#endif
