/* Includes ------------------------------------------------------------------*/
#include "bluetooth.h"

#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>

#define OK_RESPONSE_SIZE 4
#define TIMEOUT 100
#define MINIMAL_SET_SERIAL_PARAMETER_LENGTH 30
#define PIN_LENGTH 4
#define GET_PASSWORD_COMMAND_RESPONSE_LENGTH 17
#define SET_PASSWORD_COMMAND_LENGTH 14
#define GET_NAME_RESPONSE_SIZE 30
#define BLUETOOTH_MODULE_ADDRESS_RESPONSE 26
#define BLUETOOTH_MODULE_ROLE_RESPONSE_LENGTH 13

static const char* OK_RESPONSE = "OK\r\n";

struct bluetooth_handler_t
{
	UART_HandleTypeDef *uart_handler;
};

bluetooth_receivedDataBuffer bluetooth_interruptBuffer = {.isDataReady = false, .dataEnd = 0};

/** Static Functions -------------------------------------------------------- */
static bool startsWith(char* word, char* pattern)
{
	int i = -1;
	while(pattern[++i] != '\0')
	{
		if(word[i] != pattern[i])
		{
			return false;
		}
	}

	return true;
}

static bool endsWith(char* word, char* pattern)
{
	const uint8_t wordLength = strlen(word);
	const uint8_t patternLength = strlen(pattern);

	// we start at the end of both words
	int8_t i_pattern = patternLength - 1;
	int8_t i_word = wordLength - 1;

	while(i_pattern >= 0)
	{
		// we compare if chars are the same
		if(pattern[i_pattern] != word[i_word])
		{
			return false;
		}

		--i_pattern;
		--i_word;
	}

	return true;
}

static uint8_t findPosOf(char* str, char ch)
{
	uint8_t i = 0;
	while(str[i] != '\0' && str[i++] != ch);

	return i;
}

static void copyUntil(char* src, char* dst, uint8_t begin, char desiredChar)
{
	uint8_t i = begin;
	uint8_t dstIndex = 0;
	while(src[i] != '\0' && src[i] != desiredChar)
	{
		dst[dstIndex++] = src[i++];
	}
}

static void getRoleFromResponse(char* response, Bluetooth_moduleRole* role)
{
	assert(response);
	assert(role);
	// response format is: +ROLE:<Param>\r\nOK\r\n
	// So the role is on the sixth position

	switch(response[6])
	{
	case '0':
		*role = BLUETOOTH_SLAVE_ROLE;
		break;
	case '1':
		*role = BLUETOOTH_MASTER_ROLE;
		break;
	case '2':
		*role = BLUETOOTH_SLAVE_LOOP_ROLE;
		break;
	default:
		*role = BLUETOOTH_UNKNOWN_ROLE;
		break;
	}
}

static bool isModuleRoleCorrect(char* moduleRole)
{
	if(startsWith(moduleRole, "+ROLE:"))
	{
		return true;
	}
	else
	{
		return false;
	}
}

static void getAddressFromResponse(char* response, char moduleAddress[BLUETOOTH_ADDRESS_LENGTH + 1])
{
	uint8_t colonPos = findPosOf(response, ':');

	moduleAddress[0] = response[colonPos];
	moduleAddress[1] = response[colonPos + 1];

	moduleAddress[2] = ':';

	moduleAddress[3] = response[colonPos + 2];
	moduleAddress[4] = response[colonPos + 3];

	moduleAddress[5] = ':';

	moduleAddress[6] = response[colonPos + 5];
	moduleAddress[7] = response[colonPos + 6];

	moduleAddress[8] = ':';

	moduleAddress[9] = response[colonPos + 8];
	moduleAddress[10] = response[colonPos + 9];

	moduleAddress[11] = ':';

	moduleAddress[12] = response[colonPos + 10];
	moduleAddress[13] = response[colonPos + 11];

	moduleAddress[14] = ':';

	moduleAddress[15] = response[colonPos + 12];
	moduleAddress[16] = response[colonPos + 13];

	// Finally we should get address in form: ab:cd:12:34:fg:23
}

static bool isModuleAddressCorrect(char address[BLUETOOTH_MODULE_ADDRESS_RESPONSE + 1])
{
	if(startsWith(address, "+ADDR:"))
	{
		return true;
	}
	else
	{
		return false;
	}
}

