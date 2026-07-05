#include "CanFrames.h"
#include <Arduino.h>
#include <FlexCAN_T4.h>
#include <SD.h>
#include <TimeLib.h>
#include <USBHost_t36.h>

// Define SD card CS pin
const int sdChipSelect = BUILTIN_SDCARD;

// ================= BMS DATA =================
volatile float bmsSeg[5] = {0};
volatile float bmsTotal = 0;
volatile float bmsPackAvgTemp = 0;
volatile float bmsCurrent = 0;
volatile float bmsLowestCellV = 0;
volatile uint8_t bmsLowestCellIC = 0;
volatile uint8_t bmsLowestCellIndex = 0;
volatile uint8_t bmsLatchState = 0;
volatile uint8_t bmsFaultFlags = 0;

// Motor and VSM data
volatile float motorSpeed = 0;
volatile float wheelRPM = 0;
volatile float vehicle_kmh = 0;
volatile float segmentVoltage0 = 0;
volatile float segmentVoltage1 = 0;
volatile float segmentVoltage2 = 0;
volatile float segmentVoltage3 = 0;
volatile float segmentVoltage4 = 0;
volatile uint8_t vsmState = 0;

// ================= BMS CAN IDs =================
#define CAN_ID_SEGMENT_VOLTAGES_1 0x18FF0001UL
#define CAN_ID_SEGMENT_VOLTAGES_2 0x18FF0002UL
#define CAN_ID_LATCH_STATUS 0x18FF0003UL
#define CAN_ID_TEMPERATURE_SUMMARY 0x18FF0004UL
#define CAN_ID_LOWEST_CELL 0x18FF0005UL
#define CAN_ID_PACK_CURRENT 0x18FF0006UL
#define LED_GREEN 2016
#define LED_YELLOW 65504
#define LED_RED 63488
#define LED_OFF 50712

// ================= COG TRANSFORM CONSTANTS =================
// All positions in metres, relative to vehicle origin
// VectorNav position
static const float VN_X = 0.1003f;
static const float VN_Y = 0.0065f;
static const float VN_Z = 0.0480f;
// Centre of Gravity position
static const float COG_X = 0.8262f;
static const float COG_Y = 0.0000f;
static const float COG_Z = 0.2700f;

// Lever arm: CoG → IMU  (r = VN_pos - CoG_pos)
static const float R_X = VN_X - COG_X; // -0.7259 m
static const float R_Y = VN_Y - COG_Y; //  0.0065 m
static const float R_Z = VN_Z - COG_Z; // -0.2220 m

// Gravity in body frame  (VN-200 Z-up convention: at rest Accel_Z ≈ -9.81)
static const float G_X = 0.0f;
static const float G_Y = 0.0f;
static const float G_Z = -9.81f;

// Sanity limit on raw accelerometer to detect corrupt CAN / VN frames
static const float ACCEL_SANITY_LIMIT = 1000.0f; // m/s²

// FlexCAN & VN parser
FlexCAN_T4<CAN2, RX_SIZE_256, TX_SIZE_16> can2;
CanFrames::Parser parser;

// USB Host for VN-200
USBHost myusb;
USBSerial vn200Serial(myusb);

File dataFile;

// ======================= DATA STRUCTS =======================
struct __attribute__((packed)) CANData {
  uint32_t timestamp;

  float moduleATemp, moduleBTemp, moduleCTemp, gateDriverTemp;
  uint32_t temperatures1Timestamp;

  float controlBoardTemp, rtd1Temp, rtd2Temp, stallBurstTemp;
  uint32_t temperatures2Timestamp;

  float motorAngle, motorSpeed, electricalFrequency, deltaResolverFiltered;
  uint32_t motorPositionTimestamp;

  float phaseACurrent, phaseBCurrent, phaseCCurrent, dcBusCurrent;
  uint32_t currentInfoTimestamp;

  float dcBusVoltage, outputVoltage, vAB_VdVoltage, vBC_VqVoltage;
  uint32_t voltageInfoTimestamp;

  float commandedTorque, torqueFeedback, powerOnTimer;
  uint32_t torqueTimerTimestamp;

  bool isComplete;
};

struct __attribute__((packed)) VN200Data {
  uint32_t timestamp;
  float yaw, pitch, roll;
  float pos_ex, pos_ey, pos_ez;
  float vel_ex, vel_ey, vel_ez;
  float accel_x, accel_y, accel_z;
  float gyro_x, gyro_y, gyro_z;
  bool hasData;
};

// CoG-transformed output (computed once per VN sample)
struct CoGData {
  float vx, vy, vz, v_mag; // body-frame velocity at CoG  (m/s)
  float ax, ay, az, a_mag; // body-frame kinematic accel at CoG (m/s²)
};

