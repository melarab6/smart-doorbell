#include <WiFi.h>
#include "esp_camera.h"


// ============================================================
// WI-FI SETTINGS
// ============================================================

const char* ssid = "YOUR_WIFI_NAME";
const char* password = "YOUR_WIFI_PASSWORD";


// ============================================================
// FLASK SERVER SETTINGS
// ============================================================

// Computer running Flask.
IPAddress serverIP(10, 0, 0, 103);

const int serverPort = 5000;

const char* eventPath = "/api/events";


// ============================================================
// HARDWARE PINS
// ============================================================

// Doorbell button:
// D1 -> button -> GND
const int buttonPin = D1;


// PIR motion sensor:
// PIR OUT -> D2
const int pirPin = D2;


// ============================================================
// EVENT FLAGS
// ============================================================

volatile bool buttonEventPending = false;
volatile bool motionEventPending = false;


// ============================================================
// TIMING SETTINGS
// ============================================================

// PIR warm-up time.
const unsigned long PIR_WARMUP_MS = 30000;


// Minimum time between motion events.
const unsigned long MOTION_COOLDOWN_MS = 5000;


// Number of complete upload attempts.
const int MAX_UPLOAD_ATTEMPTS = 3;


// Maximum time with no JPEG upload progress.
const unsigned long JPEG_STALL_TIMEOUT_MS = 7000;


unsigned long programStartTime = 0;
unsigned long lastMotionEventTime = 0;

bool pirReadyMessagePrinted = false;


// ============================================================
// XIAO ESP32-S3 SENSE CAMERA PINS
// ============================================================

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1

#define XCLK_GPIO_NUM  10
#define SIOD_GPIO_NUM  40
#define SIOC_GPIO_NUM  39

#define Y9_GPIO_NUM    48
#define Y8_GPIO_NUM    11
#define Y7_GPIO_NUM    12
#define Y6_GPIO_NUM    14
#define Y5_GPIO_NUM    16
#define Y4_GPIO_NUM    18
#define Y3_GPIO_NUM    17
#define Y2_GPIO_NUM    15

#define VSYNC_GPIO_NUM 38
#define HREF_GPIO_NUM  47
#define PCLK_GPIO_NUM  13


// ============================================================
// FUNCTION DECLARATIONS
// ============================================================

bool connectToWiFi();

bool initializeCamera();

bool captureAndSend(
  const char* eventType
);

bool sendImageEvent(
  const char* eventType,
  camera_fb_t* picture
);

bool performUploadAttempt(
  const char* eventType,
  camera_fb_t* picture
);


// ============================================================
// INTERRUPT FUNCTIONS
// ============================================================

// Button press:
// HIGH -> LOW
void IRAM_ATTR onButtonPress() {
  buttonEventPending = true;
}


// PIR motion:
// LOW -> HIGH
void IRAM_ATTR onMotionDetected() {
  motionEventPending = true;
}


// ============================================================
// SETUP
// ============================================================

void setup() {

  programStartTime = millis();


  // ----------------------------------------------------------
  // Serial Monitor
  // ----------------------------------------------------------

  Serial.begin(115200);

  delay(2000);

  Serial.println();
  Serial.println("==============================");
  Serial.println("Smart Doorbell Starting");
  Serial.println("==============================");


  // ----------------------------------------------------------
  // Configure button and PIR
  // ----------------------------------------------------------

  // Button:
  //
  // released = HIGH
  // pressed  = LOW
  pinMode(buttonPin, INPUT_PULLUP);


  // PIR output.
  pinMode(pirPin, INPUT);


  // ----------------------------------------------------------
  // Wi-Fi
  // ----------------------------------------------------------

  if (!connectToWiFi()) {

    Serial.println(
      "WARNING: Initial Wi-Fi connection failed."
    );
  }


  // ----------------------------------------------------------
  // Camera
  // ----------------------------------------------------------

  if (!initializeCamera()) {

    Serial.println(
      "ERROR: Camera initialization failed."
    );

    Serial.println(
      "Doorbell cannot continue."
    );

    return;
  }


  // ----------------------------------------------------------
  // Hardware interrupts
  // ----------------------------------------------------------

  attachInterrupt(
    digitalPinToInterrupt(buttonPin),
    onButtonPress,
    FALLING
  );


  attachInterrupt(
    digitalPinToInterrupt(pirPin),
    onMotionDetected,
    RISING
  );


  // Ignore anything triggered during startup.
  buttonEventPending = false;
  motionEventPending = false;


  Serial.println();
  Serial.println("Camera ready.");
  Serial.println("Button ready.");
  Serial.println("PIR warming up...");
  Serial.println();
  Serial.println("Smart doorbell ready!");
}


// ============================================================
// MAIN LOOP
// ============================================================

