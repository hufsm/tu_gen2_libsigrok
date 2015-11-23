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
	SR_CONF_VOLTAGE | SR_CONF_GET | SR_CONF_SET,
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
  sr_dbg( "%s", __FUNCTION__);
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
  int ret = 0;
  int read_reply = 0;

  sr_dbg( "%s", __FUNCTION__);

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
      sr_dbg( "Branch SR_CONF_CONN %d", src->key);
			conn = g_variant_get_string(src->data, NULL);
			break;
		case SR_CONF_SERIALCOMM:
      sr_dbg( "Branch SR_CONF_SERIALCOMM %d", src->key);
			serialcomm = g_variant_get_string(src->data, NULL);
			break;
		default:
      sr_dbg( "Unknown option %d", src->key);
			sr_err("Unknown option %d, skipping.", src->key);
			break;
		}
	}

	if (!conn)
		return NULL;
	if (!serialcomm)
		serialcomm = "2400/8n1";

	serial = sr_serial_dev_inst_new(conn, serialcomm);
  sr_dbg( "conn= %s", conn);
  sr_dbg( "serialcomm= %s", serialcomm);

	if (serial_open(serial, SERIAL_RDWR) != SR_OK) {
    sr_dbg( "ser could not be opened");
    return NULL;
  }

	serial_flush(serial);

	sr_info("Probing serial port %s.", conn);

	/* Init device instance, etc. */
	sdi = g_malloc0(sizeof(struct sr_dev_inst));
	sdi->status = SR_ST_INACTIVE;
	sdi->vendor = g_strdup("GW Instek");
	sdi->model = g_strdup(models[0].name);
	sdi->inst_type = SR_INST_SERIAL;
	sdi->conn = serial;
	sdi->driver = di;

	sr_channel_new(sdi, 0, SR_CHANNEL_ANALOG, TRUE, "CH1");

	devc = g_malloc0(sizeof(struct dev_context));
	devc->model = &models[0];

	sdi->priv = devc;

  ret = gw_instek_psp_send_cmd(serial, "L\r");

  sr_dbg( "Asking device...");
  read_reply = gw_instek_psp_read_reply(serial, 1, reply, sizeof(reply));

  sr_dbg( "Device said:  %s", reply);
  sr_dbg( "       code:  %i", read_reply);

	tokens = g_strsplit((const gchar *)&reply, "\r", 1);
	if (gw_instek_psp_parse_volt_curr_mode(sdi, tokens) < 0) {
		g_strfreev(tokens);
		goto exit_err;
	}
	g_strfreev(tokens);

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
  sr_dbg( "%s", __FUNCTION__);
	return ((struct drv_context *)(di->context))->instances;
}

static int dev_clear(const struct sr_dev_driver *di)
{
  sr_dbg( "%s", __FUNCTION__);
	return std_dev_clear(di, NULL);
}

static int dev_open(struct sr_dev_inst *sdi)
{
  sr_dbg( "%s", __FUNCTION__);
	(void)sdi;

	/* TODO: get handle from sdi->conn and open it. */

	sdi->status = SR_ST_ACTIVE;

	return SR_OK;
}

static int dev_close(struct sr_dev_inst *sdi)
{
  sr_dbg( "%s", __FUNCTION__);
	(void)sdi;

	/* TODO: get handle from sdi->conn and close it. */

	sdi->status = SR_ST_INACTIVE;

	return SR_OK;
}

static int cleanup(const struct sr_dev_driver *di)
{
  sr_dbg( "%s", __FUNCTION__);
	dev_clear(di);

	/* TODO: free other driver resources, if any. */

	return SR_OK;
}

