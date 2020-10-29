/* Includes ------------------------------------------------------------------*/
#include "bluetooth.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>

#define OK_RESPONSE_SIZE 4
#define TIMEOUT 100

static const char* OK_RESPONSE = "OK\r\n";

struct bluetooth_handler_t
{
	UART_HandleTypeDef *uart_handler;
};


/** Static Functions -------------------------------------------------------- */
static bool isBluetoothResponseCorrect(char* response)
{
	int responseLength = strlen(response);

	/*+UART:baudrate,stopBit,parityOK\r\n*/
	/*last 4 bytes should be equal to OK\r\n*/
	char ok_buffer[5];
	memcpy(ok_buffer, response + responseLength - 4, sizeof(ok_buffer));

	if(strcmp(ok_buffer, OK_RESPONSE) == 0)
	{
		return true;
	}
	else
	{
		return false;
	}
}

static uint32_t getBaudRate(char* response)
{
	/*Max used baudrate is 5.25MBit, so the buffer should have space for 8 chars*/
	char baudrateBuffer[8];

	/*
	 * Message form is: +UART:baudrate,stop_bit,parityOK\r\n
	 * So baudrate info starts at sixth index
	 * */

	const uint8_t baudrateIndexBegin = 6;
	uint8_t baudrateIndexEnd = baudrateIndexBegin;
	while(response[++baudrateIndexEnd] != ',');

	const uint8_t baudrateLength = baudrateIndexEnd - baudrateIndexBegin;
	memcpy(baudrateBuffer, response + baudrateIndexBegin, baudrateLength);

	baudrateBuffer[baudrateLength] = '\0';

	return atoi(baudrateBuffer);
}

static uint8_t getStopBit(char* response)
{
	int i = -1;
	while(response[++i] != ',');

	uint8_t stopBit = response[i+1] - '0';
	return stopBit;
}

static uint8_t getParity(char* response)
{
	/*Find second comma*/
	int i = -1;
	while(response[++i] != ',');
	while(response[++i] != ',');

	uint8_t parity = response[i+1] - '0';
	return parity;
}

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
	response[OK_RESPONSE_SIZE] = '\0';

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

Bluetooth_response bluetooth_getSerialParameters(bluetooth_handler_t *bluetooth, bluetooth_SerialParameters *serialParam)
{
	assert(bluetooth);
	assert(serialParam);

	const char* currentBaudrate = "AT+UART\r\n";
	const int command_length = strlen(currentBaudrate);

	HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)currentBaudrate, command_length, TIMEOUT);

	char response[100];
	memset(response, 0, sizeof(response));
	uint8_t currentIndex = 0;
	uint8_t pickedChar = 0;
	while(HAL_UART_Receive(bluetooth->uart_handler, &pickedChar, 1, TIMEOUT) != HAL_TIMEOUT)
	{
		if(currentIndex >= 100)
		{
			break;
		}

		response[currentIndex++] = pickedChar;
	}
	response[currentIndex] = '\0';

	if(!isBluetoothResponseCorrect(response))
	{
		return BLUETOOTH_FAIL;
	}

	serialParam->baudRate = getBaudRate(response);
	serialParam->parity = getParity(response);
	serialParam->stopBit = getStopBit(response);

	return BLUETOOTH_OK;
}

Bluetooth_response bluetooth_getDefaultBaudrate(bluetooth_handler_t* bluetooth)
{
	return BLUETOOTH_OK;
}
