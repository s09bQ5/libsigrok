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

#include "protocol.h"
#include <math.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>

/* Protocol commands */
#define CMD_GET_FW_VERSION		0xb0
#define CMD_START			0xb1
#define CMD_GET_REVID_VERSION		0xb2

#define CMD_START_FLAGS_WIDE_POS	5
#define CMD_START_FLAGS_CLK_SRC_POS	6

#define CMD_START_FLAGS_SAMPLE_8BIT	(0 << CMD_START_FLAGS_WIDE_POS)
#define CMD_START_FLAGS_SAMPLE_16BIT	(1 << CMD_START_FLAGS_WIDE_POS)

#define CMD_START_FLAGS_CLK_30MHZ	(0 << CMD_START_FLAGS_CLK_SRC_POS)
#define CMD_START_FLAGS_CLK_48MHZ	(1 << CMD_START_FLAGS_CLK_SRC_POS)

/* Modified protocol commands & flags used by DSLogic */
#define CMD_DSLOGIC_GET_REVID_VERSION	0xb1
#define CMD_DSLOGIC_START	0xb2
#define CMD_DSLOGIC_CONFIG	0xb3
#define CMD_DSLOGIC_SETTING	0xb4

#define CMD_START_FLAGS_DSLOGIC_STOP_POS	7
#define CMD_START_FLAGS_DSLOGIC_STOP (1 << CMD_START_FLAGS_DSLOGIC_STOP_POS)

/* Size of FPGA bitstream for DSLogic */
#define XC6SLX9_BYTE_CNT	340604

#pragma pack(push, 1)

struct version_info {
	uint8_t major;
	uint8_t minor;
};

struct cmd_start_acquisition {
	uint8_t flags;
	uint8_t sample_delay_h;
	uint8_t sample_delay_l;
};

#pragma pack(pop)

static int command_get_fw_version(libusb_device_handle *devhdl,
				  struct version_info *vi)
{
	int ret;

	ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
		LIBUSB_ENDPOINT_IN, CMD_GET_FW_VERSION, 0x0000, 0x0000,
		(unsigned char *)vi, sizeof(struct version_info), 100);

	if (ret < 0) {
		sr_err("Unable to get version info: %s.",
		       libusb_error_name(ret));
		return SR_ERR;
	}

	return SR_OK;
}

static int command_get_revid_version(struct sr_dev_inst *sdi, uint8_t *revid)
{
	struct dev_context *devc = sdi->priv;
	struct sr_usb_dev_inst *usb = sdi->conn;
	libusb_device_handle *devhdl = usb->devhdl;
	int ret;

	ret = libusb_control_transfer(devhdl,
		LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_IN,
		devc->dslogic ? CMD_DSLOGIC_GET_REVID_VERSION : CMD_GET_REVID_VERSION,
		0x0000, 0x0000, revid, 1, 100);

	if (ret < 0) {
		sr_err("Unable to get REVID: %s.", libusb_error_name(ret));
		return SR_ERR;
	}

	return SR_OK;
}

