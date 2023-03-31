#ifndef _SERDES_9XX_H
#define _SERDES_9XX_H

enum serdes_9xx_port {
	PRIMARY_PORT,
	SECOND_PORT,
	DOUBLE_PORT,
};

extern int du90ub941_or_947_enable_port(struct i2c_client *client,
	uint8_t addr, enum serdes_9xx_port port_flag);

extern int du90ub941_or_947_enable_i2c_passthrough(struct i2c_client *client,
	uint8_t addr);

extern int du90ub941_or_947_enable_int(struct i2c_client *client,
	uint8_t addr);

extern int du90ub948_gpio_output(struct i2c_client *client, 
	uint8_t addr, int gpio, int val);

extern int du90ub948_gpio_input(struct i2c_client *client,
	uint8_t addr, int gpio);

extern int du90ub941_or_947_gpio_input(struct i2c_client *client,
	uint8_t addr, int gpio);

#endif

