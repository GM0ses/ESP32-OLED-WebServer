#include <WiFi.h>                  // ESP32 WiFi library
#include <Wire.h>                  // I2C communication library
#include <Adafruit_GFX.h>          // Graphics library for OLED text/shapes
#include <Adafruit_SSD1306.h>      // SSD1306 OLED driver

// ---------------- OLED SETTINGS ----------------
#define SCREEN_WIDTH 128           // OLED width in pixels
#define SCREEN_HEIGHT 64           // OLED height in pixels

// Create OLED display object
// Parameters: width, height, I2C interface, reset pin (-1 = no reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ---------------- NETWORK SETTINGS ----------------
// Static IP settings for the ESP32
IPAddress ip(192, 168, 0, 51);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);

// WiFi credentials
const char* ssid = "YourWiFi";
const char* password = "YourPassword";

// ---------------- OUTPUT SETTINGS ----------------
const int outputPin = 2;           // GPIO pin used as digital output

// Create a web server on port 80
WiFiServer server(80);

// -------------------------------------------------
// Function: updateOLED
// Purpose : Clears the OLED and writes 4 lines of text
// -------------------------------------------------
void updateOLED(String line1, String line2, String line3, String line4) {
  display.clearDisplay();          // Clear previous screen contents
  display.setTextSize(1);          // Small text size
  display.setTextColor(WHITE);     // White text
  display.setCursor(0, 0);         // Top line
  display.println(line1);

  display.setCursor(0, 16);        // Second line
  display.println(line2);

  display.setCursor(0, 32);        // Third line
  display.println(line3);

  display.setCursor(0, 48);        // Fourth line
  display.println(line4);

  display.display();               // Push buffer to OLED so text becomes visible
}

// -------------------------------------------------
// Function: getOutputState
// Purpose : Returns "ON" or "OFF" depending on output pin state
// -------------------------------------------------
String getOutputState() {
  if (digitalRead(outputPin) == HIGH) {
    return "Output: ON";
  } else {
    return "Output: OFF";
  }
}

// -------------------------------------------------
// SETUP
// -------------------------------------------------
void setup() {
  // Start serial monitor for debugging
  Serial.begin(115200);
  delay(1000);
  Serial.println("Booting...");

  // Set output pin as an output
  pinMode(outputPin, OUTPUT);

  // Ensure output starts OFF
  digitalWrite(outputPin, LOW);

  // Start I2C communication
  // ESP32 default common pins:
  // SDA = GPIO21
  // SCL = GPIO22
  Wire.begin(21, 22);

  // Initialize the OLED display
  // 0x3C is the common I2C address for many SSD1306 OLEDs
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("SSD1306 allocation failed");
    while (true);                  // Stop here if OLED is not found
  }

  // Show startup message on OLED
  updateOLED("GMoses", "Starting...", "", "");

  // Disconnect WiFi first to clear previous connection state
  WiFi.disconnect(true);
  delay(1000);

  // Put ESP32 into station mode (connects to router)
  WiFi.mode(WIFI_STA);

  // Apply static IP settings before connecting
  WiFi.config(ip, gateway, subnet);

  // Start WiFi connection
  WiFi.begin(ssid, password);

  Serial.print("Connecting to WiFi");
  updateOLED("GMoses", "Connecting WiFi", "", "");

  // Wait until WiFi is connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  // Start the web server
  server.begin();
  Serial.println("Web server started");

  // Show connected status on OLED
  updateOLED("GMoses", "WiFi Connected", WiFi.localIP().toString(), getOutputState());
}

// -------------------------------------------------
// LOOP
// -------------------------------------------------
void loop() {
  // Check if a browser/client is connecting to the server
  WiFiClient client = server.available();

  // Only continue if a client has connected
  if (client) {
    Serial.println("Client connected");

    // Show client connection on OLED
    updateOLED("GMoses", "Client connected", WiFi.localIP().toString(), getOutputState());

    String request = "";                   // Store incoming HTTP request
    unsigned long timeout = millis();      // Start timeout counter

    // Wait for client data, but do not wait forever
    while (client.connected() && !client.available() && millis() - timeout < 2000) {
      delay(1);
    }

    // Read incoming request while client is connected
    while (client.connected() && millis() - timeout < 2000) {
      if (client.available()) {
        char c = client.read();            // Read one character from browser
        request += c;                      // Add character to request string
        Serial.print(c);                   // Print request to Serial Monitor

        // Check for end of HTTP header
        // Browsers end header section with: \r\n\r\n
        if (request.indexOf("\r\n\r\n") >= 0) {

          // If browser requested /ON, switch output ON
          if (request.indexOf("GET /ON") >= 0) {
            digitalWrite(outputPin, HIGH);
            Serial.println("Output turned ON");
            updateOLED("GMoses", "Client connected", WiFi.localIP().toString(), "Output: ON");
          }

          // If browser requested /OFF, switch output OFF
          if (request.indexOf("GET /OFF") >= 0) {
            digitalWrite(outputPin, LOW);
            Serial.println("Output turned OFF");
            updateOLED("GMoses", "Client connected", WiFi.localIP().toString(), "Output: OFF");
          }

          // -------- Send HTTP response back to browser --------
          client.println("HTTP/1.1 200 OK");
          client.println("Content-Type: text/html");
          client.println("Connection: close");
          client.println();

          // Start HTML page
          client.println("<!DOCTYPE html>");
          client.println("<html>");
          client.println("<head>");
          client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
          client.println("<title>ESP32 Output Control</title>");
          client.println("</head>");
          client.println("<body>");
          client.println("<h1>GMoses</h1>");
          client.println("<p>Webserver Control</p>");

          // Show current output state on webpage
          client.print("<p>Output state: ");
          if (digitalRead(outputPin) == HIGH) {
            client.print("ON");
          } else {
            client.print("OFF");
          }
          client.println("</p>");

          // Add ON and OFF buttons
          client.println("<p><a href='/ON'><button>ON</button></a></p>");
          client.println("<p><a href='/OFF'><button>OFF</button></a></p>");

          // End HTML page
          client.println("</body>");
          client.println("</html>");
          client.println();

          // Exit loop after responding
          break;
        }
      }
    }

    delay(10);                      // Small delay to allow response to send
    client.stop();                  // Disconnect client
    Serial.println("Client disconnected");

    // After client disconnects, keep OLED showing current status
    updateOLED("GMoses", "Waiting...", WiFi.localIP().toString(), getOutputState());
  }
}