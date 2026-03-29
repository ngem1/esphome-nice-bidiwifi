#include "t4_protocol.h"

namespace esphome {
namespace nice_bidiwifi {

uint8_t t4_xor_checksum(const uint8_t *data, size_t start, size_t count) {
  uint8_t crc = 0;
  for (size_t i = start; i < start + count; i++) {
    crc ^= data[i];
  }
  return crc;
}

std::vector<uint8_t> t4_build_cmd(const T4Address &to, const T4Address &from, uint8_t control_cmd) {
  // CMD packet: control command via DEP protocol
  // Body: [to_row, to_addr, from_row, from_addr, MSG_CMD, msg_size, crc1,
  //        MENU_CONTROL, MENU_RUN, control_cmd, offset, crc2]
  std::vector<uint8_t> frame;
  frame.push_back(to.row);
  frame.push_back(to.address);
  frame.push_back(from.row);
  frame.push_back(from.address);
  frame.push_back(MSG_CMD);      // 0x01
  frame.push_back(0x05);         // message size (5 bytes after crc1)
  uint8_t crc1 = t4_xor_checksum(frame.data(), 0, 6);
  frame.push_back(crc1);
  frame.push_back(MENU_CONTROL); // 0x01
  frame.push_back(MENU_RUN);     // 0x82
  frame.push_back(control_cmd);
  frame.push_back(0x64);         // offset (0x64 works universally across drives)
  uint8_t crc2 = t4_xor_checksum(frame.data(), 7, 4);
  frame.push_back(crc2);

  // Wrap: [START_CODE, body_size, ...body..., body_size]
  uint8_t body_size = frame.size();
  frame.push_back(body_size);
  frame.insert(frame.begin(), body_size);
  frame.insert(frame.begin(), T4_START_CODE);

  return frame;
}

std::vector<uint8_t> t4_build_inf(const T4Address &to, const T4Address &from,
                                   uint8_t device_type, uint8_t register_id,
                                   uint8_t run_cmd, uint8_t next_data,
                                   const std::vector<uint8_t> &data) {
  // INF packet: info GET/SET via DMP protocol
  // Body: [to_row, to_addr, from_row, from_addr, MSG_INF, msg_size, crc1,
  //        device_type, register_id, run_cmd, next_data, data_len, data..., crc2]
  size_t data_len = data.size();

  std::vector<uint8_t> frame;
  frame.push_back(to.row);
  frame.push_back(to.address);
  frame.push_back(from.row);
  frame.push_back(from.address);
  frame.push_back(MSG_INF);                  // 0x08
  frame.push_back(0x06 + data_len);          // message size
  uint8_t crc1 = t4_xor_checksum(frame.data(), 0, 6);
  frame.push_back(crc1);
  frame.push_back(device_type);
  frame.push_back(register_id);
  frame.push_back(run_cmd);
  frame.push_back(next_data);
  frame.push_back(static_cast<uint8_t>(data_len));
  if (data_len > 0) {
    frame.insert(frame.end(), data.begin(), data.end());
  }
  // crc2 = XOR of message bytes (device_type through end of data)
  uint8_t crc2 = t4_xor_checksum(frame.data(), 7, 5 + data_len);
  frame.push_back(crc2);

  // Wrap: [START_CODE, body_size, ...body..., body_size]
  uint8_t body_size = frame.size();
  frame.push_back(body_size);
  frame.insert(frame.begin(), body_size);
  frame.insert(frame.begin(), T4_START_CODE);

  return frame;
}

const char *t4_motor_type_name(uint8_t type) {
  switch (type) {
    case MOTOR_SLIDING: return "Sliding";
    case MOTOR_SECTIONAL: return "Sectional";
    case MOTOR_SWING: return "Swing";
    case MOTOR_BARRIER: return "Barrier";
    case MOTOR_UP_AND_OVER: return "Up-and-Over";
    default: return "Unknown";
  }
}

const char *t4_auto_status_name(uint8_t status) {
  switch (status) {
    case STA_STOPPED: return "Stopped";
    case STA_OPENING: return "Opening";
    case STA_CLOSING: return "Closing";
    case STA_OPENED: return "Opened";
    case STA_CLOSED: return "Closed";
    case STA_PREFLASH: return "Pre-flashing";
    case STA_PAUSE: return "Pause";
    case STA_SEARCH_DEV: return "Searching Devices";
    case STA_SEARCH_POS: return "Searching Positions";
    case STA_PART_OPENED: return "Partially Opened";
    default: return "Unknown";
  }
}

const char *t4_control_cmd_name(uint8_t cmd) {
  switch (cmd) {
    case CMD_SBS: return "Step by Step";
    case CMD_STOP: return "Stop";
    case CMD_OPEN: return "Open";
    case CMD_CLOSE: return "Close";
    case CMD_P_OPN1: return "Partial Open 1";
    case CMD_P_OPN2: return "Partial Open 2";
    case CMD_P_OPN3: return "Partial Open 3";
    case CMD_P_OPN4: return "Partial Open 4";
    case CMD_P_OPN5: return "Partial Open 5";
    case CMD_P_OPN6: return "Partial Open 6";
    case CMD_CLS_LOCK: return "Close and Lock";
    case CMD_LOCK: return "Lock";
    case CMD_UNLOCK: return "Unlock";
    case CMD_UNLK_OPN: return "Unlock and Open";
    case CMD_UNLK_CLS: return "Unlock and Close";
    case CMD_HOST_SBS: return "Master SBS";
    case CMD_HOST_OPN: return "Master Open";
    case CMD_HOST_CLS: return "Master Close";
    case CMD_SLAVE_SBS: return "Slave SBS";
    case CMD_SLAVE_OPN: return "Slave Open";
    case CMD_SLAVE_CLS: return "Slave Close";
    case CMD_AUTO_ON: return "Auto Open On";
    case CMD_AUTO_OFF: return "Auto Open Off";
    case CMD_LIGHT_TIMER: return "Light Timer";
    case CMD_LIGHT_TOGGLE: return "Light Toggle";
    case CMD_HALT: return "Halt";
    case CMD_EMERGENCY_STOP: return "Emergency Stop";
    case CMD_EMERGENCY: return "Emergency";
    case CMD_PHOTO_OPN: return "Photo Open";
    case CMD_PHOTO: return "Photo";
    case CMD_PHOTO1: return "Photo 1";
    case CMD_PHOTO2: return "Photo 2";
    case CMD_PHOTO3: return "Photo 3";
    case CMD_EMERGENCY_OPN: return "Emergency Open";
    case CMD_EMERGENCY_CLS: return "Emergency Close";
    default: return "Unknown";
  }
}

const char *t4_stop_reason_name(uint8_t reason) {
  switch (reason) {
    case STOP_NORMAL: return "Normal";
    case STOP_OBSTACLE_ENCODER: return "Obstacle (encoder)";
    case STOP_OBSTACLE_FORCE: return "Obstacle (force)";
    case STOP_PHOTO_INTERVENTION: return "Photo intervention";
    case STOP_HALT: return "Halt";
    case STOP_EMERGENCY: return "Emergency";
    case STOP_ELECTRIC_ANOMALY: return "Electrical anomaly";
    case STOP_BLOCKED: return "Blocked";
    case STOP_TIMEOUT: return "Timeout";
    default: return "Unknown";
  }
}

const char *t4_register_name(uint8_t reg) {
  switch (reg) {
    case REG_TYPE_M: return "Motor Type";
    case REG_STATUS: return "Status";
    case REG_WHO: return "WHO";
    case REG_MAC: return "MAC";
    case REG_MANUFACTURER: return "Manufacturer";
    case REG_PRODUCT: return "Product";
    case REG_HARDWARE: return "Hardware";
    case REG_FIRMWARE: return "Firmware";
    case REG_DESCRIPTION: return "Description";
    case REG_INF_SUPPORT: return "Info Support";
    case REG_CUR_POS: return "Current Position";
    case REG_MAX_OPN: return "Max Open";
    case REG_POS_MAX: return "Position Max";
    case REG_POS_MIN: return "Position Min";
    case REG_P_OPN1: return "Partial Open 1";
    case REG_P_OPN2: return "Partial Open 2";
    case REG_P_OPN3: return "Partial Open 3";
    case REG_SLOW_OPN: return "Slow Open";
    case REG_SLOW_CLS: return "Slow Close";
    case REG_OPN_OFFSET: return "Open Offset";
    case REG_CLS_OFFSET: return "Close Offset";
    case REG_OPN_DISCHARGE: return "Open Discharge";
    case REG_CLS_DISCHARGE: return "Close Discharge";
    case REG_RESET_ENCODER: return "Reset Encoder";
    case REG_REV_TIME: return "Reverse Time";
    case REG_SPEED_OPN: return "Speed Open";
    case REG_SPEED_CLS: return "Speed Close";
    case REG_SPEED_DECEL: return "Speed Decel";
    // REG_SPEED_DECEL_OPN/CLS share values with REG_SPEED_SLW_OPN/CLS (0x45/0x46)
    case REG_FORCE_CRUISE: return "Force Cruise";
    case REG_FORCE_OPN: return "Force Open";
    case REG_FORCE_CLS: return "Force Close";
    case REG_OUT1: return "Output 1";
    case REG_OUT2: return "Output 2";
    case REG_LOCK_TIME: return "Lock Time";
    case REG_LAMP_TIME: return "Lamp Time";
    case REG_SUCTION_TIME: return "Suction Time";
    case REG_CMD_SBS: return "Cmd SBS";
    case REG_CMD_POPN: return "Cmd Partial Open";
    case REG_CMD_OPN: return "Cmd Open";
    case REG_CMD_CLS: return "Cmd Close";
    case REG_CMD_STP: return "Cmd Stop";
    case REG_CMD_PHOTO: return "Cmd Photo";
    case REG_CMD_PHOTO2: return "Cmd Photo 2";
    case REG_CMD_PHOTO3: return "Cmd Photo 3";
    case REG_CMD_OPN_STP: return "Cmd Open Stop";
    case REG_CMD_CLS_STP: return "Cmd Close Stop";
    case REG_IN1: return "Input 1";
    case REG_IN2: return "Input 2";
    case REG_IN3: return "Input 3";
    case REG_IN4: return "Input 4";
    case REG_CMD_LET_OPN: return "Cmd Let Open";
    case REG_CMD_LET_CLS: return "Cmd Let Close";
    case REG_AUTOCLS: return "Auto Close";
    case REG_PAUSE_TIME: return "Pause Time";
    case REG_PH_CLS_ON: return "Photo Close";
    case REG_PH_CLS_TIME: return "Photo Close Time";
    case REG_PH_CLS_MODE: return "Photo Close Mode";
    case REG_ALW_CLS_ON: return "Always Close";
    case REG_ALW_CLS_TIME: return "Always Close Time";
    case REG_ALW_CLS_MODE: return "Always Close Mode";
    case REG_STANDBY_ACT: return "Standby";
    case REG_STANDBY_TIME: return "Standby Time";
    case REG_STANDBY_MODE: return "Standby Mode";
    case REG_START_ON: return "Starting Torque";
    case REG_START_TIME: return "Starting Torque Time";
    case REG_WATER_HAMMER: return "Water Hammer";
    case REG_PEAK_ON: return "Peak Mode";
    case REG_BLINK_ON: return "Pre-Flash";
    case REG_BLINK_OPN_TIME: return "Pre-Flash Open Time";
    case REG_BLINK_CLS_TIME: return "Pre-Flash Close Time";
    case REG_OP_BLOCK: return "Operator Block";
    case REG_RADIO_INH: return "Radio Inhibit";
    case REG_KEY_LOCK: return "Key Lock";
    case REG_SLOW_ON: return "Slow Mode";
    case REG_DIS_VAL: return "Display Value";
    case REG_MAINT_THRESHOLD: return "Maintenance Threshold";
    case REG_MAINT_COUNT: return "Maintenance Count";
    case REG_TOTAL_COUNT: return "Total Count";
    case REG_MAINT_CANCEL: return "Maintenance Cancel";
    case REG_NUM_MOVEMENTS: return "Number of Movements";
    case REG_DIAG_BB: return "Diagnostics Blackbox";
    case REG_DIAG_IO: return "Diagnostics I/O";
    case REG_DIAG_PAR: return "Diagnostics Parameters";
    default: return "Unknown";
  }
}

}  // namespace nice_bidiwifi
}  // namespace esphome
