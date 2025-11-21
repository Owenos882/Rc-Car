#include <WiFi.h>
#include <WebServer.h>

// -------------------------------------------------------------
// WIFI HOTSPOT LOGIN
// -------------------------------------------------------------
const char* ssid = "OOS";
const char* password = "123456789";

// -------------------------------------------------------------
// MOTOR DRIVER PINS (L298N)
// -------------------------------------------------------------
#define IN1 27
#define IN2 26
#define IN3 33
#define IN4 25

#define ENA 14
#define ENB 32

// -------------------------------------------------------------
// EXTRAS
// -------------------------------------------------------------
#define HEADLIGHT_PIN 23
#define BUZZER 15

WebServer server(80);

// -------------------------------------------------------------
// GLOBALS
// -------------------------------------------------------------
volatile int leftSpeed = 0;   // -255 .. 255
volatile int rightSpeed = 0;  // -255 .. 255

unsigned long lastPWM = 0;

// -------------------------------------------------------------
// SOFTWARE PWM FOR MOTORS (NO LEDC NEEDED)
// -------------------------------------------------------------
void updatePWM() {
  static unsigned long last = 0;
  unsigned long now = micros();
  if (now - last < 5000) return;  // 5ms cycle
  last = now;

  int ls = abs(leftSpeed);
  int rs = abs(rightSpeed);

  int lt = map(ls, 0, 255, 0, 5000);
  int rt = map(rs, 0, 255, 0, 5000);

  // LEFT MOTOR
  if (lt > 0) {
    if (leftSpeed >= 0) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); }
    else               { digitalWrite(IN1, LOW);  digitalWrite(IN2, HIGH); }
    digitalWrite(ENA, HIGH);
  } else {
    digitalWrite(ENA, LOW);
  }

  // RIGHT MOTOR
  if (rt > 0) {
    if (rightSpeed >= 0) { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); }
    else                { digitalWrite(IN3, LOW);  digitalWrite(IN4, HIGH); }
    digitalWrite(ENB, HIGH);
  } else {
    digitalWrite(ENB, LOW);
  }
}

// -------------------------------------------------------------
// DIFFERENTIAL DRIVE (TRUE ANALOG, PWM-LESS)
// -------------------------------------------------------------
void drive(int x, int y) {
  if (abs(x) < 10 && abs(y) < 10) {
    leftSpeed = 0;
    rightSpeed = 0;
    return;
  }

  int left  = y + x;
  int right = y - x;

  left  = constrain(left,  -255, 255);
  right = constrain(right, -255, 255);

  leftSpeed  = left;
  rightSpeed = right;
}

// -------------------------------------------------------------
// BUTTON COMMAND HANDLER
// -------------------------------------------------------------
void handleCmd() {
  String c = server.arg("d");

  if (c == "HL_ON")  digitalWrite(HEADLIGHT_PIN, HIGH);
  if (c == "HL_OFF") digitalWrite(HEADLIGHT_PIN, LOW);
  if (c == "BEEP") {
    digitalWrite(BUZZER, HIGH);
    delay(150);
    digitalWrite(BUZZER, LOW);
  }

  server.send(200, "text/plain", "OK");
}

// -------------------------------------------------------------
// JOYSTICK HANDLER (X/Y)
// -------------------------------------------------------------
void handleMove() {
  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  drive(x, y);
  server.send(200, "text/plain", "OK");
}

// -------------------------------------------------------------
// HTML PAGE
// -------------------------------------------------------------
const char HTML_PAGE[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">

<title>ESP32 RC Car</title>

<style>
body {
  margin: 0;
  padding: 0;
  background: #0b0f1a;
  color: white;
  font-family: Arial;
  text-align: center;
  user-select: none;
}

h1 {
  margin-top: 20px;
}

#joyArea {
  position: relative;
  width: 260px;
  height: 260px;
  margin: 30px auto;
}

#base {
  position: absolute;
  width: 240px;
  height: 240px;
  border-radius: 50%;
  border: 3px solid #5af;
  background: rgba(255,255,255,0.05);
  left: 10px;
  top: 10px;
}

#knob {
  position: absolute;
  width: 110px;
  height: 110px;
  border-radius: 50%;
  background: #4af;
  box-shadow: 0 0 20px #4af;
  left: 75px;
  top: 75px;
  touch-action: none;
}

#buttons {
  width: 80%;
  margin: 0 auto;
  margin-top: 20px;
  max-width: 260px;
  display: flex;
  flex-direction: column;
  gap: 12px;
}

button {
  padding: 15px;
  font-size: 20px;
  background: #4af;
  border: none;
  border-radius: 10px;
  box-shadow: 0 0 12px #4af;
}
</style>
</head>

<body>

<h1>ESP32 RC Car</h1>

<div id="joyArea">
  <div id="base"></div>
  <div id="knob"></div>
</div>

<div id="buttons">
  <button onclick="cmd('HL_ON')">Headlights ON</button>
  <button onclick="cmd('HL_OFF')">Headlights OFF</button>
  <button onclick="cmd('BEEP')">Beep</button>
</div>

<script>
let area = document.getElementById("joyArea");
let knob = document.getElementById("knob");

let rect, cx, cy, maxR = 110;
let active = false;

function recalc() {
  rect = area.getBoundingClientRect();
  cx = rect.left + rect.width / 2;
  cy = rect.top + rect.height / 2;
}
window.onload = recalc;
window.onresize = recalc;

function cmd(c) {
  fetch("/cmd?d=" + c);
}

function moveXY(x, y) {
  fetch(`/move?x=${x}&y=${y}`).catch(e=>{});
}

function centerKnob() {
  knob.style.left = "75px";
  knob.style.top = "75px";
}

function handleMove(clientX, clientY) {
  let dx = clientX - cx;
  let dy = clientY - cy;
  let dist = Math.sqrt(dx*dx + dy*dy);

  if (dist > maxR) {
    dx = dx/dist * maxR;
    dy = dy/dist * maxR;
  }

  knob.style.left = (75 + dx) + "px";
  knob.style.top  = (75 + dy) + "px";

  let outX = Math.round(dx / maxR * 255);
  let outY = Math.round(-dy / maxR * 255);

  moveXY(outX, outY);
}

// TOUCH
area.addEventListener("touchstart", e => {
  active = true;
  let t = e.touches[0];
  handleMove(t.clientX, t.clientY);
});
area.addEventListener("touchmove", e => {
  if (!active) return;
  let t = e.touches[0];
  handleMove(t.clientX, t.clientY);
  e.preventDefault();
});
area.addEventListener("touchend", e => {
  active = false;
  centerKnob();
  moveXY(0, 0);
});

// MOUSE (for testing)
area.addEventListener("mousedown", e => {
  active = true;
  handleMove(e.clientX, e.clientY);
});
window.addEventListener("mousemove", e => {
  if (!active) return;
  handleMove(e.clientX, e.clientY);
});
window.addEventListener("mouseup", e => {
  active = false;
  centerKnob();
  moveXY(0, 0);
});
</script>

</body>
</html>
)HTML";

// -------------------------------------------------------------
// SETUP
// -------------------------------------------------------------
void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(HEADLIGHT_PIN, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }

  Serial.println("\nCONNECTED!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", [](){ server.send_P(200, "text/html", HTML_PAGE); });
  server.on("/move", handleMove);
  server.on("/cmd", handleCmd);
  server.begin();
}

// -------------------------------------------------------------
// LOOP
// -------------------------------------------------------------
void loop() {
  server.handleClient();
  updatePWM();   // software PWM for analog drive
}
