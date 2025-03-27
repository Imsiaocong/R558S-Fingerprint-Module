// ESP32-S3 Fingerprint Module interface
#include <Arduino.h>

// Define hardware serial pins for ESP32-S3
#define FINGERPRINT_RX 18
#define FINGERPRINT_TX 17
// Use Hardware Serial1 instead of SoftwareSerial
HardwareSerial fpsSerial(1); // Use UART1

// Package format constants
const byte HEADER_1 = 0xEF;
const byte HEADER_2 = 0x01;
const uint32_t DEFAULT_ADDER = 0xFFFFFFFF;
const byte CMD_PACKET = 0x01;
const byte DATA_PACKET = 0x02;
const byte ACK_PACKET = 0x07;
const byte END_PACKET = 0x08;

// Command codes
const byte CMD_HANDSHAKE = 0x35;
const byte CMD_ENROLL = 0x31;
const byte CMD_SEARCH = 0x04;
const byte CMD_EMPTY = 0x0D;
const byte CMD_GET_IMAGE = 0x01;  // GetImg command
const byte CMD_LED_CONTROL = 0x3C;  // ControlBLN command
const byte CMD_GET_ENROLL_IMAGE = 0x29;  // GetEnrollImage command
const byte CMD_AUTO_ENROLL = 0x31;  // AutoEnroll command

// Add confirmation code constants
const byte ACK_SUCCESS = 0x00;
const byte ACK_FAIL = 0x01;
const byte ACK_NO_FINGER = 0x02;
const byte ACK_ENROLL_FAIL = 0x03;

// Add enrollment-related confirmation codes
const byte ENROLL_INVALID_ID = 0x0B;
const byte ENROLL_DATABASE_FULL = 0x1F;
const byte ENROLL_TEMPLATE_EXISTS = 0x22;
const byte ENROLL_INVALID_ENTRY_COUNT = 0x25;
const byte ENROLL_TIMEOUT = 0x26;
const byte ENROLL_DUPLICATE = 0x27;
const byte ENROLL_AREA_OVERLAP = 0x28;
const byte ENROLL_POOR_QUALITY = 0x3B;

// Add these color constants at the top with other constants
const byte COLOR_RED = 0x04;
const byte COLOR_GREEN = 0x02;
const byte COLOR_BLUE = 0x01;
const byte COLOR_OFF = 0x00;
const byte COLOR_YELLOW = 0x06;    // Red + Green
const byte COLOR_CYAN = 0x03;      // Green + Blue
const byte COLOR_MAGENTA = 0x05;   // Red + Blue
const byte COLOR_WHITE = 0x07;     // Red + Green + Blue

struct CommandPacket {
  byte header[2] = {HEADER_1, HEADER_2};
  uint32_t adder = DEFAULT_ADDER;
  byte pid;
  uint16_t length;
  byte* data;
  uint16_t checksum;
};

// Hardware UART test for R558S fingerprint module

// Function prototypes
void autoEnroll(uint16_t fingerID, byte numEntries = 2, bool requireFingerRemoval = true, bool allowOverwrite = false, bool checkDuplicate = true);

void setup() {
  // Hardware Serial for PC communication
  Serial.begin(9600);
  
  // Hardware Serial for module communication
  fpsSerial.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  
  Serial.println("R558S Fingerprint Module Test - ESP32-S3");
  Serial.println("Commands:");
  Serial.println("h - Send handshake command");
  Serial.println("c - Send handshake (alternative)");
  Serial.println("s - Search fingerprint");
  Serial.println("i - Capture fingerprint image");
  Serial.println("e - Enroll fingerprint");
  Serial.println("d - Empty fingerprint database");
  Serial.println("l - LED breathing (blue)");
  Serial.println("f - LED flashing (red)");
  Serial.println("o - LED constantly on (white)");
  Serial.println("x - LED off");
  Serial.println("r - RGB breathing effect");
  Serial.println("------------------");
}

void loop() {
  // Check for commands from PC
  if (Serial.available()) {
    char cmd = Serial.read();
    
    // Use the handleCommand function to process all commands
    handleCommand(cmd);
  }
  
  // Check for responses from module
  if (fpsSerial.available()) {
    byte receivedByte = fpsSerial.read();
    Serial.print("Received: 0x");
    if (receivedByte < 0x10) Serial.print("0"); // Pad with leading zero
    Serial.println(receivedByte, HEX);
  }
}

