/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for QCT platform
 *
 *  Copyright (C) 2019 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_common.h"
#include "himax_platform_SPI.h"
#include <linux/spi/spi.h>


int i2c_error_count;
struct spi_device *spi;

static uint8_t *gBuffer;

int himax_dev_set(struct himax_ts_data *ts)
{
	int ret = 0;

	ts->input_dev = devm_input_allocate_device(&ts->spi->dev);
	if (ts->input_dev == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input device\n", __func__);
		return ret;
	}
	ts->input_dev->id.bustype = BUS_SPI;
	ts->input_dev->dev.parent = &ts->spi->dev;
	ts->input_dev->name = "sec_touchscreen";

	if (ts->pdata->support_dex) {
		ts->input_dev_pad = devm_input_allocate_device(&ts->spi->dev);
		if (ts->input_dev_pad == NULL) {
			E("%s: Failed to allocate dex input device\n", __func__);
		} else {
			ts->input_dev_pad->id.bustype = BUS_SPI;
			ts->input_dev_pad->dev.parent = &ts->spi->dev;
			ts->input_dev_pad->name = "sec_touchpad";
		}
	}

#ifdef HX_TOUCH_PROXIMITY
	ts->input_dev_proximity = input_allocate_device();

	if (ts->input_dev_proximity == NULL) {
		ret = -ENOMEM;
		E("%s: Failed to allocate input_dev_proximity device\n", __func__);
		return ret;
	}

	ts->input_dev_proximity->name = "sec_touchproximity";
#endif

	return ret;
}

int himax_input_register_device(struct input_dev *input_dev)
{
	return input_register_device(input_dev);
}

#if defined(HX_PLATFOME_DEFINE_KEY)
void himax_platform_key(void)
{
	I("Nothing to be done! Plz cancel it!\n");
}
#endif

void himax_vk_parser(struct device_node *dt,
						struct himax_i2c_platform_data *pdata)
{
	u32 data = 0;
	uint8_t cnt = 0, i = 0;
	uint32_t coords[4] = {0};
	struct device_node *node, *pp = NULL;
	struct himax_virtual_key *vk;

	node = of_parse_phandle(dt, "virtualkey", 0);

	if (node == NULL) {
		I(" DT-No vk info in DT\n");
		return;
	}
	while ((pp = of_get_next_child(node, pp)))
		cnt++;

	if (!cnt)
		return;

	vk = kcalloc(cnt, sizeof(struct himax_virtual_key), GFP_KERNEL);
	if (vk == NULL) {
		E("%s, vk init fail!\n", __func__);
		return;
	}
	pp = NULL;

	while ((pp = of_get_next_child(node, pp))) {
		if (of_property_read_u32(pp, "idx", &data) == 0)
			vk[i].index = data;

		if (of_property_read_u32_array(pp, "range", coords, 4) == 0) {
			vk[i].x_range_min = coords[0], vk[i].x_range_max = coords[1];
			vk[i].y_range_min = coords[2], vk[i].y_range_max = coords[3];
		} else {
			I(" range faile\n");
		}

		i++;
	}

	pdata->virtual_key = vk;

	for (i = 0; i < cnt; i++)
		I(" vk[%d] idx:%d x_min:%d, y_max:%d\n", i, pdata->virtual_key[i].index,
			 pdata->virtual_key[i].x_range_min, pdata->virtual_key[i].y_range_max);

}

int himax_parse_dt(struct himax_ts_data *ts,
					struct himax_i2c_platform_data *pdata)
{
	struct device_node *dt = ts->dev->of_node;
	struct property *prop;
	u32 data = 0;
	int lcdtype = 0;
	int fw_name_cnt;
	int lcd_id1_gpio = 0, lcd_id2_gpio = 0, lcd_id3_gpio = 0, dt_lcdtype;
	int lcdtype_cnt;
	int fw_sel_idx = 0;
	int err;
	u32 coords[20];
	u32 px_zone[3] = { 0 };
	int i = 0;

	lcdtype = sec_input_get_lcd_id(ts->dev);
	if (lcdtype < 0) {
		input_err(true, ts->dev, "lcd is not attached\n");
		return -ENODEV;
	}