CANData currentData;
VN200Data vn200Data;
CoGData cogData;
volatile bool newDataAvailable = false;

// Previous gyro reading for angular-acceleration finite difference
static float prevGyroX = 0.0f;
static float prevGyroY = 0.0f;
static float prevGyroZ = 0.0f;
static uint32_t prevVNTimestamp = 0;

String baseFileName = "can_data";
String fileName;
int fileIndex = 0;

// ======================= DEBUG PRINT =======================
void printSnapshot(const CanFrames::Snapshot &s) {
  Serial.printf("T1:%u | A:%.1f B:%.1f C:%.1f Gate:%.1f | ",
                s.temperatures1Timestamp, s.moduleATemp, s.moduleBTemp,
                s.moduleCTemp, s.gateDriverTemp);
  Serial.printf("Ctrl:%.1f RTD1:%.1f RTD2:%.1f RTD3:%.1f | ",
                s.controlBoardTemp, s.rtd1Temp, s.rtd2Temp,
                s.rtd3StallBurstTemp);
  Serial.printf("MotorAngle:%.1f Speed:%.1f TorqueCmd:%.1f TorqueFB:%.1f\n",
                s.motorAngle, s.motorSpeed, s.commandedTorque,
                s.torqueFeedback);
}

// ======================= MATH HELPERS =======================
// 3-component cross product:  out = a × b
static inline void cross3(float ax, float ay, float az, float bx, float by,
                          float bz, float &ox, float &oy, float &oz) {
  ox = ay * bz - az * by;
  oy = az * bx - ax * bz;
  oz = ax * by - ay * bx;
}

// Clamp a float to [-limit, +limit]
static inline float clampf(float v, float limit) {
  if (v > limit)
    return limit;
  if (v < -limit)
    return -limit;
  return v;
}

// ======================= COG TRANSFORM =======================
/*
 * Rotation matrix (ECEF → Body, ZYX convention):
 *   R = Rz(yaw) @ Ry(pitch) @ Rx(roll)
 *   (transpose of body→ECEF, i.e. passive rotation)
 *
 *   Ry sign was previously wrong in original Python — corrected here.
 *
 * Velocity:
 *   v_CoG_body = R * v_ECEF  -  ω × r
 *
 * Acceleration (kinematic, gravity removed):
 *   a_CoG_body = f_IMU  -  g_body  -  α × r  -  ω × (ω × r)
 *
 *   where f_IMU is the VN specific-force output (body frame),
 *   g_body = [0, 0, -9.81] for VN Z-up convention.
 */