void sendCommand(byte command, byte* params = NULL, int paramLength = 0) {
  // Calculate length (command + params + checksum)
  uint16_t length = 1 + paramLength + 2;
  
  // Send header
  fpsSerial.write((uint8_t)HEADER_1);
  fpsSerial.write((uint8_t)HEADER_2);
  
  // Send adder (4 bytes - 0xFFFFFFFF)
  for (int i = 0; i < 4; i++) {
    fpsSerial.write((uint8_t)0xFF);
  }
  
  // Package identifier
  byte pid = 0x01; // Command packet
  fpsSerial.write(pid);
  
  // Send length (high byte first)
  fpsSerial.write((uint8_t)(length >> 8));
  fpsSerial.write((uint8_t)(length & 0xFF));
  
  // Send command
  fpsSerial.write((uint8_t)command);
  
  // Send parameters if any
  for (int i = 0; i < paramLength; i++) {
    fpsSerial.write((uint8_t)params[i]);
  }
  
  // Calculate checksum: pid + length + command + params
  uint16_t checksum = pid;
  checksum += (length >> 8) & 0xFF;
  checksum += length & 0xFF;
  checksum += command;
  
  // Add parameters to checksum
  for (int i = 0; i < paramLength; i++) {
    checksum += params[i];
  }
  
  // Send checksum (high byte first)
  fpsSerial.write((uint8_t)(checksum >> 8));
  fpsSerial.write((uint8_t)(checksum & 0xFF));
  
  Serial.print("Command sent with checksum: 0x");
  if (checksum < 0x10) Serial.print("0");
  Serial.println(checksum, HEX);
}

void sendHandshakePacket() {
  Serial.println("Sending handshake command...");
  sendCommand(0x35); // Handshake command (0x35)
}

void sendStatusPacket() {
  Serial.println("Sending status request...");
  sendCommand(0x34); // Get chip serial number (0x34)
}

void sendPacket(byte command, byte* data, int dataLength) {
  uint16_t length = dataLength + 2;  // Add 2 for checksum
  uint16_t checksum = command + (length >> 8) + (length & 0xFF);
  
  // Calculate checksum for data
  for (int i = 0; i < dataLength; i++) {
    checksum += data[i];
  }
  
  // Send header
  fpsSerial.write(HEADER_1);
  fpsSerial.write(HEADER_2);
  
  // Send adder (default 0xFFFFFFFF)
  for (int i = 0; i < 4; i++) {
    fpsSerial.write(0xFF);
  }
  
  // Send PID (command packet)
  fpsSerial.write(CMD_PACKET);
  
  // Send length (high byte first)
  fpsSerial.write((uint8_t)(length >> 8));
  fpsSerial.write((uint8_t)(length & 0xFF));
  
  // Send command
  fpsSerial.write(command);
  
  // Send data if any
  if (data != NULL && dataLength > 0) {
    for (int i = 0; i < dataLength; i++) {
      fpsSerial.write(data[i]);
    }
  }
  
  // Send checksum (high byte first)
  fpsSerial.write((uint8_t)(checksum >> 8));
  fpsSerial.write((uint8_t)(checksum & 0xFF));
}

void sendHandshake() {
  byte data[] = {};  // No data for handshake
  sendPacket(CMD_HANDSHAKE, data, 0);
}

void handleCommand(char cmd) {
  switch(cmd) {
    case 'h':  // Add this case for handshake
      sendHandshakePacket();
      break;
    case 'e':
      autoEnroll(1);
      break;
    case 's':
      searchFingerprint();
      break;
    case 'c':
      sendHandshake();
      break;
    case 'd':
      emptyDatabase();
      break;
    case 'i':
      getFingerImage();
      break;
    case 'l':  // LED breathing - blue to off
      controlLED(1, 0x01, 0x00, 0);  // Breathing light, blue, off, infinite
      break;
    case 'f':  // LED flashing - red
      controlLED(2, 0x04, 0x04, 10);  // Flashing, red, red, 10 cycles
      break;
    case 'o':  // LED constantly on - white
      controlLED(3, 0x07, 0x07, 0);  // Normally on, white, white, n/a
      break;
    case 'x':  // LED off
      controlLED(4, 0x00, 0x00, 0);  // Normally off
      break;
    case 'r':  // RGB breathing effect
      breathRGB();
      break;
  }
}