	pdata->gpio_irq = of_get_named_gpio(dt, "himax,irq-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_irq))
		I(" DT:gpio_irq value is not valid\n");


	pdata->gpio_reset = of_get_named_gpio(dt, "himax,rst-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_reset))
		I(" DT:gpio_rst value is not valid\n");

	pdata->gpio_cs = of_get_named_gpio(dt, "himax,cs-gpio", 0);
	if (!gpio_is_valid(pdata->gpio_cs)) {
		pdata->gpio_cs = -1;
		I(" DT:gpio_cs value is not valid\n");
	}

	pdata->gpio_3v3_en = of_get_named_gpio(dt, "himax,3v3-gpio", 0);

	if (!gpio_is_valid(pdata->gpio_3v3_en))
		I(" DT:gpio_3v3_en value is not valid\n");

	pdata->gpio_vendor_check = of_get_named_gpio(dt, "himax,vendor_check-gpio", 0);
	if (gpio_is_valid(pdata->gpio_vendor_check)) {
		err = of_property_read_u32(dt, "himax,vendor_check_enable_value", &data);
		if (err < 0) {
			E(" DT: failed to get vendor_check_enable_value, %d\n", err);
		} else {
			int vendor_check_value = gpio_get_value(pdata->gpio_vendor_check);

			if (vendor_check_value != data) {
				E(" DT: gpio_vendor_check is %d, himax ic is not connected\n",
					vendor_check_value);
				return -ENODEV;
			}
		}
	}

	I(" DT:gpio_irq=%d, gpio_rst=%d, gpio_3v3_en=%d, gpio_vendor_check=%d\n",
		pdata->gpio_irq, pdata->gpio_reset, pdata->gpio_3v3_en, pdata->gpio_vendor_check);

	fw_name_cnt = of_property_count_strings(dt, "himax,fw_name");

	if (fw_name_cnt == 0) {
		input_err(true, ts->dev, "%s: no fw_name in DT\n", __func__);
		return -EINVAL;
	} else if (fw_name_cnt == 1) {
		err = of_property_read_u32(dt, "himax,lcdtype", &dt_lcdtype);
		if (err < 0) {
			input_err(true, ts->dev, "%s: failed to read himax,lcdtype\n", __func__);
			return -EINVAL;
		}
	} else {
		lcd_id1_gpio = of_get_named_gpio(dt, "himax,lcdid1-gpio", 0);
		if (gpio_is_valid(lcd_id1_gpio)) {
			input_info(true, ts->dev, "%s: lcd id1_gpio %d(%d)\n", __func__, lcd_id1_gpio, gpio_get_value(lcd_id1_gpio));
			fw_sel_idx += gpio_get_value(lcd_id1_gpio);
		} else {
			input_err(true, ts->dev, "%s: Failed to get himax,lcdid1-gpio\n", __func__);
			return -EINVAL;
		}

		lcd_id2_gpio = of_get_named_gpio(dt, "himax,lcdid2-gpio", 0);
		if (gpio_is_valid(lcd_id2_gpio)) {
			input_info(true, ts->dev, "%s: lcd id2_gpio %d(%d)\n", __func__, lcd_id2_gpio, gpio_get_value(lcd_id2_gpio));
			fw_sel_idx += (gpio_get_value(lcd_id2_gpio) << 1);
		} else {
			input_err(true, ts->dev, "%s: Failed to get himax,lcdid2-gpio\n", __func__);
		}

		lcd_id3_gpio = of_get_named_gpio(dt, "himax,lcdid3-gpio", 0);
		if (gpio_is_valid(lcd_id3_gpio)) {
			input_info(true, ts->dev, "%s: lcd id3_gpio %d(%d)\n", __func__, lcd_id3_gpio, gpio_get_value(lcd_id3_gpio));
			fw_sel_idx += (gpio_get_value(lcd_id3_gpio) << 2);
		} else {
			input_err(true, ts->dev, "%s: Failed to get himax,lcdid3-gpio\n", __func__);
		}

		lcdtype_cnt = of_property_count_u32_elems(dt, "himax,lcdtype");
		input_info(true, ts->dev, "%s: fw_name_cnt(%d) & lcdtype_cnt(%d) & fw_sel_idx(%d)\n",
					__func__, fw_name_cnt, lcdtype_cnt, fw_sel_idx);

		if (lcdtype_cnt <= 0 || fw_name_cnt <= 0 || lcdtype_cnt <= fw_sel_idx || fw_name_cnt <= fw_sel_idx) {
			input_err(true, ts->dev, "%s: abnormal lcdtype & fw name count, fw_sel_idx(%d)\n", __func__, fw_sel_idx);
			return -EINVAL;
		}

		of_property_read_u32_index(dt, "himax,lcdtype", fw_sel_idx, &dt_lcdtype);
		input_info(true, ts->dev, "%s: lcd id(%d), ap lcdtype=0x%06X & dt lcdtype=0x%06X\n",
						__func__, fw_sel_idx, lcdtype, dt_lcdtype);
	}

	of_property_read_string_index(dt, "himax,fw_name", fw_sel_idx, &pdata->i_CTPM_firmware_name);
	if (pdata->i_CTPM_firmware_name == NULL || strlen(pdata->i_CTPM_firmware_name) == 0) {
		input_err(true, ts->dev, "%s: Failed to get fw name\n", __func__);
		return -EINVAL;
	} else {
		input_info(true, ts->dev, "%s: fw name(%s)\n", __func__, pdata->i_CTPM_firmware_name);
	}

	/* lcd regulator */
	if (of_property_read_string(dt, "himax,name_lcd_rst", &pdata->name_lcd_rst)) {
		input_err(true, ts->dev, "%s: Failed to get name_lcd_rst property\n", __func__);
		pdata->name_lcd_rst = NULL;
	}

	if (of_property_read_string(dt, "himax,name_lcd_vddi", &pdata->name_lcd_vddi)) {
		input_err(true, ts->dev, "%s: Failed to get name_lcd_vddi property\n", __func__);
		pdata->name_lcd_vddi = NULL;
	}

	if (of_property_read_string(dt, "himax,name_lcd_bl_en", &pdata->name_lcd_bl_en)) {
		input_err(true, ts->dev, "%s: Failed to get name_lcd_bl_en property\n", __func__);
		pdata->name_lcd_bl_en = NULL;
	}

	if (of_property_read_string(dt, "himax,name_lcd_vsp", &pdata->name_lcd_vsp)) {
		input_err(true, ts->dev, "%s: Failed to get name_lcd_vsp name property\n", __func__);
		pdata->name_lcd_vsp = NULL;
	}

	if (of_property_read_string(dt, "himax,name_lcd_vsn", &pdata->name_lcd_vsn)) {
		input_err(true, ts->dev, "%s: Failed to get name_lcd_vsn name property\n", __func__);
		pdata->name_lcd_vsn = NULL;
	}

	of_property_read_u32_array(dt, "himax,panel-coords", coords, 2);
	pdata->abs_x_min = 0;
	pdata->abs_x_max = (coords[0] - 1);
	pdata->abs_y_min = 0;
	pdata->abs_y_max = (coords[1] - 1);
	I(" DT-%s:panel-coords = %d, %d, %d, %d\n", __func__, pdata->abs_x_min,
		pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);

	if (of_property_read_u32_array(dt, "himax,display-coords", coords, 2)) {
		E(" %s:Fail to read display-coords\n", __func__);
		return -ENOENT;
	}
	pdata->screenWidth  = coords[0];
	pdata->screenHeight = coords[1];
	I(" DT-%s:display-coords = (%d, %d)\n", __func__, pdata->screenWidth, pdata->screenHeight);

#ifdef HX_FIX_TOUCH_INFO
	prop = of_find_property(dt, "himax,fix_touch_info", NULL);
	if (prop && prop->length) {
		if (of_property_read_u32_array(dt, "himax,fix_touch_info", coords, FIX_HX_MAX)) {
			E(" %s:Fail to read fix_touch_info\n", __func__);
			return -ENOENT;
		}

		ic_data->HX_RX_NUM = coords[FIX_HX_RX_NUM];
		ic_data->HX_TX_NUM = coords[FIX_HX_TX_NUM];
		ic_data->HX_BT_NUM = coords[FIX_HX_BT_NUM];
		ic_data->HX_X_RES = coords[FIX_HX_X_RES];
		ic_data->HX_Y_RES = coords[FIX_HX_Y_RES];
		ic_data->HX_MAX_PT = coords[FIX_HX_MAX_PT];
		ic_data->HX_XY_REVERSE = coords[FIX_HX_XY_REVERSE] ? true:false;
		ic_data->HX_INT_IS_EDGE = coords[FIX_HX_INT_IS_EDGE] ? true:false;

		I("%s:HX_RX_NUM =%d,HX_TX_NUM =%d,HX_MAX_PT=%d\n", __func__, ic_data->HX_RX_NUM, ic_data->HX_TX_NUM, ic_data->HX_MAX_PT);
		I("%s:HX_XY_REVERSE =%d,HX_Y_RES =%d,HX_X_RES=%d\n", __func__, ic_data->HX_XY_REVERSE, ic_data->HX_Y_RES, ic_data->HX_X_RES);
		I("%s:HX_INT_IS_EDGE =%d\n", __func__, ic_data->HX_INT_IS_EDGE);
	} else {
		I("%s DT: need to set fix_touch_info or not a zefoflash\n", __func__);
	}
#endif
	if (of_property_read_u32_array(dt, "himax,area-size", px_zone, 3)) {
		pdata->area_indicator = 48;
		pdata->area_navigation = 96;
		pdata->area_edge = 60;
	} else {
		pdata->area_indicator = px_zone[0];
		pdata->area_navigation = px_zone[1];
		pdata->area_edge = px_zone[2];
	}
	I(" DT-%s: zone's size - indicator:%d, navigation:%d, edge:%d\n",
		__func__, pdata->area_indicator, pdata->area_navigation, pdata->area_edge);

	if (of_property_read_u32(dt, "himax,report_type", &data) == 0) {
		pdata->protocol_type = data;
		I(" DT:protocol_type=%d\n", pdata->protocol_type);
	}

	prop = of_find_property(dt, "himax,one_frame_delay", NULL);
	if (prop && prop->length) {
		err = of_property_read_u32(dt, "himax,one_frame_delay", &data);
		if (err < 0) {
			E("%s:Unable to read himax,one_frame_delay property\n", __func__);
			pdata->one_frame_delay = 20;
		} else {
			pdata->one_frame_delay = data;
			I("%s DT:one_frame_delay=%d\n", __func__, pdata->one_frame_delay);
		}
	} else {
		pdata->one_frame_delay = 20;
		I("%s DT: set default one_frame_delay\n", __func__);
	}


	if (of_property_read_string(dt, "himax,project_name", &pdata->proj_name) < 0) {
		/* prevent from kernel panic due to use strcmp with null pointer */
		pdata->proj_name = "HIMAX";
		D("parsing from dt FAIL!!!!!, use default project name = %s\n", pdata->proj_name);
	}

	prop = of_find_property(dt, "himax,notch-setting", NULL);
	if (prop) {
		ic_data->notch_sz = prop->length / sizeof(u32);
		if (ic_data->notch_sz > 0) {
			ic_data->notch_arr = kzalloc(sizeof(int) * ic_data->notch_sz, GFP_KERNEL);
			if (of_property_read_u32_array(dt, "himax,notch-setting", ic_data->notch_arr, ic_data->notch_sz) == 0) {
				for (i = 0; i < ic_data->notch_sz; i++)
					I(" DT-%s:himax,notch-setting val%d=%d\n", __func__, i, ic_data->notch_arr[i]);
			} else {
				E(" DT-%s:notch-setting size is wrong\n", __func__);
			}
		}
	}

	pdata->support_aot = of_property_read_bool(dt, "support_aot");
	pdata->enable_sysinput_enabled = of_property_read_bool(dt, "himax,enable_sysinput_enabled");
	pdata->support_dex = of_property_read_bool(dt, "support_dex");
	pdata->notify_tsp_esd = of_property_read_bool(dt, "himax,notify_tsp_esd");
	pdata->support_ear_detect = of_property_read_bool(dt, "himax,support_ear_detect_mode");
	pdata->prox_lp_scan_enabled = of_property_read_bool(dt, "himax,prox_lp_scan_enabled");
	pdata->support_spay_gesture = of_property_read_bool(dt, "himax,support_spay_gesture_mode");

	input_info(true, ts->dev, "%s : supprot: %s%s%s%s%s%s%s\n",
				__func__, pdata->support_aot ? " AOT" : "",
				pdata->enable_sysinput_enabled ? " SE" : "",
				pdata->support_dex ? "DEX" : "",
				pdata->notify_tsp_esd ? "ESD" : "",
				pdata->support_ear_detect ? " PROX" : "",
				pdata->prox_lp_scan_enabled ? " LPSCAN" : "",
				pdata->support_spay_gesture ? " SPAY" : "");

	himax_vk_parser(dt, pdata);
	return 0;
}
EXPORT_SYMBOL(himax_parse_dt);

static ssize_t himax_spi_sync(struct himax_ts_data *ts, struct spi_message *message)
{
	int status;

	if (atomic_read(&ts->shutdown)) {
		E("%s: now IC status is shutdown\n", __func__);
		return -EIO;
	}

	if (atomic_read(&ts->suspend_mode) == HIMAX_STATE_POWER_OFF) {
		E("%s: now IC status is OFF\n", __func__);
		return -EIO;
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode()) {
		E("%s: now secure touch mode\n", __func__);
		return -EBUSY;
	}
#endif

	if (ts->cs_gpio > 0)
		gpio_direction_output(ts->cs_gpio, 0);

	status = spi_sync(ts->spi, message);

	if (ts->cs_gpio > 0)
		gpio_direction_output(ts->cs_gpio, 1);

	if (status == 0) {
		status = message->status;
		if (status == 0)
			status = message->actual_length;
	}
	return status;
}

static int himax_spi_read(uint8_t *command, uint8_t command_len, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	struct himax_ts_data *ts = private_ts;
	struct spi_message message;
	struct spi_transfer xfer[2];
	uint8_t *rbuff, *cbuff;
	int retry;
	int error;

	if (atomic_read(&ts->shutdown)) {
		E("%s: now IC status is shutdown\n", __func__);
		return -EIO;
	}

	if (atomic_read(&ts->suspend_mode) == HIMAX_STATE_POWER_OFF) {
		E("%s: now IC status is OFF\n", __func__);
		return -EIO;
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode()) {
		E("%s: now secure touch mode\n", __func__);
		return -EBUSY;
	}
#endif

	rbuff = kzalloc(sizeof(uint8_t) * length, GFP_KERNEL);
	if (!rbuff)
		return -ENOMEM;

	cbuff = kzalloc(sizeof(uint8_t) * command_len, GFP_KERNEL);
	if (!cbuff) {
		kfree(rbuff);
		return -ENOMEM;
	}

	spi_message_init(&message);
	memset(xfer, 0, sizeof(xfer));

	memcpy(cbuff, command, command_len);
	xfer[0].tx_buf = cbuff;
	xfer[0].len = command_len;
	xfer[0].cs_change = 0;
	spi_message_add_tail(&xfer[0], &message);

	xfer[1].rx_buf = rbuff;
	xfer[1].len = length;
	xfer[1].cs_change = 0;
	spi_message_add_tail(&xfer[1], &message);

	for (retry = 0; retry < toRetry; retry++) {
		if (ts->cs_gpio > 0)
			gpio_direction_output(ts->cs_gpio, 0);

		error = spi_sync(private_ts->spi, &message);

		if (ts->cs_gpio > 0)
			gpio_direction_output(ts->cs_gpio, 1);

		if (unlikely(error))
			E("SPI read error: %d\n", error);
		else
			break;
	}

	if (retry == toRetry) {
		E("%s: SPI read error retry over %d\n",
			__func__, toRetry);
		kfree(cbuff);
		kfree(rbuff);
		return -EIO;
	}
	memcpy(data, rbuff, length);

	kfree(cbuff);
	kfree(rbuff);

	return 0;
}

static int himax_spi_write(uint8_t *buf, uint32_t length)
{
	struct himax_ts_data *ts = private_ts;
	struct spi_transfer	t = {
			.tx_buf		= buf,
			.len		= length,
			.cs_change	= 0,
	};
	struct spi_message	m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);

	if (atomic_read(&ts->shutdown)) {
		E("%s: now IC status is shutdown\n", __func__);
		return -EIO;
	}

	if (atomic_read(&ts->suspend_mode) == HIMAX_STATE_POWER_OFF) {
		E("%s: now IC status is OFF\n", __func__);
		return -EIO;
	}

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	if (STUI_MODE_TOUCH_SEC & stui_get_mode()) {
		E("%s: now secure touch mode\n", __func__);
		return -EBUSY;
	}
#endif

	return himax_spi_sync(private_ts, &m);

}

int himax_bus_read(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	int result = 0;
	uint8_t spi_format_buf[3];

	mutex_lock(&(private_ts->spi_lock));
	spi_format_buf[0] = 0xF3;
	spi_format_buf[1] = command;
	spi_format_buf[2] = 0x00;

	result = himax_spi_read(&spi_format_buf[0], 3, data, length, toRetry);
	mutex_unlock(&(private_ts->spi_lock));

	return result;
}
EXPORT_SYMBOL(himax_bus_read);

int himax_bus_write(uint8_t command, uint8_t *data, uint32_t length, uint8_t toRetry)
{
	uint8_t *spi_format_buf = gBuffer;
	int i = 0;
	int result = 0;

	mutex_lock(&(private_ts->spi_lock));
	spi_format_buf[0] = 0xF2;
	spi_format_buf[1] = command;

	for (i = 0; i < length; i++)
		spi_format_buf[i + 2] = data[i];

	result = himax_spi_write(spi_format_buf, length + 2);
	mutex_unlock(&(private_ts->spi_lock));

	return result;
}
EXPORT_SYMBOL(himax_bus_write);

int himax_bus_write_command(uint8_t command, uint8_t toRetry)
{
	return himax_bus_write(command, NULL, 0, toRetry);
}

int himax_bus_master_write(uint8_t *data, uint32_t length, uint8_t toRetry)
{
	uint8_t *buf;

	struct spi_transfer	t;
	struct spi_message	m;
	int result = 0;

	buf = kzalloc(length * sizeof(uint8_t), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	mutex_lock(&(private_ts->spi_lock));
	memcpy(buf, data, length);
	t.tx_buf = buf;
	t.len = length;
	t.cs_change = 0;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	result = himax_spi_sync(private_ts, &m);
	mutex_unlock(&(private_ts->spi_lock));

	kfree(buf);
	return result;
}
EXPORT_SYMBOL(himax_bus_master_write);

void himax_int_enable(int enable)
{
	struct himax_ts_data *ts = private_ts;
	int irqnum = ts->hx_irq;

	if (ts->use_irq == 0)
		return;

	mutex_lock(&ts->irq_lock);
	if (enable == INT_ENABLE && atomic_read(&ts->irq_state) == 0) {
		atomic_set(&ts->irq_state, 1);
		enable_irq(irqnum);
		private_ts->irq_enabled = 1;
	} else if (enable == INT_DISABLE_NOSYNC && atomic_read(&ts->irq_state) == 1) {
		atomic_set(&ts->irq_state, 0);
		disable_irq_nosync(irqnum);
		private_ts->irq_enabled = 0;
	} else if (enable == INT_DISABLE_SYNC && atomic_read(&ts->irq_state) == 1) {
		atomic_set(&ts->irq_state, 0);
		disable_irq(irqnum);
		private_ts->irq_enabled = 0;
	} else {
		I("%s: faulty enable vluae, irqnum = %d enable:%d, irq_state:%d irq_enable:%d\n",
		__func__, irqnum, enable, atomic_read(&ts->irq_state), private_ts->irq_enabled);
		mutex_unlock(&ts->irq_lock);
		return;
	}

	I("%s: irqnum = %d enable:%d, irq_state:%d irq_enable:%d\n",
		__func__, irqnum, enable, atomic_read(&ts->irq_state), private_ts->irq_enabled);
	mutex_unlock(&ts->irq_lock);
}
EXPORT_SYMBOL(himax_int_enable);

#ifdef HX_RST_PIN_FUNC
void himax_rst_gpio_set(int pinnum, uint8_t value)
{
	gpio_direction_output(pinnum, value);
}
EXPORT_SYMBOL(himax_rst_gpio_set);
#endif

uint8_t himax_int_gpio_read(int pinnum)
{
	return gpio_get_value(pinnum);
}

#if defined(CONFIG_HMX_DB)
static int himax_regulator_configure(struct himax_i2c_platform_data *pdata)
{
	int retval;
	/* struct i2c_client *client = private_ts->client; */

	pdata->vcc_dig = regulator_get(private_ts->dev, "vdd");

	if (IS_ERR(pdata->vcc_dig)) {
		E("%s: Failed to get regulator vdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_dig);
		return retval;
	}

	pdata->vcc_ana = regulator_get(private_ts->dev, "avdd");

	if (IS_ERR(pdata->vcc_ana)) {
		E("%s: Failed to get regulator avdd\n",
		  __func__);
		retval = PTR_ERR(pdata->vcc_ana);
		regulator_put(pdata->vcc_dig);
		return retval;
	}

	return 0;
};

static void himax_regulator_deinit(struct himax_i2c_platform_data *pdata)
{
	I("%s: entered.\n", __func__);

	if (!IS_ERR(pdata->vcc_ana))
		regulator_put(pdata->vcc_ana);

	if (!IS_ERR(pdata->vcc_dig))
		regulator_put(pdata->vcc_dig);

	I("%s: regulator put, completed.\n", __func__);
};

static int himax_power_on(struct himax_i2c_platform_data *pdata, bool on)
{
	int retval;

	if (on) {
		retval = regulator_enable(pdata->vcc_dig);

		if (retval) {
			E("%s: Failed to enable regulator vdd\n",
			  __func__);
			return retval;
		}

		/*msleep(100);*/
		usleep_range(1000, 1001);
		retval = regulator_enable(pdata->vcc_ana);

		if (retval) {
			E("%s: Failed to enable regulator avdd\n",
			  __func__);
			regulator_disable(pdata->vcc_dig);
			return retval;
		}
	} else {
		regulator_disable(pdata->vcc_dig);
		regulator_disable(pdata->vcc_ana);
	}

	return 0;
}

int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error;
	/* struct i2c_client *client = private_ts->client; */

	error = himax_regulator_configure(pdata);

	if (error) {
		E("Failed to intialize hardware\n");
		goto err_regulator_not_on;
	}

#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset)) {
		/* configure touchscreen reset out gpio */
		error = gpio_request(pdata->gpio_reset, "hmx_reset_gpio");

		if (error) {
			E("unable to request gpio [%d]\n", pdata->gpio_reset);
			goto err_regulator_on;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_req;
		}
	}

#endif
	error = himax_power_on(pdata, true);

	if (error) {
		E("Failed to power on hardware\n");
		goto err_power_on;
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "hmx_gpio_irq");

		if (error) {
			E("unable to request gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_req_irq_gpio;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_irq);
			goto err_set_gpio_irq;
		}

		private_ts->hx_irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		goto err_req_irq_gpio;
	}

	/*msleep(20);*/
	usleep_range(2000, 2001);
#ifdef HX_RST_PIN_FUNC

	if (gpio_is_valid(pdata->gpio_reset)) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_set_gpio_irq;
		}
		usleep_range(5000, 5000);
	}