void computeCoG(const VN200Data &vn, CoGData &out) {
  // ── Guard: reject corrupt frames ──────────────────────────────────────
  if (fabsf(vn.accel_x) > ACCEL_SANITY_LIMIT ||
      fabsf(vn.accel_y) > ACCEL_SANITY_LIMIT ||
      fabsf(vn.accel_z) > ACCEL_SANITY_LIMIT) {
    // Keep last good CoG values; don't update
    return;
  }

  // ── Euler angles → radians ────────────────────────────────────────────
  float psi = vn.yaw * (float)DEG_TO_RAD;     // yaw
  float theta = vn.pitch * (float)DEG_TO_RAD; // pitch
  float phi = vn.roll * (float)DEG_TO_RAD;    // roll

  float cPsi = cosf(psi), sPsi = sinf(psi);
  float cTheta = cosf(theta), sTheta = sinf(theta);
  float cPhi = cosf(phi), sPhi = sinf(phi);

  // ── Rotation matrix R = Rz @ Ry @ Rx  (ECEF → Body) ─────────────────
  // Row-major storage  R[row][col]
  // Rz:  [ cPsi   sPsi  0 ]      Ry:  [ cTheta  0  sTheta ]
  //      [-sPsi   cPsi  0 ]           [  0      1  0      ]
  //      [  0      0    1 ]           [-sTheta  0  cTheta  ]
  //
  // Rx:  [ 1   0     0   ]
  //      [ 0  cPhi  sPhi ]
  //      [ 0 -sPhi  cPhi ]
  //
  // Combined (R = Rz @ Ry @ Rx):
  float R00 = cPsi * cTheta;
  float R01 = sPsi * cTheta;
  float R02 = -sTheta;

  float R10 = cPsi * sTheta * sPhi - sPsi * cPhi;
  float R11 = sPsi * sTheta * sPhi + cPsi * cPhi;
  float R12 = cTheta * sPhi;

  float R20 = cPsi * sTheta * cPhi + sPsi * sPhi;
  float R21 = sPsi * sTheta * cPhi - cPsi * sPhi;
  float R22 = cTheta * cPhi;

  // ── ECEF velocity → body frame ────────────────────────────────────────
  float vEx = vn.vel_ex, vEy = vn.vel_ey, vEz = vn.vel_ez;

  float vBx = R00 * vEx + R01 * vEy + R02 * vEz;
  float vBy = R10 * vEx + R11 * vEy + R12 * vEz;
  float vBz = R20 * vEx + R21 * vEy + R22 * vEz;

  // ── Angular velocity (already in body frame from VN) ──────────────────
  float wx = vn.gyro_x, wy = vn.gyro_y, wz = vn.gyro_z;

  // ── ω × r ─────────────────────────────────────────────────────────────
  float wxrX, wxrY, wxrZ;
  cross3(wx, wy, wz, R_X, R_Y, R_Z, wxrX, wxrY, wxrZ);

  // ── v_CoG = v_IMU_body − ω × r ───────────────────────────────────────
  out.vx = vBx - wxrX;
  out.vy = vBy - wxrY;
  out.vz = vBz - wxrZ;
  out.v_mag = sqrtf(out.vx * out.vx + out.vy * out.vy + out.vz * out.vz);

  // ── Angular acceleration α = dω/dt (finite difference) ───────────────
  float dt = 0.0f;
  if (prevVNTimestamp != 0 && vn.timestamp > prevVNTimestamp) {
    dt = (vn.timestamp - prevVNTimestamp) / 1000.0f; // ms → s
    if (dt > 1.0f)
      dt = 0.0f; // reject gaps > 1 s (startup / dropout)
  }

  float alphaX = 0.0f, alphaY = 0.0f, alphaZ = 0.0f;
  if (dt > 0.0f) {
    alphaX = clampf((wx - prevGyroX) / dt, 100.0f);
    alphaY = clampf((wy - prevGyroY) / dt, 100.0f);
    alphaZ = clampf((wz - prevGyroZ) / dt, 100.0f);
  }

  // Save state for next call
  prevGyroX = wx;
  prevGyroY = wy;
  prevGyroZ = wz;
  prevVNTimestamp = vn.timestamp;

  // ── α × r ─────────────────────────────────────────────────────────────
  float axrX, axrY, axrZ;
  cross3(alphaX, alphaY, alphaZ, R_X, R_Y, R_Z, axrX, axrY, axrZ);

  // ── ω × (ω × r) ──────────────────────────────────────────────────────
  float wxwxrX, wxwxrY, wxwxrZ;
  cross3(wx, wy, wz, wxrX, wxrY, wxrZ, wxwxrX, wxwxrY, wxwxrZ);

  // ── a_CoG = f_IMU − g − α×r − ω×(ω×r) ───────────────────────────────
  // Subtracting g_body removes gravity from specific force → kinematic accel
  out.ax = vn.accel_x - G_X - axrX - wxwxrX;
  out.ay = vn.accel_y - G_Y - axrY - wxwxrY;
  out.az = vn.accel_z - G_Z - axrZ - wxwxrZ;
  out.a_mag = sqrtf(out.ax * out.ax + out.ay * out.ay + out.az * out.az);
}

// ======================= SETUP =======================
void setup() {
  Serial.begin(115200);
  Serial7.begin(460800);
  Serial2.begin(115200);
  pinMode(13, OUTPUT);

  can2.begin();
  can2.setBaudRate(250000);
  can2.setMaxMB(16);
  can2.enableFIFO();
  can2.enableFIFOInterrupt();
  can2.onReceive(canSniff);

  Serial.println("CAN Initialized - Listening for CAN messages");

  myusb.begin();
  Serial.println("Waiting for VN-200 connection...");

  resetVN200Data();
  memset(&cogData, 0, sizeof(CoGData));

  delay(100);

  if (!SD.begin(sdChipSelect)) {
    Serial.println("SD card initialization failed!");
    return;
  }

  findNextFileName();

  dataFile = SD.open(fileName.c_str(), FILE_WRITE);
  if (!dataFile) {
    Serial.println("Error opening " + fileName + "!");
    return;
  }

  writeHeader();
  resetCurrentData();
}

void updateVSMIndicators(uint16_t state) {
  Serial2.print("page0.t0.bco=");
  Serial2.print(state >= 2 ? LED_GREEN : LED_RED);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);

  Serial2.print("page0.t1.bco=");
  Serial2.print(state >= 4 ? LED_GREEN : LED_RED);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);

  Serial2.print("page0.t2.bco=");
  Serial2.print(state >= 6 ? LED_GREEN : LED_RED);
  Serial2.write(0xff);
  Serial2.write(0xff);
  Serial2.write(0xff);
}