void loop() {

  unsigned long now = millis();


  // ==========================================================
  // PIR WARM-UP
  // ==========================================================

  bool pirReady =
    (now - programStartTime >= PIR_WARMUP_MS);


  if (!pirReady) {

    motionEventPending = false;
  }

  else if (!pirReadyMessagePrinted) {

    Serial.println("PIR ready.");

    pirReadyMessagePrinted = true;

    motionEventPending = false;
  }


  // ==========================================================
  // DOORBELL BUTTON
  // ==========================================================

  if (buttonEventPending) {

    buttonEventPending = false;


    // --------------------------------------------------------
    // Button debounce
    // --------------------------------------------------------

    detachInterrupt(
      digitalPinToInterrupt(buttonPin)
    );


    delay(50);


    attachInterrupt(
      digitalPinToInterrupt(buttonPin),
      onButtonPress,
      FALLING
    );


    Serial.println();
    Serial.println("Doorbell pressed");


    captureAndSend("doorbell");


    return;
  }


  // ==========================================================
  // PIR MOTION
  // ==========================================================

  if (
    pirReady &&
    motionEventPending
  ) {

    motionEventPending = false;


    if (
      lastMotionEventTime == 0 ||
      now - lastMotionEventTime >=
      MOTION_COOLDOWN_MS
    ) {

      Serial.println();
      Serial.println("Motion detected");


      captureAndSend("motion");


      lastMotionEventTime =
        millis();
    }
  }


  delay(5);
}


// ============================================================
// CONNECT TO WI-FI
// ============================================================

bool connectToWiFi() {

  // Already connected.
  if (
    WiFi.status() == WL_CONNECTED
  ) {

    return true;
  }


  Serial.println(
    "Connecting to Wi-Fi..."
  );


  // ==========================================================
  // CANCEL OLD CONNECTION ATTEMPT
  // ==========================================================

  WiFi.disconnect(false, false);

  delay(1000);


  // ==========================================================
  // CONFIGURE WI-FI
  // ==========================================================

  WiFi.mode(WIFI_STA);

  WiFi.persistent(false);

  WiFi.setAutoReconnect(true);

  WiFi.setSleep(false);


  // ==========================================================
  // START CONNECTION
  // ==========================================================

  WiFi.begin(
    ssid,
    password
  );


  unsigned long connectionStart =
    millis();


  // Try for up to 20 seconds.
  while (
    WiFi.status() != WL_CONNECTED &&
    millis() - connectionStart < 20000
  ) {

    delay(500);

    Serial.print(".");
  }


  Serial.println();


  // ==========================================================
  // CONNECTION FAILED
  // ==========================================================

  if (
    WiFi.status() != WL_CONNECTED
  ) {

    Serial.println(
      "Wi-Fi connection failed."
    );


    WiFi.disconnect(false, false);

    delay(1000);


    return false;
  }


  // ==========================================================
  // CONNECTION SUCCESSFUL
  // ==========================================================

  Serial.println(
    "Wi-Fi connected!"
  );


  Serial.print(
    "ESP32 IP address: "
  );

  Serial.println(
    WiFi.localIP()
  );


  Serial.print(
    "Wi-Fi signal: "
  );

  Serial.print(
    WiFi.RSSI()
  );

  Serial.println(
    " dBm"
  );


  delay(2000);


  return true;
}


// ============================================================
// INITIALIZE CAMERA
// ============================================================

bool initializeCamera() {

  Serial.println(
    "Starting camera..."
  );


  // Camera needs PSRAM.
  if (!psramFound()) {

    Serial.println(
      "ERROR: PSRAM not found."
    );

    Serial.println(
      "Enable Tools -> PSRAM -> OPI PSRAM."
    );

    return false;
  }


  camera_config_t config = {};


  // ==========================================================
  // CAMERA CLOCK
  // ==========================================================

  config.ledc_channel =
    LEDC_CHANNEL_0;

  config.ledc_timer =
    LEDC_TIMER_0;


  // ==========================================================
  // CAMERA DATA PINS
  // ==========================================================

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;


  // ==========================================================
  // CAMERA SYNC PINS
  // ==========================================================

  config.pin_xclk =
    XCLK_GPIO_NUM;

  config.pin_pclk =
    PCLK_GPIO_NUM;

  config.pin_vsync =
    VSYNC_GPIO_NUM;

  config.pin_href =
    HREF_GPIO_NUM;


  // ==========================================================
  // CAMERA CONTROL PINS
  // ==========================================================

  config.pin_sccb_sda =
    SIOD_GPIO_NUM;

  config.pin_sccb_scl =
    SIOC_GPIO_NUM;

  config.pin_pwdn =
    PWDN_GPIO_NUM;

  config.pin_reset =
    RESET_GPIO_NUM;


  // ==========================================================
  // CAMERA SETTINGS
  // ==========================================================

  config.xclk_freq_hz =
    20000000;


  config.pixel_format =
    PIXFORMAT_JPEG;


  // 320 x 240.
  config.frame_size =
    FRAMESIZE_QVGA;


  config.jpeg_quality =
    12;


  config.fb_count =
    1;


  config.fb_location =
    CAMERA_FB_IN_PSRAM;


  config.grab_mode =
    CAMERA_GRAB_WHEN_EMPTY;


  // ==========================================================
  // START CAMERA
  // ==========================================================

  esp_err_t result =
    esp_camera_init(&config);


  if (
    result != ESP_OK
  ) {

    Serial.print(
      "Camera failed. Error: 0x"
    );

    Serial.println(
      result,
      HEX
    );


    return false;
  }


  Serial.println(
    "Camera started successfully."
  );


  return true;
}