#endif
	return 0;
err_set_gpio_irq:

	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

err_req_irq_gpio:
	himax_power_on(pdata, false);
err_power_on:
#ifdef HX_RST_PIN_FUNC
err_gpio_reset_req:
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);

err_regulator_on:
#endif
	himax_regulator_deinit(pdata);
err_regulator_not_on:
	return error;
}

#else
int himax_gpio_power_config(struct himax_i2c_platform_data *pdata)
{
	int error = 0;
	/* struct i2c_client *client = private_ts->client; */
#ifdef HX_RST_PIN_FUNC

	if (pdata->gpio_reset >= 0) {
		error = gpio_request(pdata->gpio_reset, "himax-reset");

		if (error < 0) {
			E("%s: request reset pin failed\n", __func__);
			goto err_gpio_reset_req;
		}

		error = gpio_direction_output(pdata->gpio_reset, 0);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_dir;
		}
	}

#endif

	if (gpio_is_valid(pdata->gpio_cs)) {
		error = gpio_request(pdata->gpio_cs, "himax-cs");
		if (error < 0) {
			E("%s: request cs pin failed\n", __func__);
			goto err_gpio_cs_req;
		}

		error = gpio_direction_output(pdata->gpio_cs, 1);
		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_cs);
			goto err_gpio_cs_dir;
		}
		E("%s: pdata->gpio_cs %d\n", __func__, pdata->gpio_cs);
	}