// ======================= LOOP =======================
void sendTestPacket() {
  struct __attribute__((packed)) TestPayload {
    uint8_t start;      // 0
    uint32_t timestamp; // 1
    // BMS
    float bmsTotal;       // 5
    float bmsPackAvgTemp; // 9
    float bmsCurrent;     // 13
    float bmsLowestCellV; // 17
    // Motor
    float motorSpeedVal; // 21
    // Segment voltages 0-4
    float segVolt0; // 25
    float segVolt1; // 29
    float segVolt2; // 33
    float segVolt3; // 37
    float segVolt4; // 41
    // VSM state
    uint8_t vsmStateVal; // 45
    // Position from VectorNav
    float posX; // 46
    float posY; // 50
    float posZ; // 54
    // Wheel & vehicle speed
    float wheelSpeedVal;   // 58
    float vehicleSpeedVal; // 62
    // Orientation
    float yaw;   // 66
    float pitch; // 70
    float roll;  // 74
    uint8_t end; // 78  -> total 79 bytes
  };

  // Print sizeof once for debugging
  static bool printed = false;
  if (!printed) {
    Serial.printf("TestPayload sizeof = %u bytes\n",
                  (unsigned)sizeof(TestPayload));
    printed = true;
  }

  TestPayload pkt;
  pkt.start = 0xAA;
  pkt.timestamp = millis();
  pkt.bmsTotal = bmsTotal;
  pkt.bmsPackAvgTemp = bmsPackAvgTemp;
  pkt.bmsCurrent = bmsCurrent;
  pkt.bmsLowestCellV = bmsLowestCellV;

  pkt.motorSpeedVal = motorSpeed;
  pkt.segVolt0 = segmentVoltage0;
  pkt.segVolt1 = segmentVoltage1;
  pkt.segVolt2 = segmentVoltage2;
  pkt.segVolt3 = segmentVoltage3;
  pkt.segVolt4 = segmentVoltage4;
  pkt.vsmStateVal = vsmState;

  pkt.posX = vn200Data.pos_ex;
  pkt.posY = vn200Data.pos_ey;
  pkt.posZ = vn200Data.pos_ez;

  pkt.wheelSpeedVal = wheelRPM;
  pkt.vehicleSpeedVal = vehicle_kmh;

  pkt.yaw = vn200Data.yaw;
  pkt.pitch = vn200Data.pitch;
  pkt.roll = vn200Data.roll;

  pkt.end = 0x55;

  Serial7.write((uint8_t *)&pkt, sizeof(pkt));
}

// ======================= NEXTION RPM BAR =======================
void updateRPMBar(int rpm) {
  int thresholds[9] = {1000, 1500, 2000, 2500, 3000, 3500, 4000, 4500, 5000};
  int colors[9] = {
      2016,  2016,  2016,  // Green
      65504, 65504, 65504, // Yellow
      63488, 63488, 63488  // Red
  };

  // F1 shift light effect: If RPM is at or above redline, make all active LEDs
  // solid red
  bool redline = (rpm >= 5000);

  for (int i = 0; i < 9; i++) {
    Serial2.print("page1.led");
    Serial2.print(i);
    Serial2.print(".bco=");

    if (rpm >= thresholds[i]) {
      if (redline) {
        Serial2.print(63488); // Solid Red
      } else {
        Serial2.print(colors[i]); // Progressive Color
      }
    } else {
      Serial2.print(10565); // Dark Grey (off state)
    }
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);
  }
}