void enrollFingerprint() {
  Serial.println("Capturing enrollment fingerprint image...");
  sendCommand(CMD_GET_ENROLL_IMAGE);
}

void searchFingerprint() {
  Serial.println("Searching for fingerprint match...");
  
  // Parameters for search:
  // BufferID (1 byte) - Default is 1, searches with the template in buffer 1
  // StartPage (2 bytes) - First page to start searching from, typically 0
  // PageNum (2 bytes) - Number of templates to search, use 0xFFFF for all
  
  byte params[5] = {
    0x01,           // BufferID = 1 (use template in buffer 1)
    0x00, 0x00,     // StartPage = 0 (start from beginning)
    0xFF, 0xFF      // PageNum = 0xFFFF (search all templates)
  };
  
  sendCommand(CMD_SEARCH, params, 5);
  
  // The response will include:
  // - Confirmation code (0x00 = success, 0x09 = no match)
  // - PageID (2 bytes) of the matching template (if found)
  // - Match score (2 bytes) - higher score means better match
}

void emptyDatabase() {
  Serial.println("Emptying fingerprint database...");
  
  // Send empty command with no parameters
  // Instruction code: 0DH
  sendCommand(CMD_EMPTY);
  
  // Add response handling
  Serial.println("Waiting for response...");
  
  // Wait a bit for the response
  delay(1000);
  
  // Parse the response
  if (fpsSerial.available() >= 12) { // Header(2) + Address(4) + PID(1) + Length(2) + Confirmation(1) + Checksum(2)
    // Read header
    if (fpsSerial.read() == HEADER_1 && fpsSerial.read() == HEADER_2) {
      // Skip address
      for (int i = 0; i < 4; i++) {
        fpsSerial.read();
      }
      
      // Read package ID
      byte pid = fpsSerial.read();
      
      // Read length
      fpsSerial.read(); // High byte
      fpsSerial.read(); // Low byte
      
      // Read confirmation code
      byte confirmationCode = fpsSerial.read();
      
      // Skip checksum
      fpsSerial.read();
      fpsSerial.read();
      
      // Handle confirmation code
      if (confirmationCode == 0x00) {
        Serial.println("Database successfully emptied!");
      } else if (confirmationCode == 0x11) {
        Serial.println("Error: Failed to clear fingerprint library");
      } else if (confirmationCode == 0x01) {
        Serial.println("Error: Package receiving error");
      } else {
        Serial.print("Unknown error code: 0x");
        Serial.println(confirmationCode, HEX);
      }
    }
  }
}

void getFingerImage() {
  Serial.println("Capturing fingerprint image...");
  sendCommand(CMD_GET_IMAGE);
}

void readResponse() {
  if (fpsSerial.available() < 9) return;  // Minimum packet size
  
  // Look for header
  if (fpsSerial.read() != HEADER_1) return;
  if (fpsSerial.read() != HEADER_2) return;
  
  Serial.println("Found header");
  
  // Read adder (4 bytes)
  for (int i = 0; i < 4; i++) {
    fpsSerial.read();
  }
  
  byte pid = fpsSerial.read();
  uint16_t length = (fpsSerial.read() << 8) | fpsSerial.read();
  
  Serial.print("Package ID: 0x");
  Serial.println(pid, HEX);
  Serial.print("Length: 0x");
  Serial.println(length, HEX);
  
  // Read data and checksum
  byte buffer[256];  // Max length is 256 bytes
  int dataLength = length - 2;  // Subtract 2 for checksum
  
  for (int i = 0; i < dataLength; i++) {
    if (fpsSerial.available()) {
      buffer[i] = fpsSerial.read();
    } else {
      Serial.println("Not enough data available");
      return;
    }
  }
  
  // Read checksum
  uint16_t receivedChecksum = 0;
  if (fpsSerial.available() >= 2) {
    receivedChecksum = (fpsSerial.read() << 8) | fpsSerial.read();
    Serial.print("Checksum: 0x");
    Serial.println(receivedChecksum, HEX);
  } else {
    Serial.println("Checksum not available");
    return;
  }
  
  // Process response based on PID
  switch(pid) {
    case ACK_PACKET:
      handleAcknowledgement(buffer[0], buffer + 1, dataLength - 1);
      break;
    case DATA_PACKET:
      Serial.println("Data packet received");
      break;
    case END_PACKET:
      Serial.println("End packet received");
      break;
    default:
      Serial.print("Unknown packet type: 0x");
      Serial.println(pid, HEX);
  }
}

