#include <SoftwareSerial.h>

// Define software serial pins
#define FINGERPRINT_RX 2
#define FINGERPRINT_TX 3
SoftwareSerial fpsSerial(FINGERPRINT_RX, FINGERPRINT_TX);

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

// Add confirmation code constants
const byte ACK_SUCCESS = 0x00;
const byte ACK_FAIL = 0x01;
const byte ACK_NO_FINGER = 0x02;
const byte ACK_ENROLL_FAIL = 0x03;
// ... add other confirmation codes as needed ...

struct CommandPacket {
  byte header[2] = {HEADER_1, HEADER_2};
  uint32_t adder = DEFAULT_ADDER;
  byte pid;
  uint16_t length;
  byte* data;
  uint16_t checksum;
};

// Hardware UART test for R558S fingerprint module

void setup() {
  // Hardware Serial for PC communication
  Serial.begin(9600);
  
  // Software Serial for module communication
  fpsSerial.begin(57600);
  
  Serial.println("R558S Fingerprint Module Test");
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
      enrollFingerprint();
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
  byte data[] = {};
  sendPacket(CMD_EMPTY, data, 0);
  Serial.println("Empty database command sent");
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