// ============================================================
// CAPTURE ONE IMAGE
// ============================================================

bool captureAndSend(
  const char* eventType
) {

  Serial.println(
    "Taking picture..."
  );


  camera_fb_t* picture =
    esp_camera_fb_get();


  if (
    picture == NULL
  ) {

    Serial.println(
      "ERROR: Picture capture failed."
    );

    return false;
  }


  Serial.print(
    "Picture captured: "
  );

  Serial.print(
    picture->len
  );

  Serial.println(
    " bytes"
  );


  // Keep the SAME captured image in memory while
  // upload attempts are retried.
  bool success =
    sendImageEvent(
      eventType,
      picture
    );


  // Return image memory after all attempts finish.
  esp_camera_fb_return(
    picture
  );


  if (success) {

    Serial.print(
      "Event successfully uploaded: "
    );

    Serial.println(
      eventType
    );
  }

  else {

    Serial.print(
      "Event upload failed after all retries: "
    );

    Serial.println(
      eventType
    );
  }


  return success;
}


// ============================================================
// RETRY COMPLETE IMAGE UPLOAD
// ============================================================

bool sendImageEvent(
  const char* eventType,
  camera_fb_t* picture
) {

  for (
    int uploadAttempt = 1;
    uploadAttempt <= MAX_UPLOAD_ATTEMPTS;
    uploadAttempt++
  ) {

    Serial.println();


    Serial.print(
      "Full upload attempt "
    );

    Serial.print(
      uploadAttempt
    );

    Serial.print(
      " of "
    );

    Serial.println(
      MAX_UPLOAD_ATTEMPTS
    );


    // ========================================================
    // CHECK WI-FI
    // ========================================================

    if (
      WiFi.status() != WL_CONNECTED
    ) {

      Serial.println(
        "Wi-Fi disconnected."
      );

      Serial.println(
        "Trying to reconnect..."
      );


      if (!connectToWiFi()) {

        Serial.println(
          "Wi-Fi reconnect failed."
        );

        delay(1500);

        continue;
      }
    }


    Serial.print(
      "Current Wi-Fi signal: "
    );

    Serial.print(
      WiFi.RSSI()
    );

    Serial.println(
      " dBm"
    );


    // ========================================================
    // TRY COMPLETE HTTP/JPEG UPLOAD
    // ========================================================

    bool success =
      performUploadAttempt(
        eventType,
        picture
      );


    if (success) {

      return true;
    }


    Serial.println(
      "Upload attempt failed."
    );


    delay(1500);
  }


  return false;
}


// ============================================================
// PERFORM ONE COMPLETE HTTP UPLOAD
// ============================================================