SR_PRIV int fx2lafw_command_start_acquisition(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc = sdi->priv;
	struct sr_usb_dev_inst *usb = sdi->conn;
	libusb_device_handle *devhdl = usb->devhdl;
	uint64_t samplerate = devc->cur_samplerate;
	gboolean samplewide = devc->sample_wide;
	struct cmd_start_acquisition cmd = { 0 };
	int delay = 0, ret;

	if (devc->dslogic)
	{
		cmd.flags = CMD_START_FLAGS_CLK_30MHZ;
		delay = 0;
	}
	else
	{
		/* Compute the sample rate. */
		if (samplewide && samplerate > MAX_16BIT_SAMPLE_RATE) {
			sr_err("Unable to sample at %" PRIu64 "Hz "
				   "when collecting 16-bit samples.", samplerate);
			return SR_ERR;
		}

		if ((SR_MHZ(48) % samplerate) == 0) {
			cmd.flags = CMD_START_FLAGS_CLK_48MHZ;
			delay = SR_MHZ(48) / samplerate - 1;
			if (delay > MAX_SAMPLE_DELAY)
				delay = 0;
		}

		if (delay == 0 && (SR_MHZ(30) % samplerate) == 0) {
			cmd.flags = CMD_START_FLAGS_CLK_30MHZ;
			delay = SR_MHZ(30) / samplerate - 1;
		}
	}

	sr_info("GPIF delay = %d, clocksource = %sMHz.", delay,
		(cmd.flags & CMD_START_FLAGS_CLK_48MHZ) ? "48" : "30");

	if (!devc->dslogic)
	{
		if (delay <= 0 || delay > MAX_SAMPLE_DELAY) {
			sr_err("Unable to sample at %" PRIu64 "Hz.", samplerate);
			return SR_ERR;
		}
	}

	cmd.sample_delay_h = (delay >> 8) & 0xff;
	cmd.sample_delay_l = delay & 0xff;

	/* Select the sampling width. */
	cmd.flags |= samplewide ? CMD_START_FLAGS_SAMPLE_16BIT :
		CMD_START_FLAGS_SAMPLE_8BIT;

	/* Send the control message. */
	ret = libusb_control_transfer(devhdl,
			LIBUSB_REQUEST_TYPE_VENDOR | LIBUSB_ENDPOINT_OUT,
			devc->dslogic ? CMD_DSLOGIC_START : CMD_START, 0x0000, 0x0000,
			(unsigned char *)&cmd, sizeof(cmd), devc->dslogic ? 3000 : 100);
	if (ret < 0) {
		sr_err("Unable to send start command: %s.",
		       libusb_error_name(ret));
		return SR_ERR;
	}

	return SR_OK;
}

SR_PRIV int dslogic_command_stop_acquisition(libusb_device_handle *devhdl)
{
	struct cmd_start_acquisition cmd;
	int ret;

	/* Stop acquisition command */
	cmd.flags = CMD_START_FLAGS_DSLOGIC_STOP;
	cmd.sample_delay_h = 0;
	cmd.sample_delay_l = 0;

	/* Send the control message. */
	ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_ENDPOINT_OUT, CMD_DSLOGIC_START, 0x0000, 0x0000,
			(unsigned char *)&cmd, sizeof(cmd), 3000);
	if (ret < 0) {
		sr_err("Unable to send stop command: %s.",
				libusb_error_name(ret));
		return SR_ERR;
	}

	return SR_OK;
}

SR_PRIV int dslogic_command_fpga_config(libusb_device_handle *devhdl)
{
	int ret;

	/* Send the control message. */
	ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_ENDPOINT_OUT, CMD_DSLOGIC_CONFIG, 0x0000, 0x0000,
			NULL, 0, 3000);
	if (ret < 0) {
		sr_err("Unable to send FPGA configure command: %s.",
				libusb_error_name(ret));
		return SR_ERR;
	}

	return SR_OK;
}

SR_PRIV int dslogic_command_fpga_setting(libusb_device_handle *devhdl,
	uint32_t setting_count)
{
	struct {
		uint8_t byte0;
		uint8_t byte1;
		uint8_t byte2;
	} cmd;
	int ret;

	/* ... */
	cmd.byte0 = (uint8_t)setting_count;
	cmd.byte1 = (uint8_t)(setting_count >> 8);
	cmd.byte2 = (uint8_t)(setting_count >> 16);

	/* Send the control message. */
	ret = libusb_control_transfer(devhdl, LIBUSB_REQUEST_TYPE_VENDOR |
			LIBUSB_ENDPOINT_OUT, CMD_DSLOGIC_SETTING, 0x0000, 0x0000,
			(unsigned char *)&cmd, sizeof(cmd), 100);
	if (ret < 0) {
		sr_err("Unable to send FPGA setting command: %s.",
				libusb_error_name(ret));
		return SR_ERR;
	}

	return SR_OK;
}

/**
 * Check the USB configuration to determine if this is an fx2lafw device.
 *
 * @return TRUE if the device's configuration profile match fx2lafw
 *         configuration, FALSE otherwise.
 */