static void getModuleNameFromResponse(char* response, char* name)
{
	/*Response format is: +NAME:<Param>\r\nOK\r\n
	 * So we first have to find position of colon
	 * Then we traverse array and copy chars into name until we find \r
	 * */

	uint8_t responseIndex = findPosOf(response, ':');
	copyUntil(response, name, responseIndex, '\r');
}

static bool isGetNameResponseCorrect(char* response)
{
	if(startsWith(response, "+NAME:"))
	{
		return true;
	}
	else
	{
		return false;
	}
}

static bool isPasswordResponseCorrect(char* response)
{
	if(startsWith(response, "+PIN:\"") && endsWith(response, "OK\r\n"))
	{
		return true;
	}
	else
	{
		return false;
	}
}

static void getPasswordFromResponse(char *response, char *password)
{
	// pin starts from seventh byte and pin is 4 bytes long
	password[0] = response[6];
	password[1] = response[7];
	password[2] = response[8];
	password[3] = response[9];
}

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

static Bluetooth_stopBit getStopBit(char* response)
{
	int i = -1;
	while(response[++i] != ',');

	uint8_t stopBit = response[i+1] - '0';

	if(stopBit == 0)
	{
		return STOP_BIT_1;
	}
	else if(stopBit == 1)
	{
		return STOP_BIT_2;
	}
	else
	{
		return STOP_BIT_ERROR;
	}
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

	HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)response, OK_RESPONSE_SIZE, timeout);

	if(strcmp(response, OK_RESPONSE) == 0)
	{
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}

Bluetooth_response bluetooth_setUartBaudrate(bluetooth_handler_t* bluetooth, uint32_t newBaudrate)
{
	assert(newBaudrate != 0);

	bluetooth->uart_handler->Instance->CR1 &= ~(USART_CR1_UE);
	bluetooth->uart_handler->Instance->BRR = UART_BRR_SAMPLING16(HAL_RCC_GetPCLK2Freq(), newBaudrate);
	bluetooth->uart_handler->Instance->CR1 |= USART_CR1_UE;

	bluetooth->uart_handler->Init.BaudRate = newBaudrate;

	return BLUETOOTH_OK;
}

Bluetooth_response bluetooth_setSerialParameters(bluetooth_handler_t* bluetooth, bluetooth_SerialParameters serialParam)
{
	assert(serialParam.baudRate != 0);
	assert(serialParam.stopBit == STOP_BIT_1 || serialParam.stopBit == STOP_BIT_2);
	assert(serialParam.parity == PARITY_ODD || serialParam.parity == PARITY_EVEN || serialParam.parity == PARITY_NONE);
	assert(bluetooth->uart_handler);

	/*Message is in format:
	 * AT+UART:baud_rate,stop_bit,parity_bit\r\n
	 * */
	uint8_t serialParameterCommand[MINIMAL_SET_SERIAL_PARAMETER_LENGTH];
	memset(serialParameterCommand, 0, sizeof(serialParameterCommand));

	uint8_t commandLength = sprintf((char*)serialParameterCommand, "AT+UART:%u,%u,%u\r\n", serialParam.baudRate, serialParam.stopBit, serialParam.parity);

	HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)serialParameterCommand, commandLength, TIMEOUT);

	char* answer[OK_RESPONSE_SIZE + 1];
	HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)answer, OK_RESPONSE_SIZE, TIMEOUT);
	answer[OK_RESPONSE_SIZE] = '\0';

	if(strcmp((const char*)answer, OK_RESPONSE) == 0)
	{
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}

Bluetooth_response bluetooth_getSerialParameters(bluetooth_handler_t *bluetooth, bluetooth_SerialParameters *serialParam)
{
	assert(bluetooth);
	assert(bluetooth->uart_handler);
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

Bluetooth_response bluetooth_restoreDefaultSettings(bluetooth_handler_t* bluetooth)
{
	assert(bluetooth);
	assert(bluetooth->uart_handler);

	char* restoreSettingsCommand = "AT+ORGL\r\n";
	char restoreSettingsResponse[OK_RESPONSE_SIZE + 1];

	HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*) restoreSettingsCommand, strlen(restoreSettingsCommand), TIMEOUT);
	HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*) restoreSettingsResponse, sizeof(restoreSettingsResponse), TIMEOUT);

	if(strcmp(restoreSettingsResponse, OK_RESPONSE) != 0)
	{
		return BLUETOOTH_FAIL;
	}
	else
	{
		return BLUETOOTH_OK;
	}
}

