/*
 * This file is part of the libsigrok project.
 *
 * Copyright (C) 2015 Christoph <Christoph.Gnip@gmail.com>
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

#include <config.h>
#include "protocol.h"

#include "stdio.h"

static const uint32_t drvopts[] = {
	/* Device class */
	SR_CONF_POWER_SUPPLY,
};

static const uint32_t devopts[] = {
	/* Device class */
	SR_CONF_POWER_SUPPLY,
	/* Acquisition modes. */
	SR_CONF_CONTINUOUS,
	SR_CONF_LIMIT_SAMPLES | SR_CONF_GET | SR_CONF_SET,
	SR_CONF_LIMIT_MSEC | SR_CONF_GET | SR_CONF_SET,
	/* Device configuration */
	SR_CONF_VOLTAGE | SR_CONF_GET,
	SR_CONF_VOLTAGE_TARGET | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_CURRENT | SR_CONF_GET,
	SR_CONF_CURRENT_LIMIT | SR_CONF_GET | SR_CONF_SET | SR_CONF_LIST,
	SR_CONF_ENABLED | SR_CONF_GET | SR_CONF_SET,
};

static const uint32_t scanopts[] = {
	SR_CONF_CONN,
	SR_CONF_SERIALCOMM,
};

/* Note: All models have one power supply output only. */
static const struct gw_instek_psp_model models[] = {
	{ GW_INSTEK_PPS_PSP, "GW INSTEK PSP-405",     "405", { 1, 20, 0.1 }, { 0, 10,   0.10 } },
	{ 0, NULL, NULL, { 0, 0, 0 }, { 0, 0, 0 }, },
};

SR_PRIV struct sr_dev_driver gwinstek_pps_psp_driver_info;

static int init(struct sr_dev_driver *di, struct sr_context *sr_ctx)
{
  fprintf(stdout, "%s\n", __FUNCTION__);
	return std_init(sr_ctx, di, LOG_PREFIX);
}

static GSList *scan(struct sr_dev_driver *di, GSList *options)
{
	struct drv_context *drvc;
	struct dev_context *devc;
	struct sr_dev_inst *sdi;
	struct sr_config *src;
	GSList *devices, *l;
	const char *conn, *serialcomm;
	struct sr_serial_dev_inst *serial;
	char reply[50], **tokens;

  fprintf(stdout, "%s\n", __FUNCTION__);

	drvc = di->context;
	drvc->instances = NULL;
	devices = NULL;
	conn = NULL;
	serialcomm = NULL;
	devc = NULL;
  
	sr_info("Sending ...");

	for (l = options; l; l = l->next) {
		src = l->data;
		switch (src->key) {
		case SR_CONF_CONN:
      fprintf(stdout, "Branch SR_CONF_CONN %d\n", src->key);
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
      fprintf(stdout, "Branch SR_CONF_SERIALCOMM %d\n", src->key);
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		default:
      fprintf(stdout, "Unknown option %d\n", src->key);
			sr_err("Unknown option %d, skipping.", src->key);
			break;
		}
	}

	if (!conn)
		return NULL;
	if (!serialcomm)
		serialcomm = "2400/8n1";


	serial = sr_serial_dev_inst_new(conn, serialcomm);
  fprintf(stdout, "conn= %s\n", conn);
  fprintf(stdout, "serialcomm= %s\n", serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK) {
    fprintf(stdout, "ser could not be opened\n");
    return NULL;
  }

	serial_flush(serial);

  fprintf(stdout, "Probing serial port %s\n", conn);
	sr_info("Probing serial port %s.", conn);


	/* Init device instance, etc. */
	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("GW Instek");
	sdi->model = g_strdup(models[0].name);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->driver = di;

	devc = g_malloc0(sizeof(struct dev_context));
	devc->model = &models[0];

	sdi->priv = devc;

  int ret = 0;
  ret = gw_instek_psp_send_cmd(serial, "L\r");
  fprintf(stdout, "gw_instek_psp_send_cmd: %i\n", ret);
  int i = 0;

  scanf(stdin);
  fprintf(stdout, "Asking device...\n", ret);
  gw_instek_psp_read_reply(serial, 1, reply, sizeof(reply));
  fprintf(stdout, "gw_instek_psp_read_reply %s\n", reply);

	/* Get current voltage, current, status, limits. */
	/*if((gw_instek_psp_send_cmd(serial, "L\r") < 0) ||
    (gw_instek_psp_read_reply(serial, 1, reply, sizeof(reply)) < 0))
		goto exit_err;
	tokens = g_strsplit((const gchar *)&reply, "\r", 1);
	if (gw_instek_psp_parse_volt_curr_mode(sdi, tokens) < 0) {
		g_strfreev(tokens);
		goto exit_err;
	}*/
	//g_strfreev(tokens);

	drvc->instances = g_slist_append(drvc->instances, sdi);
	devices = g_slist_append(devices, sdi);

	serial_close(serial);
	if (!devices)
		sr_serial_dev_inst_free(serial);

	return devices;

exit_err:
	sr_dev_inst_free(sdi);
	g_free(devc);

	return NULL;
}