SR_PRIV gboolean fx2lafw_check_conf_profile(libusb_device *dev)
{
	struct libusb_device_descriptor des;
	struct libusb_device_handle *hdl;
	gboolean ret;
	unsigned char strdesc[64];

	hdl = NULL;
	ret = FALSE;
	while (!ret) {
		/* Assume the FW has not been loaded, unless proven wrong. */
		if (libusb_get_device_descriptor(dev, &des) != 0)
			break;

		if (libusb_open(dev, &hdl) != 0)
			break;

		if (libusb_get_string_descriptor_ascii(hdl,
		    des.iManufacturer, strdesc, sizeof(strdesc)) < 0)
			break;
		if (strncmp((const char *)strdesc, "sigrok", 6) &&
				strncmp((const char *)strdesc, "DreamSourceLab", 14))
			break;

		if (libusb_get_string_descriptor_ascii(hdl,
				des.iProduct, strdesc, sizeof(strdesc)) < 0)
			break;
		if (strncmp((const char *)strdesc, "fx2lafw", 7) &&
				strncmp((const char *)strdesc, "DSLogic", 7))
			break;

		/* If we made it here, it must be an fx2lafw. */
		ret = TRUE;
	}
	if (hdl)
		libusb_close(hdl);

	return ret;
}

int dslogic_fpga_setting(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_usb_dev_inst *usb;
	struct libusb_device_handle *hdl;
	struct dslogic_setting setting;
	int ret;
	int transferred;
	int result;
	int i;

	devc = sdi->priv;
	usb = sdi->conn;
	hdl = usb->devhdl;

	setting.sync = 0xffffffff;
	setting.mode_header = 0x0001;
	setting.divider_header = 0x0102ffff;
	setting.count_header = 0x0302ffff;
	setting.trig_pos_header = 0x0502ffff;
	setting.trig_glb_header = 0x0701;
	setting.trig_adp_header = 0x0a02ffff;
	setting.trig_sda_header = 0x0c02ffff;
	setting.trig_mask0_header = 0x1010ffff;
	setting.trig_mask1_header = 0x1110ffff;
	setting.trig_value0_header = 0x1410ffff;
	setting.trig_value1_header = 0x1510ffff;
	setting.trig_edge0_header = 0x1810ffff;
	setting.trig_edge1_header = 0x1910ffff;
	setting.trig_count0_header = 0x1c10ffff;
	setting.trig_count1_header = 0x1d10ffff;
	setting.trig_logic0_header = 0x2010ffff;
	setting.trig_logic1_header = 0x2110ffff;
	setting.end_sync = 0x0;

	setting.mode = ((devc->dslogic_test == DSLOGIC_TEST_EXTERNAL) << 15) +
		((devc->dslogic_test == DSLOGIC_TEST_EXTERNAL) << 14) +
		((devc->dslogic_test == DSLOGIC_TEST_LOOPBACK) << 13) +
		devc->trigger.trigger_en +
		((devc->dslogic_mode > 0) << 4) + (devc->dslogic_ext_clock<< 1) +
		(((devc->cur_samplerate == SR_MHZ(200)) ||
		  (devc->dslogic_mode == DSLOGIC_MODE_ANALOG)) << 5) +
		((devc->cur_samplerate == SR_MHZ(400)) << 6) +
		((devc->dslogic_mode == DSLOGIC_MODE_ANALOG) << 7);
	setting.divider = (uint32_t)ceil(SR_MHZ(100) * 1.0 / devc->cur_samplerate);
	setting.count = (uint32_t)(devc->limit_samples);
	setting.trig_pos = (uint32_t)
		(devc->trigger.trigger_pos / 100.0f * devc->limit_samples);
	setting.trig_glb = devc->trigger.trigger_stages;
	setting.trig_adp = setting.count - setting.trig_pos - 1;
	setting.trig_sda = 0x0;
	if (devc->trigger.trigger_mode == DSLOGIC_TRIGGER_SIMPLE) {
		setting.trig_mask0[0] = ds_trigger_get_mask0(&devc->trigger,
				DSLOGIC_TRIGGER_STAGES);
		setting.trig_mask1[0] = ds_trigger_get_mask1(&devc->trigger,
				DSLOGIC_TRIGGER_STAGES);

		setting.trig_value0[0] = ds_trigger_get_value0(&devc->trigger,
				DSLOGIC_TRIGGER_STAGES);
		setting.trig_value1[0] = ds_trigger_get_value1(&devc->trigger,
				DSLOGIC_TRIGGER_STAGES);

		setting.trig_edge0[0] = ds_trigger_get_edge0(&devc->trigger,
				DSLOGIC_TRIGGER_STAGES);
		setting.trig_edge1[0] = ds_trigger_get_edge1(&devc->trigger,
				DSLOGIC_TRIGGER_STAGES);

		setting.trig_count0[0] =
				devc->trigger.trigger0_count[DSLOGIC_TRIGGER_STAGES];
		setting.trig_count1[0] =
				devc->trigger.trigger1_count[DSLOGIC_TRIGGER_STAGES];

		setting.trig_logic0[0] =
			(devc->trigger.trigger_logic[DSLOGIC_TRIGGER_STAGES] << 1)
				+ devc->trigger.trigger0_inv[DSLOGIC_TRIGGER_STAGES];
		setting.trig_logic1[0] =
			(devc->trigger.trigger_logic[DSLOGIC_TRIGGER_STAGES] << 1)
				+ devc->trigger.trigger1_inv[DSLOGIC_TRIGGER_STAGES];

		for (i = 1; i < NUM_TRIGGER_STAGES; i++) {
			setting.trig_mask0[i] = 1;
			setting.trig_mask1[i] = 1;

			setting.trig_value0[i] = 0;
			setting.trig_value1[i] = 0;

			setting.trig_edge0[i] = 0;
			setting.trig_edge1[i] = 0;

			setting.trig_count0[i] = 0;
			setting.trig_count1[i] = 0;

			setting.trig_logic0[i] = 2;
			setting.trig_logic1[i] = 2;
		}
	} else {
		for (i = 0; i < NUM_TRIGGER_STAGES; i++) {
			setting.trig_mask0[i] = ds_trigger_get_mask0(&devc->trigger, i);
			setting.trig_mask1[i] = ds_trigger_get_mask1(&devc->trigger, i);

			setting.trig_value0[i] = ds_trigger_get_value0(&devc->trigger, i);
			setting.trig_value1[i] = ds_trigger_get_value1(&devc->trigger, i);

			setting.trig_edge0[i] = ds_trigger_get_edge0(&devc->trigger, i);
			setting.trig_edge1[i] = ds_trigger_get_edge1(&devc->trigger, i);

			setting.trig_count0[i] = devc->trigger.trigger0_count[i];
			setting.trig_count1[i] = devc->trigger.trigger1_count[i];

			setting.trig_logic0[i] = (devc->trigger.trigger_logic[i] << 1)
				+ devc->trigger.trigger0_inv[i];
			setting.trig_logic1[i] = (devc->trigger.trigger_logic[i] << 1)
				+ devc->trigger.trigger1_inv[i];
		}
	}

	result  = SR_OK;
	ret = libusb_bulk_transfer(hdl, 2 | LIBUSB_ENDPOINT_OUT,
							   (unsigned char *) &setting,
							   sizeof(struct dslogic_setting),
							   &transferred, 1000);

	if (ret < 0) {
		sr_err("Unable to setting FPGA of DSLogic: %s.",
				libusb_error_name(ret));
		result = SR_ERR;
	} else if (transferred != sizeof(struct dslogic_setting)) {
		sr_err("Setting FPGA error: expacted transfer size %d; actually %d",
				sizeof(struct dslogic_setting), transferred);
		result = SR_ERR;
	}

	if (result == SR_OK)
		sr_info("FPGA setting done. trigger_mode = %d; trigger_stages = %d;"
				"trigger_mask0 = %d; trigger_value0 = %d; trigger_edge0 = %d",
				devc->trigger.trigger_mode, devc->trigger.trigger_stages,
				setting.trig_mask0[0], setting.trig_value0[0],
				setting.trig_edge0[0]);

	return result;
}