// ======================= LOOP =======================
void loop() {
  can2.events();
  myusb.Task();

  if (vn200Serial.available()) {
    String data = vn200Serial.readStringUntil('\n');
    data.trim();
    processVN200Line(data);
  }

  if (newDataAvailable && currentData.isComplete) {
    writeDataToSD();
    resetCurrentData();
    newDataAvailable = false;
    delayMicroseconds(100);
  }

  static unsigned long lastBlink = 0;
  if (millis() - lastBlink > 1000) {
    digitalWrite(13, !digitalRead(13));
    lastBlink = millis();
    delay(1);
  }

  static unsigned long lastTest = 0;
  if (millis() - lastTest >= 100) {
    sendTestPacket();
    lastTest = millis();
  }

  static unsigned long lastDisplayUpdate = 0;

  if (millis() - lastDisplayUpdate >= 100) { // 10Hz refresh
    lastDisplayUpdate = millis();

    // ---- All Serial2 prints go here ----

    Serial2.print("page1.t10.txt=\"");
    Serial2.print((float)(bmsTotal));
    Serial2.print("\""); // integer for .val
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    // Pack Average Temp
    Serial2.print("page1.n2.val=");
    Serial2.print((int)(bmsPackAvgTemp));
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    /*Serial2.print("page1.n3.val=");
    Serial2.print(vsmState);
    Serial2.write(0xff); Serial2.write(0xff); Serial2.write(0xff);*/

    // Lowest Cell Voltage
    Serial2.print("page1.t11.txt=\"");
    Serial2.print(bmsLowestCellV, 3); // 3 decimal places
    Serial2.print("\"");
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    Serial2.print("page1.n1.val=");
    Serial2.print((int)vehicle_kmh);
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    // Update F1 Style RPM Bar
    updateRPMBar(abs(motorSpeed));

    Serial2.print("page3.n1.val=");
    Serial2.print((int)currentData.dcBusVoltage);
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    Serial2.print("page3.n2.val=");
    Serial2.print((int)currentData.dcBusCurrent);
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    Serial2.print("page4.t0.txt=\"");
    Serial2.print(bmsSeg[0], 2); // 2 decimal places
    Serial2.print("\"");
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    Serial2.print("page4.t1.txt=\"");
    Serial2.print(bmsSeg[1], 2);
    Serial2.print("\"");
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    Serial2.print("page4.t7.txt=\"");
    Serial2.print(bmsSeg[2], 2);
    Serial2.print("\"");
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    Serial2.print("page4.t8.txt=\"");
    Serial2.print(bmsSeg[3], 2);
    Serial2.print("\"");
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    Serial2.print("page4.t9.txt=\"");
    Serial2.print(bmsSeg[4], 2);
    Serial2.print("\"");
    Serial2.write(0xff);
    Serial2.write(0xff);
    Serial2.write(0xff);

    updateVSMIndicators(vsmState);
  }
}

// ======================= BMS HELPERS =======================
static inline uint16_t read_u16_be(const uint8_t *b) {
  return (uint16_t(b[0]) << 8) | uint16_t(b[1]);
}
static inline int16_t read_i16_be(const uint8_t *b) {
  return (int16_t)((b[0] << 8) | b[1]);
}

// ======================= CAN CALLBACK =======================
void canSniff(const CAN_message_t &msg) {

  if (msg.flags.extended) {
    switch (msg.id) {
    case CAN_ID_SEGMENT_VOLTAGES_1:
      decode_segment_voltages_1(msg);
      break;
    case CAN_ID_SEGMENT_VOLTAGES_2:
      decode_segment_voltages_2(msg);
      break;
    case CAN_ID_PACK_CURRENT:
      decode_pack_current(msg);
      break;
    case CAN_ID_LATCH_STATUS:
      decode_latch_status(msg);
      break;
    case CAN_ID_TEMPERATURE_SUMMARY:
      decode_temperature_summary(msg);
      break;
    default:
      break;
    }
  }

  if (msg.id == 0x0A0 || msg.id == 0x0A1 || msg.id == 0x0A5 ||
      msg.id == 0x0A6 || msg.id == 0x0A7 || msg.id == 0x0AC) {
    parser.feed(msg.id, msg.buf);
    if (parser.hasComplete()) {
      CanFrames::Snapshot snap;
      parser.read(snap);
      printSnapshot(snap);
      parser.clearWindow();
    }
  }

  processCANMessage(msg);
}

// ======================= MESSAGE ROUTER =======================
void processCANMessage(const CAN_message_t &msg) {
  switch (msg.id) {
  case 0x0A0:
    parseTemperatures1(msg);
    break;
  case 0x0A1:
    parseTemperatures2(msg);
    break;
  case 0x0A5:
    parseMotorPosition(msg);
    break;
  case 0x0A6:
    parseCurrentInformation(msg);
    break;
  case 0x0A7:
    parseVoltageInformation(msg);
    break;
  case 0x0AA:
    parseInternalStates(msg);
    break;
  case 0x0AC:
    parseTorqueTimerInformation(msg);
    break;
  default:
    return;
  }
  newDataAvailable = true;
  checkDataCompleteness();
}

// ======================= FILE FUNCTIONS =======================
void writeHeader() {
  dataFile.println(
      "Timestamp,"
      "T1_Timestamp,Module_A_Temp,Module_B_Temp,Module_C_Temp,Gate_Driver_Temp,"
      "T2_Timestamp,Control_Board_Temp,RTD1_Temp,RTD2_Temp,Stall_Burst_Temp,"
      "MotPos_Timestamp,Motor_Angle,Motor_Speed,Elec_Frequency,Delta_Resolver,"
      "Cur_Timestamp,Phase_A_Current,Phase_B_Current,Phase_C_Current,DC_Bus_"
      "Current,"
      "Vol_Timestamp,DC_Bus_Voltage,Output_Voltage,VAB_Vd_Voltage,VBC_Vq_"
      "Voltage,"
      "Torq_Timestamp,Commanded_Torque,Torque_Feedback,Power_On_Timer,"
      "VN_Timestamp,Yaw,Pitch,Roll,Pos_Ex,Pos_Ey,Pos_Ez,"
      "Vel_Ex,Vel_Ey,Vel_Ez,Accel_X,Accel_Y,Accel_Z,Gyro_X,Gyro_Y,Gyro_Z,"
      "vx_cog,vy_cog,vz_cog,v_mag_cog,"
      "ax_cog,ay_cog,az_cog,a_mag_cog,"
      "BMS_Seg0,BMS_Seg1,BMS_Seg2,BMS_Seg3,BMS_Seg4,"
      "BMS_TotalV,BMS_Current,BMS_LowestCellV,BMS_LowestIC,BMS_LowestIndex,"
      "BMS_PackAvgTemp,BMS_Latch,BMS_FaultFlags");
  dataFile.flush();
}