static int config_get(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;

  sr_dbg( "%s", __FUNCTION__);
	(void)cg;

	if (!sdi)
		return SR_ERR_ARG;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_SAMPLES:
		*data = g_variant_new_uint64(devc->limit_samples);
		break;
	case SR_CONF_LIMIT_MSEC:
		*data = g_variant_new_uint64(devc->limit_msec);
		break;
	case SR_CONF_VOLTAGE:
		*data = g_variant_new_double(devc->voltage);
		break;
	case SR_CONF_VOLTAGE_TARGET:
		*data = g_variant_new_double(devc->voltage_max);
		break;
	case SR_CONF_CURRENT:
		*data = g_variant_new_double(devc->current);
		break;
	case SR_CONF_CURRENT_LIMIT:
		*data = g_variant_new_double(devc->current_max);
		break;
	case SR_CONF_ENABLED:
		*data = g_variant_new_boolean(devc->output_enabled);
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

/**
 * This function is responsible for setting the device parameter
 *
 */
static int config_set(uint32_t key, GVariant *data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	gboolean bval;
	gdouble dval;

  sr_dbg( "%s", __FUNCTION__);
  sr_dbg( "key: %i", key);

	(void)cg;

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	devc = sdi->priv;

	switch (key) {
	case SR_CONF_LIMIT_MSEC:
		if (g_variant_get_uint64(data) == 0)
			return SR_ERR_ARG;
		devc->limit_msec = g_variant_get_uint64(data);
		break;

	case SR_CONF_LIMIT_SAMPLES:
		if (g_variant_get_uint64(data) == 0)
			return SR_ERR_ARG;
		devc->limit_samples = g_variant_get_uint64(data);
    sr_dbg("Limit Samples: %i",devc->limit_samples);
		break;

	case SR_CONF_VOLTAGE:
    break;

	case SR_CONF_VOLTAGE_TARGET:
		dval = g_variant_get_double(data);
    sr_dbg( "setting voltage (key:%i)", key);
    sr_dbg( "                 to: %f", dval);
    sr_dbg( "        max voltage:: %f", devc->voltage_max);

    gw_instek_psp_send_cmd(sdi->conn, "SV %05.2f\r", dval);
		devc->voltage_max = dval;

    sr_dbg( "        max voltage:: %f", devc->voltage_max);
		break;

	case SR_CONF_CURRENT_LIMIT:
		dval = g_variant_get_double(data);
    sr_dbg( "setting current_limit (key:%i)", key);
    sr_dbg( "                 to: %f", dval);
    sr_dbg( "        max current: %f", devc->current_max);

    gw_instek_psp_send_cmd(sdi->conn, "SI %4.2f\r", dval);
    
    sr_dbg( "current %i", devc->model->current[0] ) ;
    sr_dbg( "max device %f", devc->current_max_device );
//		if (dval < devc->model->current[0] || dval > devc->current_max_device)
//			return SR_ERR_ARG;
//
//		if ((gw_instek_psp_send_cmd(sdi->conn, "SI %05.2f\r", dval) < 0) ||
//        (gw_instek_psp_read_reply(sdi->conn, 1, devc->buf, sizeof(devc->buf)) < 0))
//			return SR_ERR;
		devc->current_max = dval;
		break;

	case SR_CONF_ENABLED:
		bval = g_variant_get_boolean(data);
		if (bval == devc->output_enabled) /* Nothing to do. */
			break;

    if (bval)
    {
      if (gw_instek_psp_send_cmd(sdi->conn, "KOE\r") < 0)
        return SR_ERR;
    }
    else
    {
      if (gw_instek_psp_send_cmd(sdi->conn, "KOD\r") < 0)
        return SR_ERR;
    }
		devc->output_enabled = bval;
		break;
	default:
		return SR_ERR_NA;
	}

	return SR_OK;
}

static int config_list(uint32_t key, GVariant **data,
	const struct sr_dev_inst *sdi, const struct sr_channel_group *cg)
{
	struct dev_context *devc;
	GVariant *gvar;
	GVariantBuilder gvb;
	double dval;
	int idx;

  sr_dbg( "%s", __FUNCTION__);
  sr_dbg( "with key: %i", key);

	(void)cg;

	/* Always available (with or without sdi). */
	if (key == SR_CONF_SCAN_OPTIONS) {
    sr_dbg( "%s SR_CONF_SCAN_OPTIONS", __FUNCTION__);
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
			scanopts, ARRAY_SIZE(scanopts), sizeof(uint32_t));
		return SR_OK;
	}

	/* Return drvopts without sdi (and devopts with sdi, see below). */
	if (key == SR_CONF_DEVICE_OPTIONS && !sdi) {
    sr_dbg( "%s SR_CONF_DEVICE_OPTIONS", __FUNCTION__);
		*data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
				drvopts, ARRAY_SIZE(drvopts), sizeof(uint32_t));
		return SR_OK;
	}

	/* Every other key needs an sdi. */
	if (!sdi)
		return SR_ERR_ARG;
	devc = sdi->priv;

	switch (key) {
    case SR_CONF_DEVICE_OPTIONS:
      *data = g_variant_new_fixed_array(G_VARIANT_TYPE_UINT32,
          devopts, ARRAY_SIZE(devopts), sizeof(uint32_t));
      break;

    case SR_CONF_VOLTAGE_TARGET:
      sr_dbg("SR_CONF_VOLTAGE_TARGET");
      g_variant_builder_init(&gvb, G_VARIANT_TYPE_ARRAY);
      /* Min, max, step. */
      for (idx = 0; idx < 3; idx++) {
        if (idx == 1)
          dval = devc->voltage_max_device;
        else
          dval = devc->model->voltage[idx];
        gvar = g_variant_new_double(dval);
        g_variant_builder_add_value(&gvb, gvar);
      }
      *data = g_variant_builder_end(&gvb);
      break;

    case SR_CONF_CURRENT_LIMIT:
      g_variant_builder_init(&gvb, G_VARIANT_TYPE_ARRAY);
      /* Min, max, step. */
      for (idx = 0; idx < 3; idx++) {
        if (idx == 1)
          dval = devc->current_max_device;
        else
          dval = devc->model->current[idx];
        gvar = g_variant_new_double(dval);
        g_variant_builder_add_value(&gvb, gvar);
      }
      *data = g_variant_builder_end(&gvb);
      break;

    default:
      return SR_ERR_NA;
	}

	return SR_OK;
}