int dslogic_fpga_config(struct libusb_device_handle *hdl, const char *filename)
{
	FILE *fw;
	int offset, chunksize, ret, result;
	unsigned char *buf;
	int transferred;

	if (!(buf = g_try_malloc(XC6SLX9_BYTE_CNT))) {
			sr_err("FPGA configure bit malloc failed.");
			return SR_ERR;
	}
	sr_info("Configure FPGA using %s", filename);
	if ((fw = g_fopen(filename, "rb")) == NULL) {
		sr_err("Unable to open FPGA bit file %s for reading: %s",
			   filename, strerror(errno));
		return SR_ERR;
	}

	result = SR_OK;
	offset = 0;
	while (1) {
		chunksize = fread(buf, 1, XC6SLX9_BYTE_CNT, fw);
		if (chunksize == 0)
			break;

		//do {
			ret = libusb_bulk_transfer(hdl, 2 | LIBUSB_ENDPOINT_OUT,
									   buf, chunksize,
									   &transferred, 1000);
		//} while(ret == LIBUSB_ERROR_TIMEOUT);

		if (ret < 0) {
			sr_err("Unable to configure FPGA of DSLogic: %s.",
					libusb_error_name(ret));
			result = SR_ERR;
			break;
		} else if (transferred != chunksize) {
			sr_err("Configure FPGA error: expacted transfer size %d; actually %d",
					chunksize, transferred);
			result = SR_ERR;
			break;
		}
		sr_info("Configure %d bytes", chunksize);
		offset += chunksize;
	}
	fclose(fw);
	if (result == SR_OK)
		sr_info("FPGA configure done");

	return result;
}