void writeDataToSD() {
  dataFile.printf(
      // Teensy timestamp
      "%u,"
      // Inverter temperatures 1
      "%u,%.1f,%.1f,%.1f,%.1f,"
      // Inverter temperatures 2
      "%u,%.1f,%.1f,%.1f,%.1f,"
      // Motor position
      "%u,%.1f,%.1f,%.1f,%.1f,"
      // Currents
      "%u,%.1f,%.1f,%.1f,%.1f,"
      // Voltages
      "%u,%.1f,%.1f,%.1f,%.1f,"
      // Torque / timer
      "%u,%.1f,%.1f,%.3f,"
      // VN-200 raw
      "%u,%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,"
      "%.2f,%.2f,%.2f,%.2f,%.2f,%.2f,%.4f,%.4f,%.4f,"
      // CoG velocity
      "%.4f,%.4f,%.4f,%.4f,"
      // CoG acceleration
      "%.4f,%.4f,%.4f,%.4f,"
      // BMS
      "%.2f,%.2f,%.2f,%.2f,%.2f,"
      "%.2f,%.2f,%.4f,%u,%u,"
      "%.2f,%u,%u\n",

      millis(),

      currentData.temperatures1Timestamp, currentData.moduleATemp,
      currentData.moduleBTemp, currentData.moduleCTemp,
      currentData.gateDriverTemp,

      currentData.temperatures2Timestamp, currentData.controlBoardTemp,
      currentData.rtd1Temp, currentData.rtd2Temp, currentData.stallBurstTemp,

      currentData.motorPositionTimestamp, currentData.motorAngle,
      currentData.motorSpeed, currentData.electricalFrequency,
      currentData.deltaResolverFiltered,

      currentData.currentInfoTimestamp, currentData.phaseACurrent,
      currentData.phaseBCurrent, currentData.phaseCCurrent,
      currentData.dcBusCurrent,

      currentData.voltageInfoTimestamp, currentData.dcBusVoltage,
      currentData.outputVoltage, currentData.vAB_VdVoltage,
      currentData.vBC_VqVoltage,

      currentData.torqueTimerTimestamp, currentData.commandedTorque,
      currentData.torqueFeedback, currentData.powerOnTimer,

      vn200Data.timestamp, vn200Data.yaw, vn200Data.pitch, vn200Data.roll,
      vn200Data.pos_ex, vn200Data.pos_ey, vn200Data.pos_ez, vn200Data.vel_ex,
      vn200Data.vel_ey, vn200Data.vel_ez, vn200Data.accel_x, vn200Data.accel_y,
      vn200Data.accel_z, vn200Data.gyro_x, vn200Data.gyro_y, vn200Data.gyro_z,

      cogData.vx, cogData.vy, cogData.vz, cogData.v_mag, cogData.ax, cogData.ay,
      cogData.az, cogData.a_mag,

      bmsSeg[0], bmsSeg[1], bmsSeg[2], bmsSeg[3], bmsSeg[4], bmsTotal,
      bmsCurrent, bmsLowestCellV, (unsigned)bmsLowestCellIC,
      (unsigned)bmsLowestCellIndex, bmsPackAvgTemp, (unsigned)bmsLatchState,
      (unsigned)bmsFaultFlags);

  dataFile.flush();
}

// ======================= HELPERS =======================
void resetCurrentData() {
  memset(&currentData, 0, sizeof(CANData));
  currentData.isComplete = false;
}

void checkDataCompleteness() {
  currentData.isComplete = currentData.temperatures1Timestamp != 0 &&
                           currentData.temperatures2Timestamp != 0 &&
                           currentData.motorPositionTimestamp != 0 &&
                           currentData.currentInfoTimestamp != 0 &&
                           currentData.voltageInfoTimestamp != 0 &&
                           currentData.torqueTimerTimestamp != 0;
}

void findNextFileName() {
  do {
    fileName = baseFileName + String(fileIndex) + ".csv";
    fileIndex++;
  } while (SD.exists(fileName.c_str()));

  Serial.print("Logging to file: ");
  Serial.println(fileName);
}

