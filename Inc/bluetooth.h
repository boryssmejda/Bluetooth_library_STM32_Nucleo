#ifndef _BLUETOOTH_H__
#define _BLUETOOTH_H__

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"

enum Bluetooth_response
{
	BLUETOOTH_OK,
	BLUETOOTH_FAIL
};

typedef enum Bluetooth_response Bluetooth_response;

typedef struct bluetooth_handler_t bluetooth_handler_t;

bluetooth_handler_t* bluetooth_init(UART_HandleTypeDef *uart_handler);
void bluetooth_destroy(bluetooth_handler_t* bluetooth);
Bluetooth_response bluetooth_pingDevice(bluetooth_handler_t* bluetooth);
Bluetooth_response bluetooth_setBaudrate(bluetooth_handler_t* bluetooth, uint32_t desired_baudrate);

#endif
