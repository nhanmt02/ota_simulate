#define _CRT_SECURE_NO_WARNINGS
#include <Windows.h>

#include <stdio.h>
#include <string.h>


int uart_init(char* com_name, HANDLE* uart)
{
	*uart = CreateFile(com_name,
						GENERIC_READ | GENERIC_WRITE,      // Read/Write Access
						0,                                 // No Sharing, ports cant be shared
						NULL,                              // No Security
						OPEN_EXISTING,                     // Open existing port only
						0,                                 // Non Overlapped I/O
						NULL);                             // Null for Comm Devices
	if (*uart == INVALID_HANDLE_VALUE)
		return 0;

	BOOL   Status = 0;
	DCB dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	Status = GetCommState(*uart, &dcbSerialParams);
	if (Status == FALSE)
		return 0;

	dcbSerialParams.BaudRate = CBR_115200;    //BaudRate = 115200
	dcbSerialParams.ByteSize = 8;             //ByteSize = 8
	dcbSerialParams.StopBits = ONESTOPBIT;    //StopBits = 1
	dcbSerialParams.Parity = NOPARITY;        //Parity = None

	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 100;
	if (SetCommTimeouts(*uart, &timeouts) == FALSE)
		return 0;
	return 1;
}


int uart_send_data(HANDLE* uart, char* data_buffer, int data_size)
{
	LPDWORD BytesWritten = 0;
	BOOL Status = WriteFile(*uart,
		data_buffer,
		data_size,
		&BytesWritten,  
		NULL);
	if ((Status == TRUE) && (BytesWritten == data_size))
		return 1;

	return 0;
}


void uart_available_recv(HANDLE* uart)
{
	LPDWORD dwEventMask = 0;
	SetCommMask(*uart, EV_RXCHAR);
	WaitCommEvent(*uart, &dwEventMask, NULL);
}
int uart_recv_data(HANDLE* uart, char* data_buffer, int buffer_size)
{
	LPDWORD BytesReaded = 0;
	BOOL status = 0;
	status = ReadFile(*uart,
		data_buffer,
		buffer_size,
		&BytesReaded,
		0);
	if (status != TRUE)
		return 0;
	return BytesReaded;

}


void get_file_str(FILE* file, char* buffer, size_t size)
{
	for (int i = 0; i < size; i++)
	{
		buffer[i] = (char)fgetc(file);
		//if (buffer[i] == '\n') {
		//	buffer[i] = '\r';
		//	i++;
		//	buffer[i] = '\n';
		//}
	}
}
#define START_MSG_FORMAT	"firmware size: %d\r\n"
#define PACKAGE_MSG_FORMAT	"package: %d,%d,%d\r\n"
#define READY_RESPOND		"ready to recv\r\n"

void print_hex(char* buffer, size_t size)
{
	for (int i = 0; i < size; i++)
	{
		printf("%.2X ", (int)(buffer[i]&0xFF));
	}
	
}
int main()
{
	const int firmware_size = 43916;
	const char* firmware_path = "D:\\Gateway_RiscV\\23_Lora_RISC-V_System\\GD32_Firmware_Test\\Debug\\GD32_Firmware_Test.bin";


	const int package_size = 256;
	int num_package = firmware_size / package_size;
	int byte_sended = 0;
	num_package = (firmware_size % package_size == 0) ? num_package : num_package + 1;
	FILE* firmware_file = fopen(firmware_path, "rb");
	char read_file_buff[257] = { 0 };


	HANDLE uart_windows = 0;
	if (uart_init("\\\\.\\COM3", &uart_windows))
	{
		printf("OPEN COM Success\r\n");
	}
	else
	{
		printf("OPEN COM failed\r\n");
		return 0;
	}
	char ready_check_buff[32] = { 0 };
	do
	{
		char start_msg[32] = { 0 };
		memset(ready_check_buff, 0, sizeof(ready_check_buff));
		sprintf(start_msg, START_MSG_FORMAT, firmware_size);
		//send "firmware size: %d\r\n"
		uart_send_data(&uart_windows, start_msg, strlen(start_msg));

		printf("Send start OTA\r\n");
		// wait "ready to recv\r\n"
		uart_available_recv(&uart_windows);
		uart_recv_data(&uart_windows, ready_check_buff, sizeof(ready_check_buff));

	} while (!strstr(ready_check_buff, READY_RESPOND));

	for (int i = 0; i < num_package; i++)
	{
		char package_msg[32] = { 0 };
		memset(read_file_buff, 0, sizeof(read_file_buff));
		int byte_read = 0;
		byte_read = (firmware_size - byte_sended >= 256) ? package_size : firmware_size - byte_sended;
		byte_sended += byte_read;
		printf("\rSend package: [%d]%d/%d\r\n", byte_read, i + 1, num_package);
		sprintf(package_msg, PACKAGE_MSG_FORMAT, byte_read, num_package, i+1);
		int check_status = 0;
		char ok_buff[4] = { 0 };
		do
		{	
			//send "package: %d,%d,%d\r\n"
			uart_send_data(&uart_windows, package_msg, strlen(package_msg));

			//wait "OK\r\n"
			uart_available_recv(&uart_windows);
			uart_recv_data(&uart_windows, ok_buff, sizeof(ok_buff));
			get_file_str(firmware_file, read_file_buff, byte_read);
			printf("\r\ndata: ");
			print_hex(read_file_buff, byte_read);
			printf("\r\n");
			uart_send_data(&uart_windows, read_file_buff, byte_read);
			uart_available_recv(&uart_windows);
			char check_data[257] = { 0 };
			uart_recv_data(&uart_windows, check_data, byte_read);
			printf("\r\ncheck data: ");
			print_hex(check_data, byte_read);
			printf("\r\n");
			check_status = memcmp(check_data, read_file_buff, byte_read);
		} while (!byte_read);
	}
	return 0;
}