static int dev_acquisition_start(const struct sr_dev_inst *sdi, void *cb_data)
{
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;

  sr_dbg( "%s", __FUNCTION__);

	if (sdi->status != SR_ST_ACTIVE)
		return SR_ERR_DEV_CLOSED;

	devc = sdi->priv;
	devc->cb_data = cb_data;

	/* Send header packet to the session bus. */
	std_session_send_df_header(cb_data, LOG_PREFIX);

	devc->starttime = g_get_monotonic_time();
	devc->num_samples = 0;
	devc->reply_pending = FALSE;
	devc->req_sent_at = 0;

	/* Poll every 100ms, or whenever some data comes in. */
	serial = sdi->conn;
	serial_source_add(sdi->session, serial, G_IO_IN, 100,
			gw_instek_psp_receive_data, (void *)sdi);

	return SR_OK;
}

static int dev_acquisition_stop(struct sr_dev_inst *sdi, void *cb_data)
{
	return std_serial_dev_acquisition_stop(sdi, cb_data,
			std_serial_dev_close, sdi->conn, LOG_PREFIX);
}

SR_PRIV struct sr_dev_driver gwinstek_pps_psp_driver_info = {
	.name = "gwinstek-pps-psp",
	.longname = "GW Instek Power Supplies PSP-xxx",
	.api_version = 1,
	.init = init,
	.cleanup = cleanup,
	.scan = scan,
	.dev_list = dev_list,
	.dev_clear = dev_clear,
	.config_get = config_get,
	.config_set = config_set,
	.config_list = config_list,
	.dev_open = std_serial_dev_open,
	.dev_close = std_serial_dev_close,
	.dev_acquisition_start = dev_acquisition_start,
	.dev_acquisition_stop = dev_acquisition_stop,
	.context = NULL,
};