SR_PRIV int fx2lafw_dev_open(struct sr_dev_inst *sdi, struct sr_dev_driver *di)
{
	libusb_device **devlist;
	struct sr_usb_dev_inst *usb;
	struct libusb_device_descriptor des;
	struct dev_context *devc;
	struct drv_context *drvc;
	struct version_info vi;
	int ret, skip, i, device_count;
	uint8_t revid;

	drvc = di->priv;
	devc = sdi->priv;
	usb = sdi->conn;

	if (sdi->status == SR_ST_ACTIVE)
		/* Device is already in use. */
		return SR_ERR;

	skip = 0;
	device_count = libusb_get_device_list(drvc->sr_ctx->libusb_ctx, &devlist);
	if (device_count < 0) {
		sr_err("Failed to get device list: %s.",
		       libusb_error_name(device_count));
		return SR_ERR;
	}

	for (i = 0; i < device_count; i++) {
		if ((ret = libusb_get_device_descriptor(devlist[i], &des))) {
			sr_err("Failed to get device descriptor: %s.",
			       libusb_error_name(ret));
			continue;
		}

		if (des.idVendor != devc->profile->vid
		    || des.idProduct != devc->profile->pid)
			continue;

		if (sdi->status == SR_ST_INITIALIZING) {
			if (skip != sdi->index) {
				/* Skip devices of this type that aren't the one we want. */
				skip += 1;
				continue;
			}
		} else if (sdi->status == SR_ST_INACTIVE) {
			/*
			 * This device is fully enumerated, so we need to find
			 * this device by vendor, product, bus and address.
			 */
			if (libusb_get_bus_number(devlist[i]) != usb->bus
				|| libusb_get_device_address(devlist[i]) != usb->address)
				/* This is not the one. */
				continue;
		}

		if (!(ret = libusb_open(devlist[i], &usb->devhdl))) {
			if (usb->address == 0xff)
				/*
				 * First time we touch this device after FW
				 * upload, so we don't know the address yet.
				 */
				usb->address = libusb_get_device_address(devlist[i]);
		} else {
			sr_err("Failed to open device: %s.",
			       libusb_error_name(ret));
			break;
		}

		ret = command_get_fw_version(usb->devhdl, &vi);
		if (ret != SR_OK) {
			sr_err("Failed to get firmware version.");
			break;
		}

		ret = command_get_revid_version(sdi, &revid);
		if (ret != SR_OK) {
			sr_err("Failed to get REVID.");
			break;
		}

		/*
		 * Changes in major version mean incompatible/API changes, so
		 * bail out if we encounter an incompatible version.
		 * Different minor versions are OK, they should be compatible.
		 */
		if (vi.major != FX2LAFW_REQUIRED_VERSION_MAJOR) {
			sr_err("Expected firmware version %d.x, "
			       "got %d.%d.", FX2LAFW_REQUIRED_VERSION_MAJOR,
			       vi.major, vi.minor);
			break;
		}

		sdi->status = SR_ST_ACTIVE;
		sr_info("Opened device %d on %d.%d, "
			"interface %d, firmware %d.%d.",
			sdi->index, usb->bus, usb->address,
			USB_INTERFACE, vi.major, vi.minor);

		sr_info("Detected REVID=%d, it's a Cypress CY7C68013%s.",
			revid, (revid != 1) ? " (FX2)" : "A (FX2LP)");

		break;
	}
	libusb_free_device_list(devlist, 1);

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR;

	return SR_OK;
}