void handleAcknowledgement(byte confirmationCode, byte* params, int paramLength) {
  Serial.print("Acknowledgment: ");
  
  switch(confirmationCode) {
    case 0x00:
      Serial.println("Command executed successfully");
      break;
    case 0x01:
      Serial.println("Error: Data package receiving error");
      break;
    case 0x02:
      Serial.println("Error: No finger detected");
      break;
    case 0x03:
      Serial.println("Error: Failed to record fingerprint image");
      break;
    case 0x04:
      Serial.println("Error: Fingerprint too dry or faint");
      break;
    case 0x05:
      Serial.println("Error: Fingerprint too wet or mushy");
      break;
    case 0x06:
      Serial.println("Error: Image too disorderly");
      break;
    case 0x07:
      Serial.println("Error: Too few feature points");
      break;
    case 0x08:
      Serial.println("Error: Fingerprint doesn't match");
      break;
    case 0x09:
      Serial.println("Error: Failed to find matching fingerprint");
      break;
    case 0x0A:
      Serial.println("Error: Failed to combine character files");
      break;
    case 0x0B:
      Serial.println("Error: PageID beyond finger library");
      break;
    case 0x0C:
      Serial.println("Error: Invalid template or read error");
      break;
    case 0x0D:
      Serial.println("Error: Feature upload error");
      break;
    case 0x0E:
      Serial.println("Error: Module can't receive following packages");
      break;
    case 0x0F:
      Serial.println("Error: Image upload error");
      break;
    case 0x10:
      Serial.println("Error: Failed to delete template");
      break;
    case 0x11:
      Serial.println("Error: Failed to clear fingerprint library");
      break;
    case 0x12:
      Serial.println("Error: Failed to enter low-power state");
      break;
    case 0x1F:
      Serial.println("Error: Fingerprint library is full");
      break;
    case 0x26:
      Serial.println("Error: Operation timeout");
      break;
    case 0x27:
      Serial.println("Error: Fingerprint already exists");
      break;
    default:
      Serial.print("Unknown confirmation code: 0x");
      Serial.println(confirmationCode, HEX);
  }
  
  // Print any additional parameters if present
  if (paramLength > 0) {
    Serial.print("Additional parameters: ");
    for (int i = 0; i < paramLength; i++) {
      Serial.print("0x");
      Serial.print(params[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
  }
}

// Function for LED control
void controlLED(byte functionCode, byte startColor, byte endColor, byte cycleTimes) {
  Serial.println("Controlling LED...");
  
  // Create parameter array for LED control
  byte params[4] = {functionCode, startColor, endColor, cycleTimes};
  
  // Send command with parameters
  sendCommand(CMD_LED_CONTROL, params, 4);
}

// Modified function for smooth rainbow transition effect
void breathRGB() {
  Serial.println("Starting smooth rainbow transition effect...");
  
  // According to the manual format:
  // Byte 1: Function code (0x07)
  // Byte 2: Time bit (36 = 0x24 for 3.6s cycle)
  // Bytes 3-7: Color codes (Valid bit(1111) + RGB bits)
  // Byte 8: Cycle times
  byte params[8] = {
    0x07,           // Function code: 7 for colorful programmed breathing
    0x24,           // Time bit: 36 (0x24) for 3.6s per breath cycle
    0xF4,           // Color 1: Valid(1111) + Red(0100)
    0xF2,           // Color 2: Valid(1111) + Green(0010)
    0xF1,           // Color 3: Valid(1111) + Blue(0001)
    0xF6,           // Color 4: Valid(1111) + Yellow(0110)
    0xF5,           // Color 5: Valid(1111) + Magenta(0101)
    0x00            // Cycle times: 0 for infinite
  };
  
  // Send the command with proper length
  sendCommand(CMD_LED_CONTROL, params, 8);
}

// Function to perform auto enrollment
void autoEnroll(uint16_t fingerID, byte numEntries = 2, bool requireFingerRemoval = true, bool allowOverwrite = false, bool checkDuplicate = true) {
  Serial.println("Starting fingerprint enrollment...");
  Serial.print("Finger ID: ");
  Serial.println(fingerID);
  Serial.print("Number of captures required: ");
  Serial.println(numEntries);
  
  // Prepare parameters
  // Parameter bits:
  // bit2: Status return (0: return status, 1: don't return)
  // bit3: ID overwrite (0: not allowed, 1: allowed)
  // bit4: Duplicate check (0: allowed, 1: not allowed)
  // bit5: Finger removal (0: required, 1: not required)
  byte parameterBits = 0x00;
  if (!requireFingerRemoval) parameterBits |= (1 << 5);
  if (allowOverwrite) parameterBits |= (1 << 3);
  if (!checkDuplicate) parameterBits |= (1 << 4);
  
  // Create parameter array
  byte params[5] = {
    (byte)(fingerID >> 8),    // ID high byte
    (byte)(fingerID & 0xFF),  // ID low byte
    numEntries,               // Number of entries
    (byte)(parameterBits >> 8),  // Parameters high byte
    (byte)(parameterBits & 0xFF) // Parameters low byte
  };
  
  // Send enrollment command
  sendCommand(CMD_AUTO_ENROLL, params, 5);
  
  // Start monitoring the enrollment process
  monitorEnrollment();
}

// Function to monitor the enrollment process and provide feedback
void monitorEnrollment() {
  bool enrollmentComplete = false;
  byte currentStep = 0;
  byte currentEntry = 0;
  
  while (!enrollmentComplete) {
    if (fpsSerial.available()) {
      // Read and process response packet
      if (readEnrollmentResponse(&currentStep, &currentEntry)) {
        // Check if enrollment is complete
        if (currentStep == 0x06) { // Template storage step
          enrollmentComplete = true;
        }
      }
    }
  }
}

// Function to read and process enrollment response packets
bool readEnrollmentResponse(byte* step, byte* entry) {
  if (fpsSerial.available() < 12) return false; // Minimum packet size
  
  // Look for header
  if (fpsSerial.read() != HEADER_1) return false;
  if (fpsSerial.read() != HEADER_2) return false;
  
  // Skip adder bytes
  for (int i = 0; i < 4; i++) {
    fpsSerial.read();
  }
  
  // Read packet data
  byte pid = fpsSerial.read();
  byte lengthHigh = fpsSerial.read();
  byte lengthLow = fpsSerial.read();
  byte confirmationCode = fpsSerial.read();
  byte param1 = fpsSerial.read();
  byte param2 = fpsSerial.read();
  
  // Update step and entry tracking
  *step = param1;
  if (param2 < 0xF0) {
    *entry = param2;
  }
  
  // Process response based on confirmation code and parameters
  switch (confirmationCode) {
    case ACK_SUCCESS:
      printEnrollmentStatus(param1, param2);
      break;
      
    case ENROLL_INVALID_ID:
      Serial.println("Error: Invalid ID number");
      return false;
      
    case ENROLL_DATABASE_FULL:
      Serial.println("Error: Fingerprint database is full");
      return false;
      
    case ENROLL_TEMPLATE_EXISTS:
      Serial.println("Error: Template already exists for this ID");
      return false;
      
    case ENROLL_INVALID_ENTRY_COUNT:
      Serial.println("Error: Invalid number of entries specified");
      return false;
      
    case ENROLL_TIMEOUT:
      Serial.println("Error: Operation timeout");
      return false;
      
    case ENROLL_DUPLICATE:
      Serial.println("Warning: Duplicate fingerprint detected");
      break;
      
    case ENROLL_AREA_OVERLAP:
      Serial.println("Warning: Finger area overlap detected, please use a different area");
      break;
      
    case ENROLL_POOR_QUALITY:
      Serial.println("Error: Poor template quality");
      return false;
  }
  
  return true;
}

// Function to print enrollment status messages
void printEnrollmentStatus(byte param1, byte param2) {
  switch (param1) {
    case 0x00:
      Serial.println("Instruction validity check passed");
      break;
      
    case 0x01:
      Serial.print("Collecting fingerprint image #");
      Serial.println(param2);
      break;
      
    case 0x02:
      Serial.print("Generating features for capture #");
      Serial.println(param2);
      break;
      
    case 0x03:
      Serial.println("Waiting for finger to be removed");
      break;
      
    case 0x04:
      if (param2 == 0xF0) {
        Serial.println("Template merged successfully");
      }
      break;
      
    case 0x05:
      if (param2 == 0xF1) {
        Serial.println("Registration check complete");
      }
      break;
      
    case 0x06:
      if (param2 == 0xF2) {
        Serial.println("Template stored successfully");
      }
      break;
  }
}
