#pragma once

#include <cstdint>
#include <vector>
#include <string>

namespace esphome {
namespace nice_bidiwifi {

// Protocol constants
static const uint8_t T4_START_CODE = 0x55;
static const uint8_t T4_START_CODE_EXT = 0xF0;
static const uint32_t T4_BAUD_RATE = 19200;
static const uint32_t T4_TX_INTERVAL_MS = 100;
static const uint32_t T4_REPLY_TIMEOUT_MS = 500;
static const uint32_t T4_DISCOVERY_INTERVAL_MS = 10000;
static const uint32_t T4_POSITION_POLL_MS = 500;
static const uint32_t T4_STATUS_REFRESH_MS = 15000;
static const uint32_t T4_IO_POLL_MS = 5000;

// Discovery backoff intervals (ms)
static const uint32_t T4_DISCOVERY_BACKOFF[] = {1000, 2000, 4000, 8000, 10000};
static const uint8_t T4_DISCOVERY_BACKOFF_COUNT = 5;

// Learned timing bounds
static const uint32_t T4_MIN_LEARNED_DURATION = 3000;    // 3 seconds
static const uint32_t T4_MAX_LEARNED_DURATION = 300000;  // 5 minutes
static const float T4_LEARNING_DEVIATION = 0.10f;        // 10% threshold

// Protocol types
enum T4Protocol : uint8_t {
  PROTO_DEP = 1,  // Simple command/response
  PROTO_DMP = 8,  // Rich info protocol
};

// DMP flags
enum T4Flags : uint8_t {
  FLAG_FIN = 0x01,
  FLAG_ACK = 0x08,
  FLAG_GET = 0x10,
  FLAG_SET = 0x20,
  FLAG_EVT = 0x40,
  FLAG_REQ = 0x80,
};

// Device types (whose field)
enum T4DeviceType : uint8_t {
  DEV_ALL = 0x00,
  DEV_CONTROLLER = 0x04,
  DEV_RADIO = 0x0A,  // OXI receiver
};

// OXI (radio receiver) command types
enum T4OxiCommand : uint8_t {
  OXI_REMOTE_LIST = 0x25,   // Remote control info/list
  OXI_BUTTON_READ = 0x26,   // Button press detected
};

// Control commands (DEP protocol)
enum T4ControlCmd : uint8_t {
  CMD_SBS = 0x01,
  CMD_STOP = 0x02,
  CMD_OPEN = 0x03,
  CMD_CLOSE = 0x04,
  CMD_P_OPN1 = 0x05,
  CMD_P_OPN2 = 0x06,
  CMD_P_OPN3 = 0x07,
  CMD_CLS_I1 = 0x08,
  CMD_CLS_I2 = 0x09,
  CMD_CLS_I3 = 0x0A,
  CMD_P_OPN4 = 0x0B,
  CMD_P_OPN5 = 0x0C,
  CMD_P_OPN6 = 0x0D,
  CMD_CLS_LOCK = 0x0E,
  CMD_LOCK = 0x0F,
  CMD_UNLOCK = 0x10,
  CMD_LIGHT_TIMER = 0x11,
  CMD_LIGHT_TOGGLE = 0x12,
  CMD_HOST_SBS = 0x13,
  CMD_HOST_OPN = 0x14,
  CMD_HOST_CLS = 0x15,
  CMD_SLAVE_SBS = 0x16,
  CMD_SLAVE_OPN = 0x17,
  CMD_SLAVE_CLS = 0x18,
  CMD_UNLK_OPN = 0x19,
  CMD_UNLK_CLS = 0x1A,
  CMD_AUTO_ON = 0x1B,
  CMD_AUTO_OFF = 0x1C,
  CMD_HALT = 0x21,
  CMD_PHOTO_OPN = 0x22,
  CMD_PHOTO = 0x23,
  CMD_PHOTO1 = 0x24,
  CMD_PHOTO2 = 0x25,
  CMD_PHOTO3 = 0x26,
  CMD_EMERGENCY_STOP = 0x27,
  CMD_EMERGENCY = 0x28,
  CMD_EMERGENCY_OPN = 0x2B,
  CMD_EMERGENCY_CLS = 0x2C,
};

// Message types
enum T4MessageType : uint8_t {
  MSG_CMD = 0x01,
  MSG_INF = 0x08,
};

// Command menu types
enum T4CmdMenu : uint8_t {
  MENU_CONTROL = 0x01,
  MENU_CUR_MAN = 0x02,
  MENU_SUBMENU = 0x04,
  MENU_MAIN_SET = 0x80,
  MENU_RUN = 0x82,
  MENU_STA = 0xC0,
};

// INF command / parameter registers
enum T4Register : uint8_t {
  REG_TYPE_M = 0x00,
  REG_STATUS = 0x01,
  REG_WHO = 0x04,
  REG_MAC = 0x07,
  REG_MANUFACTURER = 0x08,
  REG_PRODUCT = 0x09,
  REG_HARDWARE = 0x0A,
  REG_FIRMWARE = 0x0B,
  REG_DESCRIPTION = 0x0C,
  REG_INF_SUPPORT = 0x10,
  REG_CUR_POS = 0x11,
  REG_MAX_OPN = 0x12,
  REG_POS_MAX = 0x18,
  REG_POS_MIN = 0x19,
  REG_P_OPN1 = 0x21,
  REG_P_OPN2 = 0x22,
  REG_P_OPN3 = 0x23,
  REG_SLOW_OPN = 0x24,
  REG_SLOW_CLS = 0x25,
  REG_OPN_OFFSET = 0x28,
  REG_CLS_OFFSET = 0x29,
  REG_OPN_DISCHARGE = 0x2A,
  REG_CLS_DISCHARGE = 0x2B,
  REG_RESET_ENCODER = 0x2F,
  REG_REV_TIME = 0x31,
  REG_SPEED_OPN = 0x42,
  REG_SPEED_CLS = 0x43,
  REG_SPEED_DECEL = 0x44,
  REG_SPEED_DECEL_OPN = 0x45,
  REG_SPEED_DECEL_CLS = 0x46,
  REG_SPEED_SLW_OPN = 0x45,
  REG_SPEED_SLW_CLS = 0x46,
  REG_FORCE_CRUISE = 0x49,
  REG_FORCE_OPN = 0x4A,
  REG_FORCE_CLS = 0x4B,
  REG_OUT1 = 0x51,
  REG_OUT2 = 0x52,
  REG_LOCK_TIME = 0x5A,
  REG_LAMP_TIME = 0x5B,
  REG_SUCTION_TIME = 0x5C,
  REG_CMD_SBS = 0x61,
  REG_CMD_POPN = 0x62,
  REG_CMD_OPN = 0x63,
  REG_CMD_CLS = 0x64,
  REG_CMD_STP = 0x65,
  REG_CMD_PHOTO = 0x68,
  REG_CMD_PHOTO2 = 0x69,
  REG_CMD_PHOTO3 = 0x6A,
  REG_CMD_OPN_STP = 0x6B,
  REG_CMD_CLS_STP = 0x6C,
  REG_IN1 = 0x71,
  REG_IN2 = 0x72,
  REG_IN3 = 0x73,
  REG_IN4 = 0x74,
  REG_CMD_LET_OPN = 0x78,
  REG_CMD_LET_CLS = 0x79,
  REG_AUTOCLS = 0x80,
  REG_PAUSE_TIME = 0x81,
  REG_PH_CLS_ON = 0x84,
  REG_PH_CLS_TIME = 0x85,
  REG_PH_CLS_MODE = 0x86,
  REG_ALW_CLS_ON = 0x88,
  REG_ALW_CLS_TIME = 0x89,
  REG_ALW_CLS_MODE = 0x8A,
  REG_STANDBY_ACT = 0x8C,
  REG_STANDBY_TIME = 0x8D,
  REG_STANDBY_MODE = 0x8E,
  REG_START_ON = 0x90,
  REG_START_TIME = 0x91,
  REG_PEAK_ON = 0x93,
  REG_BLINK_ON = 0x94,
  REG_BLINK_OPN_TIME = 0x95,
  REG_BLINK_CLS_TIME = 0x99,
  REG_WATER_HAMMER = 0x92,
  REG_OP_BLOCK = 0x9A,
  REG_RADIO_INH = 0x9B,
  REG_KEY_LOCK = 0x9C,
  REG_SLOW_ON = 0xA2,
  REG_DIS_VAL = 0xA4,
  REG_MAINT_THRESHOLD = 0xB1,
  REG_MAINT_COUNT = 0xB2,
  REG_TOTAL_COUNT = 0xB3,
  REG_MAINT_CANCEL = 0xB4,
  // Alternate maneuver counter on some boards (e.g. CL201): B3 returns unsupported; Nice HTTP uses FNC D4 for the same value.
  REG_NUM_MOVEMENTS = 0xD4,
  REG_DIAG_BB = 0xD0,
  REG_DIAG_IO = 0xD1,
  REG_DIAG_PAR = 0xD2,
};

// DMP request types
enum T4RunCmd : uint8_t {
  RUN_GET_SUPP = 0x89,
  RUN_GET = 0x99,
  RUN_SET = 0xA9,
};

// Automation status values
enum T4AutoStatus : uint8_t {
  STA_STOPPED = 0x01,
  STA_OPENING = 0x02,
  STA_CLOSING = 0x03,
  STA_OPENED = 0x04,
  STA_CLOSED = 0x05,
  STA_PREFLASH = 0x06,  // also used as STA_ENDTIME (same wire value)
  STA_PAUSE = 0x07,
  STA_SEARCH_DEV = 0x08,
  STA_SEARCH_POS = 0x09,
  STA_PART_OPENED = 0x10,
};

// Maneuver stop/error reasons (from DIAG_BB register 0xD0)
enum T4StopReason : uint8_t {
  STOP_NORMAL = 0x00,
  STOP_OBSTACLE_ENCODER = 0x01,
  STOP_OBSTACLE_FORCE = 0x02,
  STOP_PHOTO_INTERVENTION = 0x03,
  STOP_HALT = 0x04,
  STOP_EMERGENCY = 0x05,
  STOP_ELECTRIC_ANOMALY = 0x06,
  STOP_BLOCKED = 0x07,
  STOP_TIMEOUT = 0x08,
};

// Motor types
enum T4MotorType : uint8_t {
  MOTOR_SLIDING = 0x01,
  MOTOR_SECTIONAL = 0x02,
  MOTOR_SWING = 0x03,
  MOTOR_BARRIER = 0x04,
  MOTOR_UP_AND_OVER = 0x05,
};

// Source address (2 bytes: row/series + device address)
struct T4Address {
  uint8_t row;       // series/row (0-63)
  uint8_t address;   // device address
  bool operator==(const T4Address &o) const { return row == o.row && address == o.address; }
};

static const T4Address T4_BROADCAST = {0x00, 0xFF};
static const T4Address T4_HUB_DEFAULT = {0x50, 0x90};

// Build a CMD packet (DEP protocol, control commands)
std::vector<uint8_t> t4_build_cmd(const T4Address &to, const T4Address &from, uint8_t control_cmd);

// Build an INF packet (DMP protocol, GET/SET parameters)
std::vector<uint8_t> t4_build_inf(const T4Address &to, const T4Address &from,
                                   uint8_t device_type, uint8_t register_id,
                                   uint8_t run_cmd, uint8_t next_data = 0x00,
                                   const std::vector<uint8_t> &data = {});

// Compute XOR checksum over range
uint8_t t4_xor_checksum(const uint8_t *data, size_t start, size_t count);

// Get motor type name string
const char *t4_motor_type_name(uint8_t type);

// Get automation status name string
const char *t4_auto_status_name(uint8_t status);

// Get control command name string
const char *t4_control_cmd_name(uint8_t cmd);

// Get stop reason name string
const char *t4_stop_reason_name(uint8_t reason);

// Get register name string
const char *t4_register_name(uint8_t reg);

}  // namespace nice_bidiwifi
}  // namespace esphome