// ======================= PARSE FUNCTIONS =======================
void parseTemperatures1(const CAN_message_t &msg) {
  currentData.temperatures1Timestamp = millis();
  currentData.moduleATemp = (int16_t)((msg.buf[1] << 8) | msg.buf[0]) / 10.0f;
  currentData.moduleBTemp = (int16_t)((msg.buf[3] << 8) | msg.buf[2]) / 10.0f;
  currentData.moduleCTemp = (int16_t)((msg.buf[5] << 8) | msg.buf[4]) / 10.0f;
  currentData.gateDriverTemp =
      (int16_t)((msg.buf[7] << 8) | msg.buf[6]) / 10.0f;
}

void parseTemperatures2(const CAN_message_t &msg) {
  currentData.temperatures2Timestamp = millis();
  currentData.controlBoardTemp =
      (int16_t)((msg.buf[1] << 8) | msg.buf[0]) / 10.0f;
  currentData.rtd1Temp = (int16_t)((msg.buf[3] << 8) | msg.buf[2]) / 10.0f;
  currentData.rtd2Temp = (int16_t)((msg.buf[5] << 8) | msg.buf[4]) / 10.0f;
  currentData.stallBurstTemp =
      (int16_t)((msg.buf[7] << 8) | msg.buf[6]) / 10.0f;
}

void parseMotorPosition(const CAN_message_t &msg) {
  currentData.motorPositionTimestamp = millis();
  currentData.motorAngle = (int16_t)((msg.buf[1] << 8) | msg.buf[0]) / 10.0f;
  currentData.motorSpeed = abs((int16_t)((msg.buf[3] << 8) | msg.buf[2]));
  currentData.electricalFrequency =
      (int16_t)((msg.buf[5] << 8) | msg.buf[4]) / 10.0f;
  currentData.deltaResolverFiltered =
      (int16_t)((msg.buf[7] << 8) | msg.buf[6]) / 10.0f;

  // Update motor speed for telemetry
  motorSpeed = currentData.motorSpeed;

  // Compute wheel RPM and vehicle speed
  const float WHEEL_CIRC_M = 1.435f;
  wheelRPM = (motorSpeed / 5.818f);
  vehicle_kmh = (wheelRPM * WHEEL_CIRC_M * 60.0f / 1000.0f);
}

void parseCurrentInformation(const CAN_message_t &msg) {
  currentData.currentInfoTimestamp = millis();
  currentData.phaseACurrent = (int16_t)((msg.buf[1] << 8) | msg.buf[0]) / 10.0f;
  currentData.phaseBCurrent = (int16_t)((msg.buf[3] << 8) | msg.buf[2]) / 10.0f;
  currentData.phaseCCurrent = (int16_t)((msg.buf[5] << 8) | msg.buf[4]) / 10.0f;
  currentData.dcBusCurrent = (int16_t)((msg.buf[7] << 8) | msg.buf[6]) / 10.0f;
}

void parseVoltageInformation(const CAN_message_t &msg) {
  currentData.voltageInfoTimestamp = millis();
  currentData.dcBusVoltage = (int16_t)((msg.buf[1] << 8) | msg.buf[0]) / 10.0f;
  currentData.outputVoltage = (int16_t)((msg.buf[3] << 8) | msg.buf[2]) / 10.0f;
  currentData.vAB_VdVoltage = (int16_t)((msg.buf[5] << 8) | msg.buf[4]) / 10.0f;
  currentData.vBC_VqVoltage = (int16_t)((msg.buf[7] << 8) | msg.buf[6]) / 10.0f;
}

void parseTorqueTimerInformation(const CAN_message_t &msg) {
  currentData.torqueTimerTimestamp = millis();
  currentData.commandedTorque =
      (int16_t)((msg.buf[1] << 8) | msg.buf[0]) / 10.0f;
  currentData.torqueFeedback =
      (int16_t)((msg.buf[3] << 8) | msg.buf[2]) / 10.0f;
  currentData.powerOnTimer = msg.buf[4];
}

void parseInternalStates(const CAN_message_t &msg) {
  // Extract VSM state from byte 0
  vsmState = msg.buf[0];
}

void resetVN200Data() {
  memset(&vn200Data, 0, sizeof(VN200Data));
  vn200Data.hasData = false;
}

