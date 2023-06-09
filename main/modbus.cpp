#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <math.h>

#include "esp_err.h"

#include "modbus.h"

#define TAG "modbus"


modbus::modbus(void) //char *host, uint16_t port, uint8_t id)
{
  _modbus_host = nullptr;
  _modbus_port = 0;
  _modbus_slaveid = 0;

  _msg_id = 1;
  _connected = false;
}

modbus::~modbus(void)
{

}

esp_err_t modbus::SetHost(const char *host, uint16_t port)
{
  if ((host != nullptr) && (port > 0))
  {
    _modbus_host = host;
    _modbus_port = port;

    return ESP_OK;
  }

  return ESP_FAIL;
}

void modbus::SetSlaveID(int id)
{
  _modbus_slaveid = id;
}

esp_err_t modbus::Connect(void)
{
struct timeval timeout;
struct sockaddr_in slave;

  if (!_modbus_host || _modbus_port == 0)
  {
    ESP_LOGI(TAG, "Fatal: Missing port or address");

    return ESP_FAIL;
  }

  ESP_LOGI(TAG, "Connecting to %s on %d", _modbus_host, _modbus_port);
  
  _socket = socket(AF_INET, SOCK_STREAM, 0);
  if (!(_socket >= 0))
  {
    ESP_LOGI(TAG, "Fatal: socket()");

    return ESP_FAIL;
  }
  
  timeout.tv_sec = 10; // seconds
  timeout.tv_usec = 0;

  setsockopt(_socket, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof(timeout));
  setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof(timeout));

  slave.sin_family = AF_INET;
  slave.sin_addr.s_addr = inet_addr(_modbus_host);
  slave.sin_port = htons(_modbus_port);

  if (!(connect(_socket, (struct sockaddr *)&slave, sizeof(slave))>=0))
  {
    close(_socket);
    ESP_LOGI(TAG, "connect() error");

    _connected = false;
    return ESP_FAIL;
  }

  _connected = true;
  
  return ESP_OK;
}

void modbus::Close(void)
{
  close(_socket);
  _connected = false;
}

bool modbus::is_connected(void)
{
  return _connected; 
}


void modbus::BuildFrame(uint8_t *outBuf, uint16_t address, uint8_t fc)
{

  outBuf[0] = (uint8_t)(_msg_id >> 8u);
  outBuf[1] = (uint8_t)(_msg_id & 0x00FFu);
  outBuf[2] = 0;
  outBuf[3] = 0;
  outBuf[4] = 0;
  outBuf[6] = (uint8_t)_modbus_slaveid;
  outBuf[7] = (uint8_t)fc;
  outBuf[8] = (uint8_t)(address >> 8u);
  outBuf[9] = (uint8_t)(address & 0x00FFu);
}


int modbus::Read(uint16_t address, uint16_t len, int fc)
{
uint8_t buf[12];

  BuildFrame(buf, address, fc);
  
  buf[5] = 6;
  buf[10] = (uint8_t)(len >> 8u);
  buf[11] = (uint8_t)(len & 0x00FFu);
  
  return SendFrame(buf, 12);
}

/*
Read all registers in one query.
We start at address 40000 and read 109 items

This get mapped into a nice structure to read the individual values
*/
esp_err_t modbus::ReadRegisters(SolarEdgeSunSpec_t *ss)
{
  Read(MODBUS_SOLAREDGE_ADDR, MODBUS_SOLAREDGE_LENGTH, FC_READ_REGS);
  uint8_t inBuf[MAX_MSG_LENGTH];
  ssize_t k = RecvFrame(inBuf);

  if (k == -1)
    return ERR_CONN;

  return Frame2Struct((uint8_t *)&inBuf[9], ss);
}


ssize_t modbus::SendFrame(uint8_t *outBuf, size_t length)
{
  _msg_id++;

  return send(_socket, (const char *)outBuf, (size_t)length, 0);
}

ssize_t modbus::RecvFrame(uint8_t *buffer)
{
ssize_t n;

  n = recv(_socket, (char *)buffer, MAX_MSG_LENGTH, 0);

  return n;
}

