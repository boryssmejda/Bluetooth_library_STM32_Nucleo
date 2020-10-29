/* Includes ------------------------------------------------------------------*/
#include "bluetooth.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>

#define OK_RESPONSE_SIZE 4

static const char* OK_RESPONSE = "OK\r\n";

struct bluetooth_handler_t
{
	UART_HandleTypeDef *uart_handler;
};

/** Functions ----------------------------------------------------------------*/
bluetooth_handler_t* bluetooth_init(UART_HandleTypeDef *huart)
{
	bluetooth_handler_t *bluetooth = malloc(sizeof(bluetooth_handler_t));

	if(bluetooth != NULL)
	{
		bluetooth->uart_handler = huart;
	}
	return bluetooth;
}

void bluetooth_destroy(bluetooth_handler_t* bluetooth)
{
	if(bluetooth)
	{
		free(bluetooth);
	}
}

Bluetooth_response bluetooth_pingDevice(bluetooth_handler_t* bluetooth)
{
	assert(bluetooth != NULL);

	if(bluetooth->uart_handler == NULL)
	{
		return BLUETOOTH_FAIL;
	}

	const char* test_command = "AT\r\n";
	const uint8_t command_length = strlen(test_command);
	const uint16_t timeout = 100; // ms

	HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)test_command, command_length, timeout);

	char response[OK_RESPONSE_SIZE + 1];
	HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)response, sizeof(response), timeout);

	if(strcmp(response, OK_RESPONSE) == 0)
	{
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}

Bluetooth_response bluetooth_setBaudrate(bluetooth_handler_t* bluetooth, uint32_t desired_baudrate)
{
	assert(desired_baudrate != 0);

	return BLUETOOTH_OK;
}