void processVN200Line(String line) {
  if (!line.startsWith("$VNISE")) {
    Serial.print("ERROR: Not receiving $VNISE. Got: ");
    Serial.println(line);
    return;
  }

  // Collect comma positions
  int parts[16];
  int partCount = 0;
  for (int i = 0; i < (int)line.length() && partCount < 16; i++)
    if (line.charAt(i) == ',')
      parts[partCount++] = i;

  if (partCount < 15) {
    Serial.println("VN Parse error: not enough fields");
    return;
  }

  int starPos = line.indexOf('*');

  vn200Data.timestamp = millis();
  vn200Data.yaw = line.substring(parts[0] + 1, parts[1]).toFloat();
  vn200Data.pitch = line.substring(parts[1] + 1, parts[2]).toFloat();
  vn200Data.roll = line.substring(parts[2] + 1, parts[3]).toFloat();
  vn200Data.pos_ex = line.substring(parts[3] + 1, parts[4]).toFloat();
  vn200Data.pos_ey = line.substring(parts[4] + 1, parts[5]).toFloat();
  vn200Data.pos_ez = line.substring(parts[5] + 1, parts[6]).toFloat();
  vn200Data.vel_ex = line.substring(parts[6] + 1, parts[7]).toFloat();
  vn200Data.vel_ey = line.substring(parts[7] + 1, parts[8]).toFloat();
  vn200Data.vel_ez = line.substring(parts[8] + 1, parts[9]).toFloat();
  vn200Data.accel_x = line.substring(parts[9] + 1, parts[10]).toFloat();
  vn200Data.accel_y = line.substring(parts[10] + 1, parts[11]).toFloat();
  vn200Data.accel_z = line.substring(parts[11] + 1, parts[12]).toFloat();
  vn200Data.gyro_x = line.substring(parts[12] + 1, parts[13]).toFloat();
  vn200Data.gyro_y = line.substring(parts[13] + 1, parts[14]).toFloat();

  String lastField = line.substring(parts[14] + 1);
  if (starPos != -1)
    lastField = lastField.substring(0, lastField.indexOf('*'));
  vn200Data.gyro_z = lastField.toFloat();
  vn200Data.hasData = true;

  // ── Compute CoG transform immediately on each fresh VN sample ─────────
  computeCoG(vn200Data, cogData);
}

// ======================= BMS DECODERS =======================
void decode_segment_voltages_1(const CAN_message_t &msg) {
  if (msg.len < 6)
    return;
  bmsSeg[0] = read_u16_be(&msg.buf[0]) / 100.0f;
  bmsSeg[1] = read_u16_be(&msg.buf[2]) / 100.0f;
  bmsSeg[2] = read_u16_be(&msg.buf[4]) / 100.0f;

  // Update individual segment voltage variables for telemetry
  segmentVoltage0 = bmsSeg[0];
  segmentVoltage1 = bmsSeg[1];
  segmentVoltage2 = bmsSeg[2];

  // Bytes 6-7: pack current as signed int16 in deci-amps (0.1 A)
  if (msg.len >= 8) {
    bmsCurrent = read_i16_be(&msg.buf[6]) / 10.0f;
    Serial.printf("BMS CURRENT: %.1fA\n", bmsCurrent);
  }
}

void decode_segment_voltages_2(const CAN_message_t &msg) {
  if (msg.len < 6)
    return;
  bmsSeg[3] = read_u16_be(&msg.buf[0]) / 100.0f;
  bmsSeg[4] = read_u16_be(&msg.buf[2]) / 100.0f;
  bmsTotal = read_u16_be(&msg.buf[4]) / 100.0f;

  // Update remaining segment voltage variables for telemetry
  segmentVoltage3 = bmsSeg[3];
  segmentVoltage4 = bmsSeg[4];

  // Bytes 6-7: lowest cell voltage as uint16 in millivolts
  uint16_t low_cell_mv = (uint16_t)((msg.buf[6] << 8) | msg.buf[7]);
  bmsLowestCellV = low_cell_mv / 1000.0f;
  Serial.printf("LOWEST CELL V: %.4f\n", bmsLowestCellV);
}

void decode_pack_current(const CAN_message_t &msg) {
  if (msg.len < 2)
    return;
  bmsCurrent = read_i16_be(&msg.buf[0]) / 10.0f;
  Serial.printf("BMS CURRENT PKT: %.1fA\n", bmsCurrent);
}

void decode_lowest_cell(const CAN_message_t &msg) {
  if (msg.len < 6)
    return;
  bmsLowestCellV = read_u16_be(&msg.buf[0]) / 1000.0f;
  bmsLowestCellIC = msg.buf[4];
  bmsLowestCellIndex = msg.buf[5];
}

void decode_latch_status(const CAN_message_t &msg) {
  if (msg.len < 2)
    return;
  bmsLatchState = msg.buf[0];
  bmsFaultFlags = msg.buf[1];
}

void decode_temperature_summary(const CAN_message_t &msg) {
  if (msg.len < 8)
    return;
  bmsPackAvgTemp = (float)(int8_t)msg.buf[6];
}