SR_PRIV int fx2lafw_configure_channels(const struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_channel *ch;
	GSList *l;
	int channel_bit, stage, i;
	char *tc;

	devc = sdi->priv;
	for (i = 0; i < NUM_TRIGGER_STAGES; i++) {
		devc->trigger_mask[i] = 0;
		devc->trigger_value[i] = 0;
	}

	stage = -1;
	for (l = sdi->channels; l; l = l->next) {
		ch = (struct sr_channel *)l->data;
		if (ch->enabled == FALSE)
			continue;

		if (devc->dslogic)
		{
			if ((ch->index > 7 && ch->type == SR_CHANNEL_LOGIC) ||
					(ch->index > 0 && ch->type == SR_CHANNEL_ANALOG))
				devc->sample_wide = TRUE;
			else
				devc->sample_wide = FALSE;
		}
		else
		{
			if (ch->index > 7)
				devc->sample_wide = TRUE;
		}

		channel_bit = 1 << (ch->index);
		if (!(ch->trigger))
			continue;

		stage = 0;
		for (tc = ch->trigger; *tc; tc++) {
			devc->trigger_mask[stage] |= channel_bit;
			if (*tc == '1')
				devc->trigger_value[stage] |= channel_bit;
			stage++;
			if (stage > NUM_TRIGGER_STAGES)
				return SR_ERR;
		}
	}

	if (stage == -1)
		/*
		 * We didn't configure any triggers, make sure acquisition
		 * doesn't wait for any.
		 */
		devc->trigger_stage = TRIGGER_FIRED;
	else
		devc->trigger_stage = 0;

	return SR_OK;
}

SR_PRIV struct dev_context *fx2lafw_dev_new(void)
{
	struct dev_context *devc;

	if (!(devc = g_try_malloc(sizeof(struct dev_context)))) {
		sr_err("Device context malloc failed.");
		return NULL;
	}

	devc->profile = NULL;
	devc->fw_updated = 0;
	devc->cur_samplerate = 0;
	devc->limit_samples = 0;
	devc->sample_wide = FALSE;

	devc->dslogic_ext_clock = FALSE;
	devc->dslogic_test = DSLOGIC_TEST_NONE;

	return devc;
}

SR_PRIV void fx2lafw_abort_acquisition(struct dev_context *devc)
{
	int i;

	devc->num_samples = -1;

	for (i = devc->num_transfers - 1; i >= 0; i--) {
		if (devc->transfers[i])
			libusb_cancel_transfer(devc->transfers[i]);
	}
}

static void finish_acquisition(struct dev_context *devc)
{
	struct sr_datafeed_packet packet;

	/* Terminate session. */
	packet.type = SR_DF_END;
	sr_session_send(devc->cb_data, &packet);

	/* Remove fds from polling. */
	usb_source_remove(devc->ctx);

	devc->num_transfers = 0;
	g_free(devc->transfers);
}

void fx2lafw_free_transfer(struct libusb_transfer *transfer)
{
	struct dev_context *devc;
	unsigned int i;

	devc = transfer->user_data;

	g_free(transfer->buffer);
	transfer->buffer = NULL;
	libusb_free_transfer(transfer);

	for (i = 0; i < devc->num_transfers; i++) {
		if (devc->transfers[i] == transfer) {
			devc->transfers[i] = NULL;
			break;
		}
	}

	devc->submitted_transfers--;
	if (devc->submitted_transfers == 0)
		finish_acquisition(devc);
}

static void resubmit_transfer(struct libusb_transfer *transfer)
{
	int ret;

	if ((ret = libusb_submit_transfer(transfer)) == LIBUSB_SUCCESS)
		return;

	fx2lafw_free_transfer(transfer);
	/* TODO: Stop session? */

	sr_err("%s: %s", __func__, libusb_error_name(ret));
}

