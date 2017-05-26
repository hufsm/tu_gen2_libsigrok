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

#define REQ_TIMEOUT_MS 500

SR_PRIV int gw_instek_psp_send_cmd(struct sr_serial_dev_inst *serial, const char *cmd, ...)
{
	int ret;
	char cmdbuf[50];
	char *cmd_esc;
	va_list args;
  sr_dbg( "%s", __FUNCTION__);

	va_start(args, cmd);
	vsnprintf(cmdbuf, sizeof(cmdbuf), cmd, args);
	va_end(args);

	cmd_esc = g_strescape(cmdbuf, NULL);
	sr_dbg("Sending '%s'.", cmd_esc);
	g_free(cmd_esc);

	if ((ret = serial_write_blocking(serial, cmdbuf, strlen(cmdbuf),
			serial_timeout(serial, strlen(cmdbuf)))) < 0) {
		sr_err("Error sending command: %d.", ret);
		return ret;
	}

	return ret;
}

/**
 * Read data from interface into buffer blocking until @a lines number of \\r chars
 * received.
 * @param serial Previously initialized serial port structure.
 * @param[in] lines Number of \\r-terminated lines to read (1-n).
 * @param     buf Buffer for result. Contents is NUL-terminated on success.
 * @param[in] buflen Buffer length (>0).
 * @retval SR_OK Lines received and ending with "OK\r" (success).
 * @retval SR_ERR Error.
 * @retval SR_ERR_ARG Invalid argument.
 */
SR_PRIV int gw_instek_psp_read_reply(struct sr_serial_dev_inst *serial, int lines, char *buf, int buflen)
{
	int l_recv = 0;
	int bufpos = 0;
	int retc;
  sr_dbg( "%s", __FUNCTION__);

	if (!serial || (lines <= 0) || !buf || (buflen <= 0))
		return SR_ERR_ARG;

	while ((l_recv < lines) && (bufpos < (buflen + 1))) {
		retc = serial_read_blocking(serial, &buf[bufpos], 1, 0);
		if (retc != 1)
			return SR_ERR;
		if (buf[bufpos] == '\r')
			l_recv++;
		bufpos++;
	}
	buf[bufpos] = '\0';

	sr_dbg("got: '%s'.", buf);
	if ((l_recv == lines) && (g_str_has_suffix(buf, "\r")))
		return SR_OK;
	else
		return SR_ERR;
}

/**
 * Interpret result of L command.
 *
 * @param[in] sr_dev_inst *sdi
 *            A pointer to the device instance
 * @param[in] char** tokens
 *            The tokens from the response as two dimensional array
 * @brief:
 *    This function will parse the device responce and will read out the values for voltage and current.
 *    The values will be returned to the given device instance in *sdi.
 *
 *    From the device we will get an answer like this:
 *        'Vvv.vv<cr><cr><lf>
 *        Aa.aaa<cr><cr><lf>'
 *
 *    This function should get called after the use of parse_reply(). That leads to a slightly nicer
 *    format for the values, that should look like this:
 *        tokens[0] = "Vvv.vv"
 *        tokens[1] = "Aa.aaa"
 *
 *    Somtimes there are leading '<cr>' and/ or <lf> in the string we will read from the device.
 *    Those are handled accordingly.
 */
SR_PRIV int gw_instek_psp_parse_volt_curr_mode(struct sr_dev_inst *sdi, char **tokens)
{
	double voltage;
	double current;
	struct dev_context *devc;

  char* voltage_start;
  char* current_start;
  char* voltage_str;
  char* current_str;

  sr_dbg( "%s", __FUNCTION__);

	devc = sdi->priv;

  // get the start of the values for voltage and current from the string
  voltage_start = g_strrstr(tokens[0], "V");
  current_start = g_strrstr(tokens[1], "A");

  // construct a new string for temporary use
  voltage_str = g_strndup(voltage_start + 1, 5);
  current_str = g_strndup(current_start + 1, 5);

  if( voltage_start != NULL &&
      current_start != NULL)
  {
      sr_dbg( "voltage_start: '%s' V", voltage_str);
      sr_dbg( "current_start: '%s' A", current_str);
  } else
  {
    return SR_ERR;
  }

  // convert to double
	voltage = g_ascii_strtod(voltage_str, NULL);
	current = g_ascii_strtod(current_str, NULL);

  sr_dbg( "voltage: %f V", voltage);
  sr_dbg( "current: %f A", current);

	devc->voltage = voltage;
	devc->current = current;

  g_free(voltage_str);
  g_free(current_str);

	/* Byte 8: Mode ('0' means CV, '1' means CC). */
	devc->cc_mode = (tokens[0][8] == '1');

	/* Output enabled? Works because voltage cannot be set to 0.0 directly. */
	devc->output_enabled = devc->voltage != 0.0;

  sr_dbg( "Resuls:");
  sr_dbg( "\tvoltage: %f", devc->voltage);
  sr_dbg( "\tcurrent: %f", devc->current);

	return SR_OK;
}