/*
  Swap neccessary items in the buffer to the right endianness
  Reads buf and overwrites values in the SolarEdgeSunSpec_t structure
  The raw values from the SolarEdgeSunSpect_t are used to calculate the values for the SolarEdge_t struct.
*/
esp_err_t modbus::Frame2Struct(uint8_t *buf, SolarEdgeSunSpec_t *ss)
{
  memset(ss, 0, sizeof(SolarEdgeSunSpec_t));
  memcpy(ss, buf, sizeof(SolarEdgeSunSpec_t));

  // Validate if the data contains the proper magic number to indicate a SunSpec register set.
  ss->C_SunSpec_ID = SWAPU32(ss->C_SunSpec_ID);
  if (ss->C_SunSpec_ID != MODBUS_SOLAREDGE_MAGIC)
  {
    ESP_LOGI(TAG, "Received wrong SunSpec_ID: %" PRIx32, ss->C_SunSpec_ID);
    return ESP_FAIL;
  }

  // null terminate the string values:
  ss->C_Manufacturer[sizeof(ss->C_Manufacturer)-1] = '\0';
  ss->C_Model[sizeof(ss->C_Model)-1] = '\0';
  ss->C_Version[sizeof(ss->C_Version)-1] = '\0';
  ss->C_SerialNumber[sizeof(ss->C_SerialNumber)-1] = '\0';

  // fix endiannes of the values:
  ss->C_SunSpec_DID = SWAPU16(ss->C_SunSpec_DID);
  ss->C_SunSpec_Length = SWAPU16(ss->C_SunSpec_Length);
  ss->C_DeviceAddress = SWAPU16(ss->C_DeviceAddress);
  ss->C_SunSpec_Phase = SWAPU16(ss->C_SunSpec_Phase);
  ss->C_SunSpec_Length2 = SWAPU16(ss->C_SunSpec_Length2);
  
  ss->I_AC_Current = SWAPU16(ss->I_AC_Current);
  ss->I_AC_CurrentA = SWAPU16(ss->I_AC_CurrentA);
  ss->I_AC_CurrentB = SWAPU16(ss->I_AC_CurrentB);
  ss->I_AC_CurrentC = SWAPU16(ss->I_AC_CurrentC);
  ss->I_AC_Current_SF = SWAPU16(ss->I_AC_Current_SF);

  ss->I_AC_VoltageAB = SWAPU16(ss->I_AC_VoltageAB);
  ss->I_AC_VoltageBC = SWAPU16(ss->I_AC_VoltageBC);
  ss->I_AC_VoltageCA = SWAPU16(ss->I_AC_VoltageCA);
  ss->I_AC_VoltageAN = SWAPU16(ss->I_AC_VoltageAN);
  ss->I_AC_VoltageBN = SWAPU16(ss->I_AC_VoltageBN);
  ss->I_AC_VoltageCN = SWAPU16(ss->I_AC_VoltageCN);
  ss->I_AC_Voltage_SF= SWAPU16(ss->I_AC_Voltage_SF);
  
  ss->I_AC_Power = SWAPU16(ss->I_AC_Power);
  ss->I_AC_Power_SF = SWAPU16(ss->I_AC_Power_SF);
  
  ss->I_AC_Frequency = SWAPU16(ss->I_AC_Frequency);
  ss->I_AC_Frequency_SF = SWAPU16(ss->I_AC_Frequency_SF);
  
  ss->I_AC_VA = SWAPU16(ss->I_AC_VA);
  ss->I_AC_VA_SF = SWAPU16(ss->I_AC_VA_SF);
  ss->I_AC_VAR = SWAPU16(ss->I_AC_VAR);
  ss->I_AC_VAR_SF = SWAPU16(ss->I_AC_VAR_SF);
  ss->I_AC_PF = SWAPU16(ss->I_AC_PF);
  ss->I_AC_PF_SF = SWAPU16(ss->I_AC_PF_SF);
  
  ss->I_AC_Energy_WH = SWAPU32(ss->I_AC_Energy_WH);
  ss->I_AC_Energy_WH_SF = SWAPU16(ss->I_AC_Energy_WH_SF);
  
  ss->I_DC_Current = SWAPU16(ss->I_DC_Current);
  ss->I_DC_Current_SF = SWAPU16(ss->I_DC_Current_SF);
  
  ss->I_DC_Voltage = SWAPU16(ss->I_DC_Voltage);
  ss->I_DC_Voltage_SF = SWAPU16(ss->I_DC_Voltage_SF);
  
  ss->I_DC_Power = SWAPU16(ss->I_DC_Power);
  ss->I_DC_Power_SF = SWAPU16(ss->I_DC_Power_SF);
  
  ss->I_Temp_Sink = SWAPU16(ss->I_Temp_Sink);
  ss->I_Temp_SF = SWAPU16(ss->I_Temp_SF);
  
  ss->I_Status = SWAPU16(ss->I_Status);
  ss->I_Status_Vendor = SWAPU16(ss->I_Status_Vendor);

  return ESP_OK;
}


