#ifndef _BLUETOOTH_H__
#define _BLUETOOTH_H__

#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_uart.h"

#include <stdbool.h>

#define BLUETOOTH_RECEIVED_DATA_BUFFER 20
#define BLUETOOTH_ADDRESS_LENGTH 17

enum Bluetooth_response
{
	BLUETOOTH_OK,
	BLUETOOTH_FAIL
};

enum Bluetooth_stopBit
{
	STOP_BIT_1 = 0,
	STOP_BIT_2 = 1,
	STOP_BIT_ERROR = 2
};

enum Bluetooth_parity
{
	PARITY_NONE = 0,
	PARITY_ODD = 1,
	PARITY_EVEN = 2
};

enum Bluetooth_moduleRole
{
	BLUETOOTH_SLAVE_ROLE,
	BLUETOOTH_MASTER_ROLE,
	BLUETOOTH_SLAVE_LOOP_ROLE,
	BLUETOOTH_UNKNOWN_ROLE
};

typedef struct
{
	uint32_t baudRate: 28;
	uint8_t stopBit: 2;
	uint8_t parity: 2;
}bluetooth_SerialParameters;

typedef struct
{
	char receivedData[BLUETOOTH_RECEIVED_DATA_BUFFER];
	uint8_t dataEnd;
	bool isDataReady;
}bluetooth_receivedDataBuffer;

typedef enum Bluetooth_response Bluetooth_response;
typedef enum Bluetooth_stopBit Bluetooth_stopBit;
typedef enum Bluetooth_parity Bluetooth_parity;
typedef enum Bluetooth_moduleRole Bluetooth_moduleRole;

typedef struct bluetooth_handler_t bluetooth_handler_t;

extern bluetooth_receivedDataBuffer bluetooth_interruptBuffer;

bluetooth_handler_t* bluetooth_init(UART_HandleTypeDef *uart_handler);
void bluetooth_destroy(bluetooth_handler_t* bluetooth);

Bluetooth_response bluetooth_pingDevice(bluetooth_handler_t *bluetooth);
Bluetooth_response bluetooth_setUartBaudrate(bluetooth_handler_t* bluetooth, uint32_t newBaudrate);

Bluetooth_response bluetooth_setSerialParameters(bluetooth_handler_t *bluetooth, bluetooth_SerialParameters serialParam);
Bluetooth_response bluetooth_getSerialParameters(bluetooth_handler_t *bluetooth, bluetooth_SerialParameters *serialParam);

Bluetooth_response bluetooth_restoreDefaultSettings(bluetooth_handler_t *bluetooth);
Bluetooth_response bluetooth_reset(bluetooth_handler_t *bluetooth);

Bluetooth_response bluetooth_sendMessage(bluetooth_handler_t *bluetooth, char* message, uint32_t timeout);
Bluetooth_response bluetooth_sendMessage_IT(bluetooth_handler_t *bluetooth, char* message);
Bluetooth_response bluetooth_sendMessage_DMA(bluetooth_handler_t *bluetooth, char* message);

Bluetooth_response bluetooth_readMessage(bluetooth_handler_t *bluetooth, char* message, uint32_t maxMessageLength, uint32_t timeout);
Bluetooth_response bluetooth_readMessage_IT(bluetooth_handler_t *bluetooth, uint32_t messageLength);
Bluetooth_response bluetooth_readMessage_DMA(bluetooth_handler_t *bluetooth, uint32_t messageLength);

Bluetooth_response bluetooth_getName(bluetooth_handler_t *bluetooth, char* name);
Bluetooth_response bluetooth_setName(bluetooth_handler_t *bluetooth, char* name);

Bluetooth_response bluetooth_getPassword(bluetooth_handler_t *bluetooth, char* password);
Bluetooth_response bluetooth_setPassword(bluetooth_handler_t *bluetooth, char* password);

Bluetooth_response bluetooth_getModuleAddress(bluetooth_handler_t *bluetooth, char moduleAddress[BLUETOOTH_ADDRESS_LENGTH + 1]);
Bluetooth_response bluetooth_getModuleRole(bluetooth_handler_t *bluetooth, Bluetooth_moduleRole* moduleRole);

#endif