static GSList *dev_list(const struct sr_dev_driver *di)
{
  fprintf(stdout, "%s\n", __FUNCTION__);
	return ((struct drv_context *)(di->context))->instances;
}

static int dev_clear(const struct sr_dev_driver *di)
{
  fprintf(stdout, "%s\n", __FUNCTION__);
	return std_dev_clear(di, NULL);
}

static int dev_open(struct sr_dev_inst *sdi)
{
  fprintf(stdout, "%s\n", __FUNCTION__);
	(void)sdi;

	/* TODO: get handle from sdi->conn and open it. */

	sdi->status = SR_ST_ACTIVE;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
  fprintf(stdout, "%s\n", __FUNCTION__);
	(void)sdi;

	/* TODO: get handle from sdi->conn and close it. */

	sdi->status = SR_ST_INACTIVE;

	return SR_OK;
}

static int cleanup(const struct sr_dev_driver *di)
{
  fprintf(stdout, "%s\n", __FUNCTION__);
	dev_clear(di);

	/* TODO: free other driver resources, if any. */

	return SR_OK;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;

	(void)sdi;
	(void)data;
	(void)cg;

  fprintf(stdout, "%s", __FUNCTION__);

	ret = SR_OK;
	switch (key) {
	/* TODO */
	default:
		return SR_ERR_NA;
	}

	return ret;
}

static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;
  fprintf(stdout, "%s", __FUNCTION__);

	(void)data;
	(void)cg;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	ret = SR_OK;
	switch (key) {
	/* TODO */
	default:
		ret = SR_ERR_NA;
	}

	return ret;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	int ret;

	(void)sdi;
	(void)data;
	(void)cg;
  fprintf(stdout, "%s\n", __FUNCTION__);

	ret = SR_OK;
	switch (key) {
	case SR_CONF_SCAN_OPTIONS:
    fprintf(stdout, "SR_CONF_SCAN_OPTIONS %s\n", __FUNCTION__);

		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
			scanopts, ARRAY_SIZE(scanopts), sizeof(uint32_t));
    fprintf(stdout, "SR_CONF_SCAN_OPTIONS %s\n", __FUNCTION__);
		return SR_OK;
	case SR_CONF_DEVICE_OPTIONS:
    fprintf(stdout, "SR_CONF_DEVICE_OPTIONS %s\n", __FUNCTION__);
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
			devopts, ARRAY_SIZE(devopts), sizeof(uint32_t));
		return SR_OK;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, void *cb_data)
{
	(void)sdi;
	(void)cb_data;

  fprintf(stdout, "%s", __FUNCTION__);
	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	/* TODO: configure hardware, reset acquisition state, set up
	 * callbacks and send header packet. */

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	(void)cb_data;
  fprintf(stdout, "%s", __FUNCTION__);

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	/* TODO: stop acquisition. */

	return SR_OK;
}

SR_PRIV struct sr_dev_driver gwinstek_pps_psp_driver_info = {
	.name = "gwinstek-pps-psp",
	.longname = "gwinstek-pps-psp",
	.api_version = 1,
	.init = init,
	.cleanup = cleanup,
	.scan = scan,
	.dev_list = dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = dev_open,
	.dev_close = dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