Bluetooth_response bluetooth_reset(bluetooth_handler_t *bluetooth)
{
	char resetCommand[] = "AT+RESET\r\n";
	if(HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)resetCommand, sizeof(resetCommand), TIMEOUT) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}

	char resetResponse[OK_RESPONSE_SIZE + 1];
	if(HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)resetResponse, OK_RESPONSE_SIZE, TIMEOUT) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}
	resetResponse[OK_RESPONSE_SIZE] = '\0';

	if(strcmp(resetResponse, OK_RESPONSE) != 0)
	{
		return BLUETOOTH_FAIL;
	}
	else
	{
		return BLUETOOTH_OK;
	}
}

Bluetooth_response bluetooth_sendMessage(bluetooth_handler_t *bluetooth, char* message, uint32_t timeout)
{
	assert(bluetooth);
	assert(message);

	uint8_t messageLength = strlen(message);

	if(HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)message, messageLength, timeout) == HAL_OK)
	{
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}

Bluetooth_response bluetooth_sendMessage_IT(bluetooth_handler_t *bluetooth, char* message)
{
	assert(bluetooth);
	assert(message);

	uint8_t length = strlen(message);

	if(HAL_UART_Transmit_IT(bluetooth->uart_handler, (uint8_t*)message, length) == HAL_OK)
	{
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}

Bluetooth_response bluetooth_sendMessage_DMA(bluetooth_handler_t *bluetooth, char* message)
{
	assert(bluetooth);
	assert(message);

	uint8_t length = strlen(message);

	if(HAL_UART_Transmit_DMA(bluetooth->uart_handler, (uint8_t*)message, length) == HAL_OK)
	{
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}

Bluetooth_response bluetooth_readMessage(bluetooth_handler_t *bluetooth, char* message, uint32_t maxMessageLength, uint32_t timeout)
{
	assert(bluetooth);
	assert(message);
	assert(maxMessageLength > 0);

	uint32_t index = 0;
	uint8_t ch;
	while(HAL_UART_Receive(bluetooth->uart_handler, &ch, 1, timeout) != HAL_TIMEOUT)
	{
		message[index++] = ch;
		if(index == maxMessageLength - 1)
		{
			message[index] = '\0';
			break;
		}
	}

	return BLUETOOTH_OK;
}

Bluetooth_response bluetooth_readMessage_IT(bluetooth_handler_t *bluetooth, uint32_t messageLength)
{
	assert(bluetooth);
	assert(messageLength < BLUETOOTH_RECEIVED_DATA_BUFFER);

	bluetooth_interruptBuffer.dataEnd = messageLength;

	if(HAL_UART_Receive_IT(bluetooth->uart_handler, (uint8_t*)bluetooth_interruptBuffer.receivedData, messageLength) == HAL_OK)
	{
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}

Bluetooth_response bluetooth_readMessage_DMA(bluetooth_handler_t *bluetooth, uint32_t messageLength)
{
	assert(bluetooth);
	assert(messageLength < BLUETOOTH_RECEIVED_DATA_BUFFER);

	bluetooth_interruptBuffer.dataEnd = messageLength;

	if(HAL_UART_Receive_DMA(bluetooth->uart_handler, (uint8_t*)bluetooth_interruptBuffer.receivedData, messageLength) == HAL_OK)
	{
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}

Bluetooth_response bluetooth_getName(bluetooth_handler_t *bluetooth, char* name)
{
	assert(bluetooth);
	assert(name);

	char* getModuleName = "AT+NAME\r\n";
	HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)getModuleName, strlen(getModuleName), TIMEOUT);

	char response[GET_NAME_RESPONSE_SIZE + 1];
	uint8_t responseIndex = -1;
	char ch;
	while(HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)&ch, 1, TIMEOUT) != HAL_TIMEOUT)
	{
		response[++responseIndex] = ch;
		if(responseIndex == GET_NAME_RESPONSE_SIZE)
		{
			return BLUETOOTH_FAIL;
		}
	}
	response[responseIndex + 1] = '\0';

	if(isGetNameResponseCorrect(response))
	{
		getModuleNameFromResponse(response, name);
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}
Bluetooth_response bluetooth_setName(bluetooth_handler_t *bluetooth, char* name)
{
	assert(bluetooth);
	assert(name);
	assert(strlen(name) > 0);

	char setNameCommand[GET_NAME_RESPONSE_SIZE + 1];
	uint8_t writtenChars = sprintf(setNameCommand, "AT+NAME=\"%s\"\r\n", name);

	if(HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)setNameCommand, writtenChars, TIMEOUT) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}

	char setNameResponse[OK_RESPONSE_SIZE + 1];
	HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)setNameResponse, OK_RESPONSE_SIZE, TIMEOUT);
	setNameResponse[OK_RESPONSE_SIZE] = '\0';

	if(strcmp(setNameResponse, OK_RESPONSE) != 0)
	{
		return BLUETOOTH_FAIL;
	}
	else
	{
		return BLUETOOTH_OK;
	}
}

