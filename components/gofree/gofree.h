#pragma once
// GoFree: Grundfos SCALA1 over Bluetooth for ESPHome.
// GENIbus framing, pump advertisement decoding, pairing request and error codes.
// https://github.com/tatutsa888/gofree (MIT). Not affiliated with or endorsed by Grundfos.
#include <cstdint>
#include <cstdio>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>
#include <esp_gap_ble_api.h>

namespace gofree {

static const char *const TAG = "gofree";
static const uint16_t COMPANY_ID = 0x0328;  // Grundfos Holding A/S (Bluetooth SIG)
static const uint16_t SERVICE = 0xFE5D;     // GENI over BLE

// GENIbus CRC: CCITT 0x1021, initial 0xFFFF, inverted, over length byte .. end of APDU.
inline uint16_t crc(const uint8_t *d, size_t n) {
  uint16_t c = 0xFFFF;
  for (size_t i = 0; i < n; i++) {
    c ^= (uint16_t) d[i] << 8;
    for (int b = 0; b < 8; b++) c = (c & 0x8000) ? (c << 1) ^ 0x1021 : (c << 1);
  }
  return ~c;
}

inline std::vector<uint8_t> seal(std::vector<uint8_t> f) {
  uint16_t c = crc(f.data() + 1, f.size() - 1);
  f.push_back(c >> 8);
  f.push_back(c & 0xFF);
  return f;
}

// Class 10 object read (object id, sub-id).
inline std::vector<uint8_t> read_obj(uint8_t id, uint16_t sub) {
  return seal({0x27, 0x07, 0xE7, 0xF8, 0x0A, 0x03, id, (uint8_t) (sub >> 8), (uint8_t) sub});
}

// Class 3 command: 2 = reset alarm, 5 = stop, 6 = auto.
inline std::vector<uint8_t> command(uint8_t id) { return seal({0x27, 0x05, 0xE7, 0xF8, 0x03, 0x81, id}); }

inline uint16_t u16(const std::vector<uint8_t> &b, size_t i) { return (b[i] << 8) | b[i + 1]; }
inline uint32_t u32(const std::vector<uint8_t> &b, size_t i) {
  return ((uint32_t) b[i] << 24) | ((uint32_t) b[i + 1] << 16) | ((uint32_t) b[i + 2] << 8) | b[i + 3];
}

// Pump advertisement, service data 0xFE5D:
// 03 01 00 <family> <type> <version> <mode> <flags> <id hi> <id lo>
struct Advert {
  bool valid = false;
  uint8_t family = 0, type = 0, flags = 0;
  char id[5] = {0};
  bool connected() const { return flags & 0x01; }
  bool alarm() const { return flags & 0x02; }
  bool warning() const { return flags & 0x04; }
  bool winking() const { return flags & 0x08; }
  bool button() const { return flags & 0x10; }  // pairing button pressed: the pump accepts pairing now
};

inline Advert parse_advert(const std::vector<uint8_t> &d) {
  Advert a;
  if (d.size() < 10) return a;
  a.valid = true;
  a.family = d[3];
  a.type = d[4];
  a.flags = d[7];
  snprintf(a.id, sizeof(a.id), "%02X%02X", d[8], d[9]);
  return a;
}

// Pairing request, as broadcast by the official app: manufacturer data 0x0328, "GF" + pump id + "02".
inline std::vector<uint8_t> pairing_request(const char *pump_id) {
  std::vector<uint8_t> m = {0x28, 0x03, 'G', 'F'};
  for (int i = 0; i < 4 && pump_id[i]; i++) m.push_back((uint8_t) pump_id[i]);
  m.push_back('0');
  m.push_back('2');
  return m;
}

// Does this ESP32 hold a pairing key for that address?
inline bool is_bonded(const uint8_t *bda) {
  int n = esp_ble_get_bond_device_num();
  if (n <= 0) return false;
  std::vector<esp_ble_bond_dev_t> list(n);
  if (esp_ble_get_bond_device_list(&n, list.data()) != ESP_OK) return false;
  for (int i = 0; i < n; i++)
    if (memcmp(list[i].bd_addr, bda, 6) == 0) return true;
  return false;
}

inline void bda_from_u64(uint64_t a, esp_bd_addr_t out) {
  for (int i = 0; i < 6; i++) out[i] = (a >> (8 * (5 - i))) & 0xFF;
}

inline bool is_bonded_u64(uint64_t a) {
  esp_bd_addr_t b;
  bda_from_u64(a, b);
  return is_bonded(b);
}

inline void forget(uint64_t a) {
  esp_bd_addr_t b;
  bda_from_u64(a, b);
  esp_ble_remove_bond_device(b);
}

// "AA:BB:CC:DD:EE:FF" -> 0xAABBCCDDEEFF (0 if empty or invalid).
inline uint64_t parse_mac(const char *s) {
  uint64_t a = 0;
  int digits = 0;
  for (; *s; s++) {
    char c = *s;
    int v = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
    if (v < 0) continue;
    a = (a << 4) | v;
    digits++;
  }
  return digits == 12 ? a : 0;
}

inline const char *family_name(uint8_t f, uint8_t t) {
  if (f == 57 && t == 1) return "SCALA1";
  return nullptr;
}

// Stable identifier of a GENI error code (used as sensor state, handy in automations).
inline std::string error_name(uint16_t code) {
  switch (code) {
    case 0: return "Ok";
    case 1: return "LeakageCurrent";
    case 2: return "MotorPhaseMissing";
    case 3: return "ExternalAlarm";
    case 4: return "MaxNumberOfRestarts";
    case 7: return "TooManyHardwareShutdowns";
    case 9: return "MotorPhaseSequenceReversal";
    case 10: return "PumpCommunication";
    case 12: return "ServiceInterval";
    case 14: return "ElectronicDCLinkProtection";
    case 15: return "CommunicationFaultMainSystem";
    case 16: return "Other";
    case 17: return "LowSystemPerformance";
    case 19: return "DiaphragmBreak";
    case 21: return "TooManyStartsPerHour";
    case 22: return "MotorMoisture";
    case 24: return "Vibration";
    case 25: return "UserWrongConfiguration";
    case 26: return "WeldingContactor";
    case 29: return "ImpellerForcedBackwards";
    case 30: return "BearingWear";
    case 31: return "VaristorChange";
    case 32: return "OverVoltage";
    case 35: return "AirInPumpHead";
    case 36: return "DischargeValveLeakage";
    case 37: return "SuctionValveLeakage";
    case 39: return "MixingLoopValveStuck";
    case 40: return "MotorLowSupplyVoltage";
    case 41: return "MotorLowSupplyVoltageTransient";
    case 42: return "CutInFault";
    case 43: return "MotorPositiveTurbineOperation";
    case 44: return "UnderTemperature";
    case 45: return "VoltageAsymmetry";
    case 46: return "ExternalWarning";
    case 48: return "MotorOverLoad";
    case 49: return "MotorOverCurrent";
    case 50: return "MotorProtectionGeneralShutDown";
    case 51: return "MotorBlocked";
    case 54: return "MotorProtectionFunction3SecondsLimit";
    case 55: return "MotorCurrentProtection";
    case 56: return "MotorUnderLoad";
    case 57: return "DryRun";
    case 58: return "LowFlow";
    case 59: return "NoFlow";
    case 61: return "WaterHammer";
    case 63: return "MediaTemperatureFrozenProtection";
    case 64: return "OverTemperature";
    case 65: return "MotorOverTemperature";
    case 66: return "ControlElectronicsOverTemperature";
    case 67: return "PowerConverterOverTemperature";
    case 68: return "HighMediaTemperatureProtection";
    case 69: return "MotorOverHeated";
    case 70: return "MotorOverHeatedThermalRelay2";
    case 72: return "MotorDriveUnitInternalFault";
    case 73: return "HardwareShutDown";
    case 74: return "MotorHighDCLinkVoltage";
    case 75: return "MotorLowDCLinkVoltage";
    case 76: return "PMCMLostNodeInternal";
    case 77: return "TwinPumpsCommunicationFault";
    case 80: return "HardwareFaultType2";
    case 83: return "FEParameterAreaVerificationError";
    case 84: return "PersistenceBreakDown";
    case 85: return "MotorDriveUnitMemoryFault";
    case 88: return "GeneralSensorFault";
    case 89: return "PrimaryFeedbackSensorSignalFault";
    case 90: return "SpeedSensorSignalFault";
    case 91: return "PumpFlowTemperatureSensor";
    case 92: return "FeedbackSensorCalibrationFault";
    case 93: return "FallbackSensorSignalFault";
    case 94: return "LimitExceeded1";
    case 95: return "LimitExceeded2";
    case 96: return "ReferenceInputSignalFault";
    case 97: return "InvalidSetpoint";
    case 105: return "ElectronicRectifierProtection";
    case 106: return "ElectronicInverterProtection";
    case 114: return "FrostProtection";
    case 117: return "DoorOpened";
    case 125: return "OutdoorTemperatureSensor";
    case 126: return "ZoneAirSupplyTemperatureSensor";
    case 127: return "ShuntRelativePressureSensor";
    case 130: return "CableTheft";
    case 131: return "Unbalance";
    case 132: return "InvalidGscFile";
    case 133: return "GenericLimitExceeded";
    case 148: return "MotorDriveEndBearingTemperatureHigh";
    case 149: return "MotorNonDriveEndBearingTemperatureHigh";
    case 152: return "CommunicationFaultAddOnModule";
    case 155: return "InrushFault";
    case 156: return "InternalFreqConverterCommunication";
    case 157: return "RealTimeClockOutOfOrder";
    case 159: return "CimCommunication";
    case 161: return "SensorSupplyFault_5v_12v";
    case 162: return "SensorSupplyFault_24v";
    case 163: return "MotorDriveUnitConfigurationFault";
    case 164: return "LiqTecSensorSignalFault";
    case 165: return "AiSignalFault";
    case 166: return "Ai2SignalFault";
    case 167: return "Ai3SignalFault";
    case 168: return "PressureSensorSignalFault";
    case 169: return "FlowSensorSignalFault";
    case 175: return "SupplyFlowTemperatureSensor";
    case 176: return "ReturnFlowTemperatureSensor";
    case 181: return "PTCSensorSignalFault";
    case 190: return "Sensor1LimitExceeded";
    case 191: return "LevelControlHighWater";
    case 203: return "AlarmOnAllPumps";
    case 204: return "InconsistencyBetweenSensors";
    case 205: return "LevelSwitchInconsistency";
    case 206: return "WaterShortageLevel1";
    case 207: return "WaterLeakage";
    case 208: return "Cavitation";
    case 210: return "Overpressure";
    case 211: return "ShuntRelativePressureSurveillance";
    case 215: return "PipeFillingTimeOut";
    case 220: return "MotorContactorWearOut";
    case 225: return "PMCMLostNodeExternal";
    case 226: return "IOModuleCommunication";
    case 228: return "FlowSwitchMonitoring";
    case 229: return "WaterOnFloor";
    case 230: return "InvalidMacAddress";
    case 236: return "MixingLoopPump1Operation";
    case 237: return "MixingLoopPump2Operation";
    case 240: return "MotorBearingLubrication";
    case 241: return "MotorPhaseFailure";
    case 242: return "MotorModelAutoRecognitionFailure";
    case 245: return "PumpMaxRunningTimeProtection";
    case 248: return "RunningOnBattery";
    case 249: return "UserConfigurableAlert";
    case 250: return "UserConfigurableAlert1";
    default: return "Code" + std::to_string(code);
  }
}

// Plain-English description of an error code.
inline std::string error_text(uint16_t code) {
  const std::string n = error_name(code);
  if (n == "DryRun") return "Dry running: the tank is probably empty";
  if (n == "NoFlow") return "No flow";
  if (n == "LowFlow") return "Flow too low";
  if (n == "TooManyStartsPerHour") return "Too many starts per hour: probably a small leak";
  if (n == "MaxNumberOfRestarts") return "Too many restarts: the pump went into safe mode";
  if (n == "WaterLeakage") return "Leak detected by the pump";
  if (n == "PumpMaxRunningTimeProtection") return "Ran too long without stopping: leak or open tap";
  if (n == "MotorOverTemperature") return "Motor overheating";
  if (n == "MotorOverHeated") return "Motor overheating";
  if (n == "OverTemperature") return "Overheating";
  if (n == "MotorBlocked") return "Motor blocked";
  if (n == "AirInPumpHead") return "Air in the pump (priming)";
  if (n == "Cavitation") return "Cavitation (suction problem)";
  if (n == "MotorLowSupplyVoltage") return "Supply voltage too low";
  if (n == "OverVoltage") return "Overvoltage";
  if (n == "MediaTemperatureFrozenProtection") return "Frost risk";
  if (n == "FrostProtection") return "Frost protection";
  if (n == "PressureSensorSignalFault") return "Pressure sensor fault";
  if (n == "FlowSensorSignalFault") return "Flow sensor fault";
  // Fallback: split the identifier into words.
  std::string w;
  for (size_t i = 0; i < n.size(); i++) {
    if (i && isupper((unsigned char) n[i]) && !isupper((unsigned char) n[i - 1])) w += ' ';
    w += n[i];
  }
  return w;
}

}  // namespace gofree