#ifdef HX_PON_PIN_SUPPORT
	if (gpio_is_valid(pdata->gpio_pon)) {
		error = gpio_request(pdata->gpio_pon, "hmx_pon_gpio");

		if (error) {
			E("unable to request scl gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_req;
		}

		error = gpio_direction_output(pdata->gpio_pon, 0);

		I("gpio_pon LOW [%d]\n", pdata->gpio_pon);

		if (error) {
			E("unable to set direction for pon gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_dir;
		}
	}
#endif

	if (pdata->gpio_3v3_en >= 0) {
		error = gpio_request(pdata->gpio_3v3_en, "himax-3v3_en");

		if (error < 0) {
			E("%s: request 3v3_en pin failed\n", __func__);
			goto err_gpio_3v3_req;
		}

		gpio_direction_output(pdata->gpio_3v3_en, 1);
		I("3v3_en set 1 get pin = %d\n", gpio_get_value(pdata->gpio_3v3_en));
	}

	if (gpio_is_valid(pdata->gpio_irq)) {
		/* configure touchscreen irq gpio */
		error = gpio_request(pdata->gpio_irq, "himax_gpio_irq");

		if (error) {
			E("unable to request gpio_irq [%d]\n", pdata->gpio_irq);
			goto err_gpio_irq_req;
		}

		error = gpio_direction_input(pdata->gpio_irq);

		if (error) {
			E("unable to set direction for gpio [%d]\n", pdata->gpio_irq);
			goto err_gpio_irq_set_input;
		}

		private_ts->hx_irq = gpio_to_irq(pdata->gpio_irq);
	} else {
		E("irq gpio not provided\n");
		goto err_gpio_irq_req;
	}
#ifdef HX_PON_PIN_SUPPORT
	msleep(20);
#else
	usleep_range(2000, 2001);
#endif

#ifdef HX_RST_PIN_FUNC

	if (pdata->gpio_reset >= 0) {
		error = gpio_direction_output(pdata->gpio_reset, 1);

		if (error) {
			E("unable to set direction for gpio [%d]\n",
			  pdata->gpio_reset);
			goto err_gpio_reset_set_high;
		}
		usleep_range(5000, 5000);
	}
#endif

#ifdef HX_PON_PIN_SUPPORT
	msleep(800);

	if (gpio_is_valid(pdata->gpio_pon)) {

		error = gpio_direction_output(pdata->gpio_pon, 1);

		I("gpio_pon HIGH [%d]\n", pdata->gpio_pon);

		if (error) {
			E("gpio_pon unable to set direction for gpio [%d]\n", pdata->gpio_pon);
			goto err_gpio_pon_set_high;
		}
	}
#endif
	return error;

#ifdef HX_PON_PIN_SUPPORT
err_gpio_pon_set_high:
#endif
#ifdef HX_RST_PIN_FUNC
err_gpio_reset_set_high:
#endif
err_gpio_irq_set_input:
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);
err_gpio_irq_req:
	if (pdata->gpio_3v3_en >= 0)
		gpio_free(pdata->gpio_3v3_en);
err_gpio_3v3_req:
#ifdef HX_PON_PIN_SUPPORT
err_gpio_pon_dir:
	if (gpio_is_valid(pdata->gpio_pon))
		gpio_free(pdata->gpio_pon);
err_gpio_pon_req:
#endif
err_gpio_cs_dir:
	if (gpio_is_valid(pdata->gpio_cs))
		gpio_free(pdata->gpio_cs);
err_gpio_cs_req:
#ifdef HX_RST_PIN_FUNC
err_gpio_reset_dir:
	if (pdata->gpio_reset >= 0)
		gpio_free(pdata->gpio_reset);
err_gpio_reset_req:
#endif
	return error;
}

