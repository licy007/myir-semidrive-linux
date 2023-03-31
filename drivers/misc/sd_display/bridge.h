#ifndef __BRDIGE_H__
#define __BRDIGE_H__

/*
 * MIPI information
 */
typedef enum {
	MIPI_RGB888 = 0,
	MIPI_YUV422,
} mipi_data_fmt;

typedef enum {
	tx_Non_Burst_with_sync_event = 0,
	tx_Non_Burst_with_sync_pluse,
	tx_Burst,
} mipi_tx_mode;

typedef enum {
	phy_DSI = 0,
	phy_CSI,
} mipi_phy_type;

struct mipi_bus {
	mipi_data_fmt data_fmt;
	mipi_tx_mode tx_mode;            /* video transmission mode */
	mipi_phy_type phy_type;
	int lanes;
	int clk;
	unsigned short clk_settle_time;  /* unit: ns, range: 95 ~ 300ns */
	unsigned short data_settle_time; /* unit: ns, range: 85 ~ 145ns + 10*UI */
};

/*
 * DVP information
 */
struct dvp_bus {

};

/*
 * DVP information
 */
struct ser_bus {

};

/*
 * Serializer information
 */
typedef enum {
	mode_AC,
	mode_DC,
} hdmi_couple_mode;

typedef enum {
	port_DVI = 0,
	port_HDMI,
} hdmi_port_type;

typedef enum {
	port_IIS = 0,
	port_SPD,
} audio_port_type;

struct hdmi_audio_bus {
	audio_port_type port;
};

struct hdmi_bus {
	hdmi_couple_mode couple;
	hdmi_port_type port;
	char en_hdcp;           /* 0:去使能 1:使能 */
	struct hdmi_audio_bus audio;
};

/*
 * eDP information
 */
struct edp_bus {

};

/*
 * Deserializer information
 */
struct des_bus {

};

struct bridge_operations {
	int (*power_on)(void);
	void (*power_off)(void);

};

struct bridge_attr {
	char *device_name;
	unsigned int cbus_addr;
	union {                            /* 输入接口 */
		struct dvp_bus dvp;
		struct mipi_bus mipi;
		struct ser_bus ser;
	};
	union {                            /* 输出接口 */
		struct edp_bus edp;
		struct hdmi_bus hdmi;
		struct des_bus des;
	};

	// struct video_timing *timing;
	const struct bridge_operations ops;
};


int register_bridge(struct bridge_attr *pdata);

void unregister_bridge(struct bridge_attr *pdata);

#endif  /* __BRDIGE_H__ */