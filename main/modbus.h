#pragma once

#include <cstring>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <esp_err.h>
#include <esp_log.h>

#include "sunspec.h"

#define SWAPU16(data) ((((data) >> 8) & 0x00FF) | (((data) << 8) & 0xFF00))
#define SWAPU32(data) ((((data) >> 24) & 0x000000FF) | (((data) >> 8) & 0x0000FF00) | (((data) << 8) & 0x00FF0000) | (((data) << 24) & 0xFF000000))

// See https://modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
// TCP MODBUS ADU = 253 bytes + MBAP (7 bytes) = 260 bytes
#define MAX_MSG_LENGTH 260

// See https://knowledge-center.solaredge.com/sites/kc/files/sunspec-implementation-technical-note.pdf
#define MODBUS_SOLAREDGE_ADDR   40000
#define MODBUS_SOLAREDGE_LENGTH 109
#define MODBUS_SOLAREDGE_MAGIC  0x53756e53

// modbus function codes
enum { FC_READ_COILS = 0x01, FC_READ_INPUT_BITS, FC_READ_REGS, FC_READ_INPUT_REGS, FC_WRITE_COIL, FC_WRITE_REG, FC_WRITE_COILS, FC_WRITE_REGS };

#pragma pack(push, 1)
typedef struct
{
    uint16_t tid;      // 0..1 transaction identifier
    uint16_t pid;      // 2..3 protocol identifier
    uint8_t _len;      // always 0 (high byte of len)
    uint8_t len;       // 5 message length
    uint8_t uid;       // 6 unit id
    uint8_t fc;        // 7 function code
    uint8_t count;     // 8 byte count
    uint8_t data[150]; // additional data
} modbus_frame_t;
#pragma pack(pop)

class modbus
{
public:
    modbus();
    ~modbus();

    esp_err_t Connect(void);
    void Close(void);

    bool is_connected(void);

    esp_err_t SetHost(const char *host, uint16_t port = 502);
    void SetSlaveID(int id);

    esp_err_t ReadRegisters(SolarEdgeSunSpec_t *);
    esp_err_t ConvertRegisters(SolarEdgeSunSpec_t *, SolarEdge_t *);

private:
    const char *_modbus_host;
    uint16_t _modbus_port;
    uint32_t _msg_id;
    bool _connected;
    int _modbus_slaveid;

    int _socket;
    int err_no;

    void BuildFrame(uint8_t *to_send, uint16_t address, uint8_t func);
    int Read(uint16_t address, uint16_t amount, int func);
    ssize_t SendFrame(uint8_t *to_send, size_t length);
    ssize_t RecvFrame(uint8_t *buffer);

    esp_err_t Frame2Struct(uint8_t *, SolarEdgeSunSpec_t *);
};