SR_PRIV void fx2lafw_receive_transfer(struct libusb_transfer *transfer)
{
	gboolean packet_has_error = FALSE;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_logic logic;
	struct sr_datafeed_analog analog;
	struct dev_context *devc;
	int trigger_offset, i, sample_width, cur_sample_count;
	int trigger_offset_bytes;
	uint8_t *cur_buf;
	uint16_t cur_sample;

	devc = transfer->user_data;

	/*
	 * If acquisition has already ended, just free any queued up
	 * transfer that come in.
	 */
	if (devc->num_samples == -1) {
		fx2lafw_free_transfer(transfer);
		return;
	}

	sr_info("receive_transfer(): status %d received %d bytes.",
		transfer->status, transfer->actual_length);

	/* Save incoming transfer before reusing the transfer struct. */
	cur_buf = transfer->buffer;
	sample_width = devc->sample_wide ? 2 : 1;
	cur_sample_count = transfer->actual_length / sample_width;

	switch (transfer->status) {
	case LIBUSB_TRANSFER_NO_DEVICE:
		fx2lafw_abort_acquisition(devc);
		fx2lafw_free_transfer(transfer);
		return;
	case LIBUSB_TRANSFER_COMPLETED:
	case LIBUSB_TRANSFER_TIMED_OUT: /* We may have received some data though. */
		break;
	default:
		packet_has_error = TRUE;
		break;
	}

	if (transfer->actual_length == 0 || packet_has_error) {
		devc->empty_transfer_count++;
		if (devc->empty_transfer_count > MAX_EMPTY_TRANSFERS) {
			/*
			 * The FX2 gave up. End the acquisition, the frontend
			 * will work out that the samplecount is short.
			 */
			fx2lafw_abort_acquisition(devc);
			fx2lafw_free_transfer(transfer);
		} else {
			resubmit_transfer(transfer);
		}
		return;
	} else {
		devc->empty_transfer_count = 0;
	}

	trigger_offset = 0;
	if (devc->trigger_stage >= 0) {
		for (i = 0; i < cur_sample_count; i++) {

			cur_sample = devc->sample_wide ?
				*((uint16_t *)cur_buf + i) :
				*((uint8_t *)cur_buf + i);

			if ((cur_sample & devc->trigger_mask[devc->trigger_stage]) ==
				devc->trigger_value[devc->trigger_stage]) {
				/* Match on this trigger stage. */
				devc->trigger_buffer[devc->trigger_stage] = cur_sample;
				devc->trigger_stage++;

				if (devc->trigger_stage == NUM_TRIGGER_STAGES ||
					devc->trigger_mask[devc->trigger_stage] == 0) {
					/* Match on all trigger stages, we're done. */
					trigger_offset = i + 1;

					/*
					 * TODO: Send pre-trigger buffer to session bus.
					 * Tell the frontend we hit the trigger here.
					 */
					packet.type = SR_DF_TRIGGER;
					packet.payload = NULL;
					sr_session_send(devc->cb_data, &packet);

					/*
					 * Send the samples that triggered it,
					 * since we're skipping past them.
					 */
					packet.type = SR_DF_LOGIC;
					packet.payload = &logic;
					logic.unitsize = sample_width;
					logic.length = devc->trigger_stage * logic.unitsize;
					logic.data = devc->trigger_buffer;
					sr_session_send(devc->cb_data, &packet);

					devc->trigger_stage = TRIGGER_FIRED;
					break;
				}
			} else if (devc->trigger_stage > 0) {
				/*
				 * We had a match before, but not in the next sample. However, we may
				 * have a match on this stage in the next bit -- trigger on 0001 will
				 * fail on seeing 00001, so we need to go back to stage 0 -- but at
				 * the next sample from the one that matched originally, which the
				 * counter increment at the end of the loop takes care of.
				 */
				i -= devc->trigger_stage;
				if (i < -1)
					i = -1; /* Oops, went back past this buffer. */
				/* Reset trigger stage. */
				devc->trigger_stage = 0;
			}
		}
	}

	if (devc->trigger_stage == TRIGGER_FIRED) {
		/* Send the incoming transfer to the session bus. */
		trigger_offset_bytes = trigger_offset * sample_width;
		if (!devc->dslogic || devc->dslogic_mode == DSLOGIC_MODE_LOGIC)
		{
			packet.type = SR_DF_LOGIC;
			packet.payload = &logic;
			logic.length = transfer->actual_length - trigger_offset_bytes;
			logic.unitsize = sample_width;
			logic.data = cur_buf + trigger_offset_bytes;
		} else {
			packet.type = SR_DF_ANALOG;
			packet.payload = &analog;
			analog.num_samples = transfer->actual_length / sample_width;
			analog.mq = SR_MQ_VOLTAGE;
			analog.mqflags = 0;
			analog.data = (float *) (cur_buf + trigger_offset_bytes);
		}

		if (devc->dslogic) {
			if ((devc->limit_samples && devc->num_samples < devc->limit_samples)
					|| devc->dslogic_mode != DSLOGIC_MODE_LOGIC) {
				uint64_t remaining_length =
					(devc->limit_samples - devc->num_samples) * sample_width;
				logic.length = MIN(logic.length, remaining_length);

				/* In test mode, check data content */
				if (devc->dslogic_test == DSLOGIC_TEST_INTERNAL) {
					for (i = 0; i < logic.length / 2; i++) {
						const uint16_t cur_sample =
							*((const uint16_t*)cur_buf + i);
						if (devc->dslogic_test_init == 1) {
							devc->dslogic_test_sample_value = cur_sample;
							devc->dslogic_test_init = 0;
						}
						if (cur_sample != devc->dslogic_test_sample_value)
							break;
						devc->dslogic_test_sample_value++;
					}
				}
				if (devc->dslogic_test == DSLOGIC_TEST_EXTERNAL) {
					for (i = 0; i < logic.length / 2; i++) {
						const uint16_t cur_sample =
							*((const uint16_t*)cur_buf + i);
						if (devc->dslogic_test_init == 1) {
							devc->dslogic_test_sample_value = cur_sample;
							devc->dslogic_test_init = 0;
						}
						if (cur_sample != devc->dslogic_test_sample_value)
							sr_err("exp: %d; act: %d",
									devc->dslogic_test_sample_value, cur_sample);
							break;
						}
						devc->dslogic_test_sample_value =
							(devc->dslogic_test_sample_value + 1) % 65001;
					}
				}

				sr_session_send(devc->cb_data, &packet);
		} else {
			sr_session_send(devc->cb_data, &packet);
		}

		devc->num_samples += cur_sample_count;
		if (devc->limit_samples &&
			(unsigned int)devc->num_samples > devc->limit_samples) {
			fx2lafw_abort_acquisition(devc);
			fx2lafw_free_transfer(transfer);
			return;
		}
	} else {
		/*
		 * TODO: Buffer pre-trigger data in capture
		 * ratio-sized buffer.
		 */
	}

	resubmit_transfer(transfer);
}

