#pragma once

#include <stdint.h>

/*
 * structure to map all SunSpec registers provided by SolarEdge to a single buffer containing all values.
 * This reduces network traffic and makes parsing really convenient.
 */

#pragma pack(push, 1)
typedef struct
{
    uint32_t C_SunSpec_ID;      // a: 40000, l:2 must contain the signature 0x53756e53 (SunS)
    uint16_t C_SunSpec_DID;     // a: 40002, l:1 0x0001 = SunSpec Common Block
    uint16_t C_SunSpec_Length;  // a: 40003, l:1 65 = length of block in 16-bit registers
    uint8_t C_Manufacturer[32]; // a: 40004, l:16 SunSpec registration "SolarEdge "
    uint8_t C_Model[32];        // a: 40020, l:16 SolarEdge model
    uint8_t _filler0[16];       // a: 40036, l:8 -unknown-
    uint8_t C_Version[16];      // a: 40044, l:8 SolarEdge version
    uint8_t C_SerialNumber[32]; // a: 40052, l:16 SolarEdge Serialnumber
    uint16_t C_DeviceAddress;   // a: 40068, l:1 Modbus Unit ID
    uint16_t C_SunSpec_Phase;   // a: 40069, l:1 101 = single phase, 102 = split phase, 103 = three phase
    uint16_t C_SunSpec_Length2; // a: 40070, l:1 50 = length of model block
    uint16_t I_AC_Current;      // a: 40071, l:1 Amps AC Total Current value
    uint16_t I_AC_CurrentA;     // a: 40072, l:1 Amps AC Phase A Current value
    uint16_t I_AC_CurrentB;     // a: 40073, l:1 Amps AC Phase B Current value
    uint16_t I_AC_CurrentC;     // a: 40074, l:1 Amps AC Phase C Current value
    int16_t I_AC_Current_SF;    // a: 40075, l:1 AC Current scale factor
    uint16_t I_AC_VoltageAB;    // a: 40076, l:1 Volts AC Voltage Phase AB value
    uint16_t I_AC_VoltageBC;    // a: 40077, l:1 Volts AC Voltage Phase BC value
    uint16_t I_AC_VoltageCA;    // a: 40078, l:1 Volts AC Voltage Phase CA value
    uint16_t I_AC_VoltageAN;    // a: 40079, l:1 Volts AC Voltage Phase A to N value
    uint16_t I_AC_VoltageBN;    // a: 40080, l:1 Volts AC Voltage Phase B to N value
    uint16_t I_AC_VoltageCN;    // a: 40081, l:1 Volts AC Voltage Phase C to N value
    int16_t I_AC_Voltage_SF;    // a: 40082, l:1 AC Voltage scale factor
    int16_t I_AC_Power;         // a: 40083, l:1 Watts AC Power value
    int16_t I_AC_Power_SF;      // a: 40084, l:1 AC Power scale factor
    uint16_t I_AC_Frequency;    // a: 40085, l:1 Hertz AC Frequency value
    int16_t I_AC_Frequency_SF;  // a: 40086, l:1 Scale factor
    int16_t I_AC_VA;            // a: 40087, l:1 VA Apparent Power
    int16_t I_AC_VA_SF;         // a: 40088, l:1 Scale factor
    int16_t I_AC_VAR;           // a: 40089, l:1 VAR Reactive Power
    int16_t I_AC_VAR_SF;        // a: 40090, l:1 Scale factor
    int16_t I_AC_PF;            // a: 40091, l:1 % Power Factor
    int16_t I_AC_PF_SF;         // a: 40092, l:1 Scale factor
    uint32_t I_AC_Energy_WH;    // a: 40093, l:2 WattHours AC Lifetime Energy production
    uint16_t I_AC_Energy_WH_SF; // a: 40095, l:1 Scale factor
    uint16_t I_DC_Current;      // a: 40096, l:1 Amps DC Current value
    int16_t I_DC_Current_SF;    // a: 40097, l:1 Scale factor
    uint16_t I_DC_Voltage;      // a: 40098, l:1 Volts DC Voltage value
    int16_t I_DC_Voltage_SF;    // a: 40099, l:1 Scale factor
    int16_t I_DC_Power;         // a: 40100, l:1 Watts DC Power value
    int16_t I_DC_Power_SF;      // a: 40101, l:1 Scale factor
    uint8_t _filler1[2];        // a: 40102, l:1 -unknown-
    int16_t I_Temp_Sink;        // a: 40103, l:1 Degrees C Heat Sink Temperature
    uint8_t _filler2[4];        // a: 40104, l:2 -unknown-
    int16_t I_Temp_SF;          // a: 40106, l:1 Scale factor
    uint16_t I_Status;          // a: 40107, l:1 Operating State
    uint16_t I_Status_Vendor;   // a: 40108, l:1 Vendor-defined operating state and error codes.
} SolarEdgeSunSpec_t;
#pragma pack(pop)

/*
 * structure with the parsed values, scaling factors etc. applied.
 */
typedef struct
{
    uint8_t C_Manufacturer[32];
    uint8_t C_Model[32];
    uint8_t C_Version[16];
    uint8_t C_SerialNumber[32];

    float I_AC_Current;  // Total current
    float I_AC_CurrentA; // Phase A current
    float I_AC_CurrentB; // Phase B current
    float I_AC_CurrentC; // Phase C current

    float I_AC_VoltageAB; // Volts AC Voltage Phase AB value
    float I_AC_VoltageBC; // Volts AC Voltage Phase BC value
    float I_AC_VoltageCA; // Volts AC Voltage Phase CA value

    float I_AC_VoltageAN; // Volts AC Voltage Phase A to N value
    float I_AC_VoltageBN; // Volts AC Voltage Phase B to N value
    float I_AC_VoltageCN; // Volts AC Voltage Phase C to N value

    float I_AC_Power;     // Watts AC Power value
    float I_AC_Frequency; // Hertz AC Frequency value
    float I_AC_VA;        // VA Apparent Power
    float I_AC_VAR;       // VAR Reactive Power
    float I_AC_PF;        // % Power Factor

    float I_AC_Energy_WH;         // WattHours AC Lifetime Energy production
    float I_AC_Energy_WH_Last24H; // place holder to store the I_AC_Energy_WH for 'yesterday' to calculate daily production

    float I_DC_Current; // Amps DC Current value
    float I_DC_Voltage; // Volts DC Voltage value
    float I_DC_Power;   // Watts DC Power value

    float I_Temp_Sink;        // Degrees C Heat Sink Temperature
    uint16_t I_Status;        // Operating state
    uint16_t I_Status_Vendor; // Vendor-defined operating state and error codes.

    bool UpdatePower_24H; // flag to signal AVG_Power_24H has been updated, reset by GUI_UpdatePanels once used.
    float AVG_Power_24H;  // scaling average over 6 minutes of values

    bool UpdatePower_1H; // flag to signal AVG_Power_1H has been updated, reset by GUI_UpdatePanels once used.
    float AVG_Power_1H;  // scaling average over 1 minutes of values
} SolarEdge_t;