/*
* Use the scaling factors to calculate the real world values:
*/
esp_err_t modbus::ConvertRegisters(SolarEdgeSunSpec_t *ss, SolarEdge_t *sf)
{
// static uint8_t lastHour = -1;
// time_t t = time(NULL);

//   struct tm *ltm = localtime(&t);

  // Validate if the data contains the proper magic number to indicate a SunSpec register set.
  if (ss->C_SunSpec_ID != MODBUS_SOLAREDGE_MAGIC)
    return ESP_FAIL;

  memcpy(sf->C_Manufacturer, ss->C_Manufacturer, sizeof(ss->C_Manufacturer));
  memcpy(sf->C_Model, ss->C_Model, sizeof(ss->C_Model));
  memcpy(sf->C_Version, ss->C_Version, sizeof(ss->C_Version));
  memcpy(sf->C_SerialNumber, ss->C_SerialNumber, sizeof(ss->C_SerialNumber));

  sf->I_AC_Current = ss->I_AC_Current * (pow10(ss->I_AC_Current_SF));
  sf->I_AC_CurrentA = ss->I_AC_CurrentA * (pow10(ss->I_AC_Current_SF));
  sf->I_AC_CurrentB = ss->I_AC_CurrentB * (pow10(ss->I_AC_Current_SF));
  sf->I_AC_CurrentC = ss->I_AC_CurrentC * (pow10(ss->I_AC_Current_SF));

  sf->I_AC_VoltageAB = ss->I_AC_VoltageAB * (pow10(ss->I_AC_Voltage_SF));
  sf->I_AC_VoltageBC = ss->I_AC_VoltageBC * (pow10(ss->I_AC_Voltage_SF));
  sf->I_AC_VoltageCA = ss->I_AC_VoltageCA * (pow10(ss->I_AC_Voltage_SF));
  sf->I_AC_VoltageAN = ss->I_AC_VoltageAN * (pow10(ss->I_AC_Voltage_SF));
  sf->I_AC_VoltageBN = ss->I_AC_VoltageBN * (pow10(ss->I_AC_Voltage_SF));
  sf->I_AC_VoltageCN = ss->I_AC_VoltageCN * (pow10(ss->I_AC_Voltage_SF));

  sf->I_AC_Power = ss->I_AC_Power * (pow10(ss->I_AC_Power_SF));

  sf->I_AC_Frequency = ss->I_AC_Frequency * (pow10(ss->I_AC_Frequency_SF));

  sf->I_AC_VA = ss->I_AC_VA * (pow10(ss->I_AC_VA_SF));
  sf->I_AC_VAR = ss->I_AC_VAR * (pow10(ss->I_AC_VAR_SF));
  sf->I_AC_PF = ss->I_AC_PF * (pow10(ss->I_AC_PF_SF));
  sf->I_AC_Energy_WH = ss->I_AC_Energy_WH * (pow10(ss->I_AC_Energy_WH_SF));
  sf->I_DC_Current = ss->I_DC_Current * (pow10(ss->I_DC_Current_SF));
  sf->I_DC_Voltage = ss->I_DC_Voltage * (pow10(ss->I_DC_Voltage_SF));
  sf->I_DC_Power = ss->I_DC_Power * (pow10(ss->I_DC_Power_SF));
  sf->I_Temp_Sink = ss->I_Temp_Sink * (pow10(ss->I_Temp_SF));

  // // store the converted AC_Power_SF if a new hour is now:
  // if (lastHour != ltm->tm_min)
  // {
  //   lastHour = ltm->tm_min;
    
  //   // shift all previous measurements:
  //   for (uint8_t n=0; n != MAX_POWER_HOUR-1; n++)
  //     sf->PowerHour[n] = sf->PowerHour[n+1];

  //   sf->PowerHour[MAX_POWER_HOUR-1] = sf->I_AC_Power;
  // }
  
  return ESP_OK;
}