static void send_sample(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	struct sr_datafeed_packet packet;
	struct sr_datafeed_analog_old analog;
  sr_dbg( "%s", __FUNCTION__);

	devc = sdi->priv;

	packet.type = SR_DF_ANALOG_OLD;
	packet.payload = &analog;
	analog.channels = sdi->channels;
	analog.num_samples = 1;

	analog.mq = SR_MQ_VOLTAGE;
	analog.unit = SR_UNIT_VOLT;
	analog.mqflags = SR_MQFLAG_DC;
	analog.data = &devc->voltage;
	sr_session_send(sdi, &packet);

	analog.mq = SR_MQ_CURRENT;
	analog.unit = SR_UNIT_AMPERE;
	analog.mqflags = 0;
	analog.data = &devc->current;
	sr_session_send(sdi, &packet);

	devc->num_samples++;
}

static int parse_reply(struct sr_dev_inst *sdi)
{
	struct dev_context *devc;
	char *reply_esc, **tokens;
	int retc;
  sr_dbg( "%s", __FUNCTION__);

	devc = sdi->priv;

	reply_esc = g_strescape(devc->buf, NULL);
	sr_dbg("Received '%s'.", reply_esc);
	g_free(reply_esc);

  /*
   * Split right after the end sequence of each command. 
   * This will leave us with the strings containing the 
   * values for voltage and current.
   */
	tokens = g_strsplit(devc->buf, "\r\n", 0);

	retc = gw_instek_psp_parse_volt_curr_mode(sdi, tokens);
	g_strfreev(tokens);
	if (retc < 0)
		return SR_ERR;

	send_sample(sdi);

	return SR_OK;
}

static int handle_new_data(struct sr_dev_inst *sdi)
{
	int len;
	int expected_len = EXPECTED_VOLTAGE_CURRENT_MSG_LEN;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
  sr_dbg( "%s", __FUNCTION__);

	devc = sdi->priv;
	serial = sdi->conn;

  len = serial_read_blocking(serial, devc->buf + devc->buflen, 1, 0);
  if (len < 1)
    return SR_ERR;

  sr_dbg( "len is: '%i'", len);

  /* Sync with the true reply;
  * there schould be no leading
  * characters in the buffer
  */
  if (devc->buf[0]=='\r'
      || devc->buf[0]=='\n') {
    return SR_OK;
  }

  devc->buflen += len;
  devc->buf[devc->buflen] = '\0';

  sr_dbg( "Read from device: '%s'", devc->buf);
  sr_dbg( "Read from device: '%i'", devc->buflen);

  /* We expect two answers: 
   * 1. The output voltage of the device
   * 2. The current of the Device
   * The voltage is Identified by a leading character 'V'.
   * Current values have a leading 'A'. So we want to check for this values.
   */
  if ( !g_strrstr(devc->buf, "V"))
    return SR_OK;
  if ( !g_strrstr(devc->buf, "A"))
    return SR_OK;
  if ( devc->buflen < expected_len )
    return SR_OK;

	parse_reply(sdi);

	devc->buf[0] = '\0';
	devc->buflen = 0;

	devc->reply_pending = FALSE;

	return SR_OK;
}

/**
 * This function will generate a timeout, basically it will loop until the
 * timer elapses, which means that the given time in @see time has passed.
 *
 * @param[in] time: The time to wait in us
 *
 */
SR_PRIV void  gw_instek_psp_timeout(int time)
{
  int64_t s_0 = g_get_monotonic_time();
  int64_t s_1 = g_get_monotonic_time();
  int s_delta = 0;
  while( s_delta < time ) {
    s_1 = g_get_monotonic_time();
    s_delta = s_1 - s_0;
  }
}

/** Driver/serial data reception function. */
SR_PRIV int gw_instek_psp_receive_data(int fd, int revents, void *cb_data)
{
	struct sr_dev_inst *sdi;
	struct dev_context *devc;
	struct sr_serial_dev_inst *serial;
	int64_t t, elapsed_us;
  sr_dbg( "%s", __FUNCTION__);

	(void)fd;

	if (!(sdi = cb_data))
		return TRUE;

	if (!(devc = sdi->priv))
		return TRUE;

	serial = sdi->conn;

	if (revents == G_IO_IN) {
		/* New data arrived. */
		handle_new_data(sdi);
	} else {
		/* Timeout. */
	}

	if (devc->limit_samples && (devc->num_samples >= devc->limit_samples)) {
		sr_info("Requested number of samples reached.");
		sdi->driver->dev_acquisition_stop(sdi, cb_data);
		return TRUE;
	}

	if (devc->limit_msec) {
		t = (g_get_monotonic_time() - devc->starttime) / 1000;
		if (t > (int64_t)devc->limit_msec) {
			sr_info("Requested time limit reached.");
			sdi->driver->dev_acquisition_stop(sdi, cb_data);
			return TRUE;
		}
	}

	/* Request next packet, if required. */
	if (sdi->status == SR_ST_ACTIVE) {
		if (devc->reply_pending) {
			elapsed_us = g_get_monotonic_time() - devc->req_sent_at;
			if (elapsed_us > (REQ_TIMEOUT_MS * 1000))
				devc->reply_pending = FALSE;
			return TRUE;
		}

		/* Send commands to get voltage, current.
     * To safe time, we send two commands instead 
     * of just one which should be 'L'.
     * Because the device needs some time to respond, 
     * there is a timeout used.
     */
		if (gw_instek_psp_send_cmd(serial, "V\r") < 0)
      return TRUE;

    gw_instek_psp_timeout(SEND_COMMAND_TIMEOUT_US);

    if (gw_instek_psp_send_cmd(serial, "A\r") < 0)
      return TRUE;

		devc->req_sent_at = g_get_monotonic_time();
		devc->reply_pending = TRUE;
	}

	return TRUE;
}