#endif

void himax_gpio_power_deconfig(struct himax_i2c_platform_data *pdata)
{
	if (gpio_is_valid(pdata->gpio_irq))
		gpio_free(pdata->gpio_irq);

#ifdef HX_RST_PIN_FUNC
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_free(pdata->gpio_reset);
#endif

	if (gpio_is_valid(pdata->gpio_cs))
		gpio_free(pdata->gpio_cs);

#if defined(CONFIG_HMX_DB)
	himax_power_on(pdata, false);
	himax_regulator_deinit(pdata);
#else
	if (pdata->gpio_3v3_en >= 0)
		gpio_free(pdata->gpio_3v3_en);

#ifdef HX_PON_PIN_SUPPORT
	if (gpio_is_valid(pdata->gpio_pon))
		gpio_free(pdata->gpio_pon);
#endif

#endif
}

static void himax_ts_isr_func(struct himax_ts_data *ts)
{
	himax_ts_work(ts);
}

irqreturn_t himax_ts_thread(int irq, void *ptr)
{
	himax_ts_isr_func((struct himax_ts_data *)ptr);

	return IRQ_HANDLED;
}

static void himax_ts_work_func(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, work);

	himax_ts_work(ts);
}

int himax_int_register_trigger(void)
{
	int ret = 0;
	struct himax_ts_data *ts = private_ts;

	if (ic_data->HX_INT_IS_EDGE) {
		I("%s edge triiger falling\n", __func__);
		ret = request_threaded_irq(ts->hx_irq, NULL, himax_ts_thread, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	} else {
		I("%s level trigger low\n", __func__);
		ret = request_threaded_irq(ts->hx_irq, NULL, himax_ts_thread, IRQF_TRIGGER_LOW | IRQF_ONESHOT, HIMAX_common_NAME, ts);
	}

	return ret;
}

int himax_int_en_set(void)
{
	int ret = NO_ERR;

	ret = himax_int_register_trigger();
	return ret;
}

int himax_ts_register_interrupt(void)
{
	struct himax_ts_data *ts = private_ts;
	/* struct i2c_client *client = private_ts->client; */
	int ret = 0;

	ts->irq_enabled = 0;

	/* Work functon */
	if (private_ts->hx_irq) {/*INT mode*/
		ts->use_irq = 1;
		ret = himax_int_register_trigger();

		if (ret == 0) {
			ts->irq_enabled = 1;
			atomic_set(&ts->irq_state, 1);
			I("%s: irq enabled at gpio: %d\n", __func__, private_ts->hx_irq);
#ifdef HX_SMART_WAKEUP
			irq_set_irq_wake(private_ts->hx_irq, 1);
#endif
		} else {
			ts->use_irq = 0;
			E("%s: request_irq failed\n", __func__);
		}
	} else {
		I("%s: private_ts->hx_irq is empty, use polling mode.\n", __func__);
	}

	if (!ts->use_irq) {/*if use polling mode need to disable HX_ESD_RECOVERY function*/
		ts->himax_wq = create_singlethread_workqueue("himax_touch");
		INIT_WORK(&ts->work, himax_ts_work_func);
		hrtimer_init(&ts->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		ts->timer.function = himax_ts_timer_func;
		hrtimer_start(&ts->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
		I("%s: polling mode enabled\n", __func__);
	}

	return ret;
}

int himax_ts_unregister_interrupt(void)
{
	struct himax_ts_data *ts = private_ts;
	int ret = 0;

	I("%s: entered.\n", __func__);

	/* Work functon */
	if (private_ts->hx_irq && ts->use_irq) {/*INT mode*/
#ifdef HX_SMART_WAKEUP
		irq_set_irq_wake(ts->hx_irq, 0);
#endif
		free_irq(ts->hx_irq, ts);
		I("%s: irq disabled at qpio: %d\n", __func__, private_ts->hx_irq);
	}

	if (!ts->use_irq) {/*if use polling mode need to disable HX_ESD_RECOVERY function*/
		hrtimer_cancel(&ts->timer);
		cancel_work_sync(&ts->work);
		if (ts->himax_wq != NULL)
			destroy_workqueue(ts->himax_wq);
		I("%s: polling mode destroyed", __func__);
	}

	return ret;
}

static int himax_common_late_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	if (!ts->initialized)
		return -ECANCELED;
#endif
	himax_chip_common_late_suspend(ts);
	return 0;
}

static int himax_common_early_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	if (!ts->initialized)
		return -ECANCELED;
#endif
	himax_chip_common_early_suspend(ts);
	return 0;
}

#ifndef HX_CONTAINER_SPEED_UP
static int himax_common_late_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	/*
	 *	wait until device resume for TDDI
	 *	TDDI: Touch and display Driver IC
	 */
	if (!ts->initialized) {
		if (himax_chip_common_init())
			return -ECANCELED;
	}
#endif
	himax_chip_common_late_resume(ts);
	return 0;
}
static int himax_common_early_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	I("%s: enter\n", __func__);
#if IS_ENABLED(CONFIG_DISPLAY_SAMSUNG)
	/*
	 *	wait until device resume for TDDI
	 *	TDDI: Touch and display Driver IC
	 */
	if (!ts->initialized) {
		if (himax_chip_common_init())
			return -ECANCELED;
	}