Bluetooth_response bluetooth_getPassword(bluetooth_handler_t *bluetooth, char* password)
{
	assert(bluetooth);
	assert(password);

	char* getPasswordCommand = "AT+PSWD\r\n";
	HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)getPasswordCommand, strlen(getPasswordCommand), TIMEOUT);

	char response[GET_PASSWORD_COMMAND_RESPONSE_LENGTH + 1];
	if(HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)response, GET_PASSWORD_COMMAND_RESPONSE_LENGTH, TIMEOUT) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}
	response[GET_PASSWORD_COMMAND_RESPONSE_LENGTH] = '\0'; // to be sure it's null-terminated when passing to function expecting it to be null-terminated

	if(isPasswordResponseCorrect(response))
	{
		getPasswordFromResponse(response, password);
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}
Bluetooth_response bluetooth_setPassword(bluetooth_handler_t *bluetooth, char* password)
{
	assert(bluetooth);
	assert(password);
	assert(strlen(password) == PIN_LENGTH);

	char setPasswordCommand[SET_PASSWORD_COMMAND_LENGTH + 1];

	int writtenChars = sprintf(setPasswordCommand, "AT+PSWD=\"%s\"\r\n", password);

	if(HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)setPasswordCommand, writtenChars, TIMEOUT) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}

	char bluetoothResponse[OK_RESPONSE_SIZE + 1];
	if(HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)bluetoothResponse, OK_RESPONSE_SIZE, TIMEOUT) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}

	if(strcmp(bluetoothResponse, OK_RESPONSE) != 0)
	{
		return BLUETOOTH_FAIL;
	}
	else
	{
		return BLUETOOTH_OK;
	}
}

Bluetooth_response bluetooth_getModuleAddress(bluetooth_handler_t *bluetooth, char moduleAddress[BLUETOOTH_ADDRESS_LENGTH + 1])
{
	char* bluetoothModuleAddressCommand = "AT+ADDR?\r\n";

	if(HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)bluetoothModuleAddressCommand, strlen(bluetoothModuleAddressCommand), TIMEOUT) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}

	char moduleBluetoothResponse[BLUETOOTH_MODULE_ADDRESS_RESPONSE + 1];

	if(HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)moduleBluetoothResponse, BLUETOOTH_MODULE_ADDRESS_RESPONSE, TIMEOUT) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}
	moduleBluetoothResponse[BLUETOOTH_MODULE_ADDRESS_RESPONSE] = '\0';

	if(isModuleAddressCorrect(moduleBluetoothResponse))
	{
		getAddressFromResponse(moduleBluetoothResponse, moduleAddress);
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}

Bluetooth_response bluetooth_getModuleRole(bluetooth_handler_t *bluetooth, Bluetooth_moduleRole* moduleRole)
{
	char* moduleRoleCommand = "AT+ROLE\r\n";

	if(HAL_UART_Transmit(bluetooth->uart_handler, (uint8_t*)moduleRoleCommand, strlen(moduleRoleCommand), TIMEOUT + 1000) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}

	char moduleRoleResponse[BLUETOOTH_MODULE_ROLE_RESPONSE_LENGTH + 1];

	if(HAL_UART_Receive(bluetooth->uart_handler, (uint8_t*)moduleRoleResponse, BLUETOOTH_MODULE_ROLE_RESPONSE_LENGTH, TIMEOUT + 1000) != HAL_OK)
	{
		return BLUETOOTH_FAIL;
	}
	moduleRoleResponse[BLUETOOTH_MODULE_ROLE_RESPONSE_LENGTH] = '\0';

	if(isModuleRoleCorrect(moduleRoleResponse))
	{
		getRoleFromResponse(moduleRoleResponse, moduleRole);
		return BLUETOOTH_OK;
	}
	else
	{
		return BLUETOOTH_FAIL;
	}
}