bool performUploadAttempt(
  const char* eventType,
  camera_fb_t* picture
) {

  WiFiClient client;


  client.setTimeout(
    10000
  );


  // ==========================================================
  // CONNECT TO FLASK
  // ==========================================================

  Serial.println(
    "Connecting to Flask..."
  );


  if (
    !client.connect(
      serverIP,
      serverPort
    )
  ) {

    Serial.println(
      "Could not connect to Flask."
    );


    client.stop();


    return false;
  }


  Serial.println(
    "Connected to Flask!"
  );


  // ==========================================================
  // BUILD MULTIPART FORM
  // ==========================================================

  String boundary =
    "DoorbellBoundary";


  String head;


  head.reserve(
    300
  );


  // ----------------------------------------------------------
  // event_type field
  // ----------------------------------------------------------

  head += "--";

  head += boundary;

  head += "\r\n";


  head +=
    "Content-Disposition: form-data; "
    "name=\"event_type\"\r\n\r\n";


  head += eventType;

  head += "\r\n";


  // ----------------------------------------------------------
  // image field
  // ----------------------------------------------------------

  head += "--";

  head += boundary;

  head += "\r\n";


  head +=
    "Content-Disposition: form-data; "
    "name=\"image\"; "
    "filename=\"capture.jpg\"\r\n";


  head +=
    "Content-Type: image/jpeg\r\n\r\n";


  // ----------------------------------------------------------
  // End multipart body
  // ----------------------------------------------------------

  String tail;


  tail += "\r\n--";

  tail += boundary;

  tail += "--\r\n";


  // ==========================================================
  // CALCULATE BODY SIZE
  // ==========================================================

  size_t contentLength =
    head.length() +
    picture->len +
    tail.length();


  // ==========================================================
  // SEND HTTP HEADERS
  // ==========================================================

  client.print(
    "POST "
  );

  client.print(
    eventPath
  );

  client.println(
    " HTTP/1.1"
  );


  // Host header
  client.print(
    "Host: "
  );

  client.print(
    serverIP
  );

  client.print(
    ":"
  );

  client.println(
    serverPort
  );


  // Content-Type
  client.print(
    "Content-Type: multipart/form-data; boundary="
  );

  client.println(
    boundary
  );


  // Content-Length
  client.print(
    "Content-Length: "
  );

  client.println(
    contentLength
  );


  client.println(
    "Connection: close"
  );


  // Blank line ends HTTP headers.
  client.println();


  // ==========================================================
  // SEND BEGINNING OF FORM
  // ==========================================================

  client.print(
    head
  );


  // ==========================================================
  // SEND JPEG
  // ==========================================================

  Serial.println(
    "Sending JPEG..."
  );


  size_t bytesSent = 0;


  unsigned long lastSuccessfulWrite =
    millis();


  while (
    bytesSent < picture->len
  ) {

    // --------------------------------------------------------
    // Check TCP connection
    // --------------------------------------------------------

    if (
      !client.connected()
    ) {

      Serial.println(
        "ERROR: Flask connection closed during JPEG upload."
      );


      client.stop();


      return false;
    }


    // --------------------------------------------------------
    // Remaining JPEG bytes
    // --------------------------------------------------------

    size_t bytesRemaining =
      picture->len -
      bytesSent;


    size_t chunkSize;


    // Send at most 512 bytes at once.
    if (
      bytesRemaining > 512
    ) {

      chunkSize = 512;
    }

    else {

      chunkSize =
        bytesRemaining;
    }


    // --------------------------------------------------------
    // Send the chunk
    // --------------------------------------------------------

    size_t written =
      client.write(
        picture->buf +
        bytesSent,
        chunkSize
      );


    // --------------------------------------------------------
    // Successful write
    // --------------------------------------------------------

    if (
      written > 0
    ) {

      bytesSent +=
        written;


      lastSuccessfulWrite =
        millis();


      yield();
    }


    // --------------------------------------------------------
    // Wi-Fi buffer temporarily unavailable
    // --------------------------------------------------------

    else {

      delay(10);

      yield();


      // Only fail if no progress happens
      // for seven seconds.
      if (
        millis() -
        lastSuccessfulWrite >
        JPEG_STALL_TIMEOUT_MS
      ) {

        Serial.println(
          "ERROR: JPEG upload stalled for 7 seconds."
        );


        client.stop();


        return false;
      }
    }
  }


  Serial.print(
    "JPEG bytes sent: "
  );

  Serial.println(
    bytesSent
  );


  // ==========================================================
  // FINISH MULTIPART BODY
  // ==========================================================

  client.print(
    tail
  );


  Serial.println(
    "Waiting for Flask response..."
  );


  // ==========================================================
  // WAIT FOR FLASK RESPONSE
  // ==========================================================

  unsigned long responseStart =
    millis();


  while (
    !client.available() &&
    client.connected() &&
    millis() - responseStart <
    10000
  ) {

    delay(10);
  }


  if (
    !client.available()
  ) {

    Serial.println(
      "ERROR: Flask response timed out."
    );


    client.stop();


    return false;
  }


  // ==========================================================
  // READ HTTP STATUS
  // ==========================================================

  String statusLine =
    client.readStringUntil('\n');


  statusLine.trim();


  Serial.print(
    "Flask response: "
  );

  Serial.println(
    statusLine
  );


  // Flask returns HTTP 201 when event is created.
  bool success =
    statusLine.indexOf("201") >= 0;


  // ==========================================================
  // PRINT REST OF FLASK RESPONSE
  // ==========================================================

  unsigned long readStart =
    millis();


  while (
    client.connected() ||
    client.available()
  ) {

    while (
      client.available()
    ) {

      char character =
        client.read();


      Serial.write(
        character
      );


      readStart =
        millis();
    }


    if (
      millis() -
      readStart >
      3000
    ) {

      break;
    }


    delay(5);
  }


  Serial.println();


  client.stop();


  return success;
}