#endif
	himax_chip_common_early_resume(ts);
	return 0;
}
#endif

static int himax_reboot_notifier(struct notifier_block *this,
		unsigned long code, void *unused)
{
	struct himax_ts_data *ts = container_of(this, struct himax_ts_data, reboot_notifier);

	I("%s %s: enter\n", HIMAX_LOG_TAG, __func__);

	himax_chip_common_early_suspend(ts);

	I("%s %s: exit\n", HIMAX_LOG_TAG, __func__);

	return NOTIFY_DONE;
}

#if defined(HX_USB_DETECT_GLOBAL)
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int otg_flag;
#endif
#endif

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int himax_ccic_notification(struct notifier_block *nb,
	   unsigned long action, void *data)
{

	PD_NOTI_USB_STATUS_TYPEDEF usb_status = *(PD_NOTI_USB_STATUS_TYPEDEF *)data;

	if (usb_status.dest != PDIC_NOTIFY_DEV_USB) {
		return 0;
	}

	switch (usb_status.drp) {
	case USB_STATUS_NOTIFY_ATTACH_DFP:
		otg_flag = 1;
		input_info(true, private_ts->dev, "%s otg_flag %d\n", __func__, otg_flag);
		break;
	case USB_STATUS_NOTIFY_DETACH:
		otg_flag = 0;
		input_info(true, private_ts->dev, "%s otg_flag %d\n", __func__, otg_flag);
		break;
	default:
		break;
	}
	return 0;
}
#endif
static int himax_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	vbus_status_t vbus_type = *(vbus_status_t *) data;
	int ret = 0;

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	input_info(true, private_ts->dev, "%s otg_flag=%d\n", __func__, otg_flag);
#endif
	input_info(true, private_ts->dev, "%s cmd=%lu, vbus_type=%d\n", __func__, cmd, vbus_type);


	switch (vbus_type) {
	case STATUS_VBUS_HIGH:/* vbus_type == 2 */
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		if (!otg_flag)
			private_ts->USB_detect_flag = USB_PLUG_ATTACHED;
		else
			ret = -1;
#else
		private_ts->USB_detect_flag = USB_PLUG_ATTACHED;
#endif
		break;
	case STATUS_VBUS_LOW:/* vbus_type == 1 */
		private_ts->USB_detect_flag = USB_PLUG_DETACHED;
		break;
	default:
		ret = -1;
		break;
	}
	if (atomic_read(&private_ts->suspend_mode) != HIMAX_STATE_POWER_OFF) {
		if (private_ts->USB_detect_flag)
			himax_cable_detect_func(true);
		else
			himax_cable_detect_func(false);
	} else {
		input_info(true, private_ts->dev, "%s It will be set after power on(%d)\n",
				 __func__, private_ts->suspended);
	}
	if (ret < 0)
		return ret;

	return 0;
}
#endif
#endif

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
static void himax_input_notify_work(struct work_struct *work)
{
	struct himax_ts_data *ts = container_of(work, struct himax_ts_data, himax_input_notify_work.work);

	switch (ts->input_notify) {
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		himax_set_ap_change_mode(SPEN_MODE, 1);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		himax_set_ap_change_mode(SPEN_MODE, 0);
		break;
	default:
		break;
	}
}

