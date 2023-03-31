/*
*   FILE NAME  : spi-is62w.h
*
*   IRAM Ltd.
*	spi-is62w.h
*   By Li Nan Tech
*   DEMO Version: 1.0 Data: 2021-8-20
*
*   DESCRIPTION: Implements an interface for the is62w of spi interface
*
*/

/*****************************
*is62w slave iram cmd defines
******************************/
#define IS62W_READ    0x03
#define IS62W_WRITE   0x02
#define IS62W_ESDI    0x3B
#define IS62W_ESQI    0x38
#define IS62W_RSTDQI  0xFF
#define IS62W_RDMR    0x05
#define IS62W_WRMR    0x01

#define IS62W_CA_LEN  4
#define IS62W_RD_DLY  1
#define IS62W_MSG_LEN 8
#define IS62W_WR_OFF  IS62W_CA_LEN
#define IS62W_RD_OFF  (IS62W_CA_LEN + IS62W_RD_DLY)
#define IS62W_WR_LEN  (IS62W_MSG_LEN - IS62W_CA_LEN)
#define IS62W_RD_LEN  (IS62W_MSG_LEN - IS62W_CA_LEN - IS62W_RD_DLY)

enum is62w_op_mode {
	OP_BYTE,
	OP_SEQUENTIAL,
	OP_PAGE,
};

struct is62w_spi_dev {
	struct spi_device *spi;
	struct mutex lock;
	enum is62w_op_mode opmode;
	u32 page_sz;
	u32 page_num;
	u32 die_sz;
	u32 die_num;
	u32 capacity;
	u8 *rw_buf;
};

int is62w_spi_write(struct is62w_spi_dev *is62w, u32 off, void *val, u32 len);

int is62w_spi_read(struct is62w_spi_dev *is62w, u32 off, void *val, u32 len);