static unsigned int to_bytes_per_ms(struct dev_context *devc)
{
	return devc->cur_samplerate / 1000
		* ((devc->dslogic && devc->sample_wide) ? 2 : 1);
}

SR_PRIV size_t fx2lafw_get_buffer_size(struct dev_context *devc)
{
	size_t s;

	/*
	 * The buffer should be large enough to hold 10ms of data and
	 * a multiple of 512.
	 */
	s = 10 * to_bytes_per_ms(devc);
	return (s + 511) & ~511;
}

SR_PRIV unsigned int fx2lafw_get_number_of_transfers(struct dev_context *devc)
{
	unsigned int n;

	/* Total buffer size should be able to hold about 500ms of data,
	 * or 100ms for DSLogic. */
	n = ((devc->dslogic ? 100 : 500) * to_bytes_per_ms(devc) /
		fx2lafw_get_buffer_size(devc));

	if (n > NUM_SIMUL_TRANSFERS)
		return NUM_SIMUL_TRANSFERS;

	return n;
}

SR_PRIV unsigned int fx2lafw_get_timeout(struct dev_context *devc)
{
	size_t total_size;
	unsigned int timeout;

	if (devc->dslogic)
		return 1000;

	total_size = fx2lafw_get_buffer_size(devc) *
			fx2lafw_get_number_of_transfers(devc);
	timeout = total_size / to_bytes_per_ms(devc);
	return timeout + timeout / 4; /* Leave a headroom of 25% percent. */
}