static int himax_input_notify_call(struct notifier_block *n, unsigned long data, void *v)
{
	struct himax_ts_data *ts = container_of(n, struct himax_ts_data, himax_input_nb);

	if (!ts)
		return -ENODEV;

	switch (data) {
	case NOTIFIER_WACOM_PEN_HOVER_IN:
		cancel_delayed_work(&ts->himax_input_notify_work);
		ts->input_notify = NOTIFIER_WACOM_PEN_HOVER_IN;
		schedule_work(&ts->himax_input_notify_work.work);
		break;
	case NOTIFIER_WACOM_PEN_HOVER_OUT:
		cancel_delayed_work(&ts->himax_input_notify_work);
		ts->input_notify = NOTIFIER_WACOM_PEN_HOVER_OUT;
		schedule_work(&ts->himax_input_notify_work.work);
		break;
	default:
		break;
	}

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
struct himax_ts_data *stui_ts;
extern int stui_spi_lock(struct spi_master *spi);
extern int stui_spi_unlock(struct spi_master *spi);

static int himax_stui_tsp_enter(void)
{
	int ret = 0;
	struct spi_device *spi = stui_ts->spi;

	input_info(true, &spi->dev, ">> %s\n", __func__);

	himax_int_enable(INT_DISABLE_NOSYNC);

	ret = stui_spi_lock(spi->master);
	if (ret < 0) {
		pr_err("[STUI] stui_spi_lock failed : %d\n", ret);
		himax_int_enable(INT_ENABLE);
		return -1;
	}

	return 0;
}

static int himax_stui_tsp_exit(void)
{
	int ret = 0;
	struct spi_device *spi = stui_ts->spi;

	input_info(true, &spi->dev, ">> %s\n", __func__);

	ret = stui_spi_unlock(spi->master);
	if (ret < 0) {
		pr_err("[STUI] stui_spi_unlock failed : %d\n", ret);
	}

	himax_int_enable(INT_ENABLE);

	return ret;
}

static int himax_stui_tsp_type(void)
{
	return STUI_TSP_TYPE_HIMAX;
}
#endif

int himax_chip_common_probe(struct spi_device *spi)
{
	struct himax_ts_data *ts;
	int ret = 0;

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
	static int deferred_flag;

	if (!deferred_flag) {
		deferred_flag = 1;
		input_info(true, &spi->dev, "deferred_flag boot %s\n", __func__);
		return -EPROBE_DEFER;
	}
#endif

	input_info(true, &spi->dev, "%s:Enter\n", __func__);

	if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
		dev_err(&spi->dev,
				"%s: Full duplex not supported by host\n", __func__);
		return -EIO;
	}

	gBuffer = kzalloc(sizeof(uint8_t) * (HX_MAX_WRITE_SZ + 6), GFP_KERNEL);
	if (gBuffer == NULL) {
		KE("%s: allocate gBuffer failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_gbuffer_failed;
	}

	ts = kzalloc(sizeof(struct himax_ts_data), GFP_KERNEL);
	if (ts == NULL) {
		KE("%s: allocate himax_ts_data failed\n", __func__);
		ret = -ENOMEM;
		goto err_alloc_data_failed;
	}

	private_ts = ts;
	spi->bits_per_word = 8;
	spi->mode = SPI_MODE_3;
	spi->chip_select = 0;

	ts->spi = spi;
	mutex_init(&(ts->spi_lock));
	ts->dev = &spi->dev;
	dev_set_drvdata(&spi->dev, ts);
	spi_set_drvdata(spi, ts);

	ts->initialized = false;
	ret = himax_chip_common_init();
	if (ret < 0)
		goto err_alloc_data_failed;

	ts->pdata->early_suspend = himax_common_early_suspend;
	ts->pdata->late_suspend = himax_common_late_suspend;
	ts->pdata->early_resume = himax_common_early_resume;
	ts->pdata->late_resume = himax_common_late_resume;

	ts->reboot_notifier.notifier_call = himax_reboot_notifier;
	register_reboot_notifier(&ts->reboot_notifier);

#if defined(HX_USB_DETECT_GLOBAL)
	ts->USB_detect_flag = USB_PLUG_DETACHED;
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_register(&ts->ccic_nb, himax_ccic_notification,
			MANAGER_NOTIFY_PDIC_INITIAL);
#endif
	vbus_notifier_register(&ts->vbus_nb, himax_vbus_notification,
			VBUS_NOTIFY_DEV_CHARGER);
#endif
#endif
#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_register_notify(&ts->himax_input_nb, himax_input_notify_call, 1);
	INIT_DELAYED_WORK(&ts->himax_input_notify_work, himax_input_notify_work);
#endif

#if SEC_LPWG_DUMP
	himax_lpwg_dump_buf_init();
#endif

#if IS_ENABLED(CONFIG_SAMSUNG_TUI)
	stui_ts = ts;
	stui_tsp_init(himax_stui_tsp_enter, himax_stui_tsp_exit, himax_stui_tsp_type);
	input_info(true, stui_ts->dev, "secure touch support\n");
#endif

	return 0;

err_alloc_data_failed:
	kfree(gBuffer);
err_alloc_gbuffer_failed:
	return ret;
}

int himax_chip_common_remove(struct spi_device *spi)
{
	struct himax_ts_data *ts = spi_get_drvdata(spi);

#if IS_ENABLED(CONFIG_INPUT_SEC_NOTIFIER)
	sec_input_unregister_notify(&ts->himax_input_nb);
	cancel_delayed_work_sync(&ts->himax_input_notify_work);
#endif
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_unregister(&ts->ccic_nb);
#endif
	vbus_notifier_unregister(&ts->vbus_nb);
#endif

	himax_pinctrl_configure(ts, false);
	msleep(ts->pdata->one_frame_delay);
#ifdef HX_RST_PIN_FUNC
		if (gpio_get_value(ts->pdata->gpio_reset)) {
			gpio_set_value(ts->pdata->gpio_reset, 0);
			I("%s make TP_RESET low\n", __func__);
		}
#endif
	if (g_hx_chip_inited)
		himax_chip_common_deinit();

	ts->spi = NULL;
	/* spin_unlock_irq(&ts->spi_lock); */
	spi_set_drvdata(spi, NULL);
	kfree(gBuffer);

	KI("%s: completed.\n", __func__);

	return 0;
}

static int himax_pm_suspend(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	input_info(true, ts->dev, "%s: called!\n", __func__);
	reinit_completion(&ts->resume_done);

	return 0;
}

static int himax_pm_resume(struct device *dev)
{
	struct himax_ts_data *ts = dev_get_drvdata(dev);

	input_info(true, ts->dev, "%s: called!\n", __func__);
	complete_all(&ts->resume_done);

	return 0;
}

#if IS_ENABLED(CONFIG_PM)
static const struct dev_pm_ops himax_common_pm_ops = {
	.suspend = himax_pm_suspend,
	.resume  = himax_pm_resume,
};
#endif

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id himax_match_table[] = {
	{.compatible = "himax,hxcommon" },
	{},
};
#else
#define himax_match_table NULL
#endif

static struct spi_driver himax_common_driver = {
	.driver = {
		.name =		HIMAX_common_NAME,
		.owner =	THIS_MODULE,
		.of_match_table = himax_match_table,
#if IS_ENABLED(CONFIG_PM)
		.pm = &himax_common_pm_ops,
#endif
	},
	.probe =	himax_chip_common_probe,
	.remove =	himax_chip_common_remove,
};

static int __init himax_common_init(void)
{
	KI("Himax common touch panel driver init\n");
	D("Himax check double loading\n");
	if (g_mmi_refcnt++ > 0) {
		KI("Himax driver has been loaded! ignoring....\n");
		return 0;
	}
	spi_register_driver(&himax_common_driver);
	input_log_fix();

	return 0;
}

static void __exit himax_common_exit(void)
{
#if IS_ENABLED(CONFIG_QGKI)
	if (spi) {
		spi_unregister_device(spi);
		spi = NULL;
	}
	spi_unregister_driver(&himax_common_driver);
#endif
}

module_init(himax_common_init);
module_exit(himax_common_exit);

MODULE_DESCRIPTION("Himax_common driver");
MODULE_LICENSE("GPL");
