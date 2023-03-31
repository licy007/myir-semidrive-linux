
/*
 * sdrv-abuf.h
 * Copyright (C) 2019 semidrive
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef __SDRV_ABUF_H__
#define __SDRV_ABUF_H__

enum audio_buff_type
{
	SDRV_ABUF_HIFI_UL,
	SDRV_ABUF_HIFI_DL,
	SDRV_ABUF_DAI,
	SDRV_ABUF_NUMB,
};

enum x9_i2s_id
{
	X9_I2S_1,
	X9_I2S_2,
	X9_I2S_NUMB,
};

enum g9_i2s_id
{
	G9_I2S_1,
	G9_I2S_2,
	G9_I2S_NUMB,
};

/* x9 audio buffer structure */
struct sdrv_audio_buf
{
	int id;
	const char *name;
	/*Buf physical address*/
	void __iomem *phy_addr;
	int buf_size;
	bool is_sram;
	bool prepared;
	struct snd_pcm_substream *substream;
	int abuf_cur;
};

#endif
