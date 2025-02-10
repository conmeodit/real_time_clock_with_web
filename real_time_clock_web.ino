#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClient.h>
#include <WebServer.h>

const char *ssid = "SONGNAM-STAFF";
const char *password = "songnam@123";

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

#define SDA_PIN 21
#define SCL_PIN 22

unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 300000;

const String weatherApiKey = "78ddca507b7f48bea8e72512250702";
String city = "Hanoi";
String countryCode = "VN";

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

struct WeatherData {
  float temperature;
  int humidity;
  String description;
  String icon;
} currentWeather;

const char index_html[] PROGMEM = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <title>Weather Station Control</title>
        <style>
        :root {
            --text-color: #ffffff;
            --background-gradient-start: #87ceeb;
            --background-gradient-end: #4682b4;
        }
        body {
            font-family: "Inter", Arial, sans-serif;
            max-width: 800px;
            margin: 0 auto;
            padding: 20px;
            background: linear-gradient(135deg, var(--background-gradient-start), var(--background-gradient-end));
            color: var(--text-color);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            transition: all 0.5s ease;
            background-size: 400% 400%;
            animation: gradientFlow 15s ease infinite;
        }
        @keyframes gradientFlow {
            0% { background-position: 0% 50%; }
            50% { background-position: 100% 50%; }
            100% { background-position: 0% 50%; }
        }
        .container {
            background: rgba(255, 255, 255, 0.2);
            backdrop-filter: blur(10px);
            border-radius: 16px;
            border: 1px solid rgba(255, 255, 255, 0.3);
            padding: 40px;
            width: 100%;
            box-shadow: 0 15px 35px rgba(0, 0, 0, 0.1);
            transition: all 0.4s ease;
            opacity: 0;
            transform: translateY(20px);
            animation: fadeIn 0.6s ease-out forwards;
        }
        @keyframes fadeIn {
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }
        select,
        input {
            width: 100%;
            padding: 15px;
            margin: 15px 0;
            background: rgba(255, 255, 255, 0.2);
            border: 1px solid rgba(255, 255, 255, 0.5);
            border-radius: 10px;
            color: #333;
            font-size: 16px;
            transition: all 0.3s ease;
            backdrop-filter: blur(5px);
        }
        input:focus, select:focus {
            outline: none;
            box-shadow: 0 0 0 3px rgba(255, 255, 255, 0.3);
            transform: scale(1.02);
        }
        button {
            width: 100%;
            padding: 15px;
            margin: 15px 0;
            background: rgba(255, 255, 255, 0.3);
            color: var(--text-color);
            border: none;
            border-radius: 10px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1.5px;
            cursor: pointer;
            transition: all 0.3s ease;
            position: relative;
            overflow: hidden;
        }
        button::after {
            content: '';
            position: absolute;
            top: 50%;
            left: 50%;
            width: 5px;
            height: 5px;
            background: rgba(255, 255, 255, 0.5);
            opacity: 0;
            border-radius: 50%;
            transform: translate(-50%, -50%) scale(1);
            animation: ripple 0.6s ease-out;
        }
        @keyframes ripple {
            from {
                transform: translate(-50%, -50%) scale(0);
                opacity: 1;
            }
            to {
                transform: translate(-50%, -50%) scale(40);
                opacity: 0;
            }
        }
        .weather-info {
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border-radius: 10px;
            padding: 25px;
            margin-top: 20px;
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }
        .weather-info > div {
            background: rgba(0, 0, 0, 0.1);
            padding: 15px;
            border-radius: 8px;
            text-align: center;
            transition: all 0.3s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
        }
        .weather-info > div:hover {
            transform: translateY(-5px) scale(1.02);
            box-shadow: 0 8px 15px rgba(0, 0, 0, 0.2);
        }
        .weather-info span {
            display: inline-block;
            transition: all 0.3s ease;
        }
        .value-update {
            animation: valuePulse 0.8s ease;
        }
        @keyframes valuePulse {
            0% { transform: scale(1); }
            50% { transform: scale(1.1); }
            100% { transform: scale(1); }
        }
        select.loading {
            position: relative;
            color: transparent;
        }
        select.loading::after {
            content: "";
            position: absolute;
            top: 50%;
            left: 50%;
            width: 20px;
            height: 20px;
            border: 3px solid rgba(255, 255, 255, 0.3);
            border-top-color: white;
            border-radius: 50%;
            animation: spin 1s linear infinite;
        }
        @keyframes spin {
            0% { transform: translate(-50%, -50%) rotate(0deg); }
            100% { transform: translate(-50%, -50%) rotate(360deg); }
        }
        @media (max-width: 600px) {
            .container {
                padding: 20px;
                margin: 10px;
            }
        }
        * {
            will-change: transform, opacity;
            backface-visibility: hidden;
        }
    </style>
    </head>
    <body>
        <div class="container">
            <h2>Weather Station Control</h2>
            <div>
                <label for="country">Chọn quốc gia:</label>
                <select id="country" onchange="handleCountryChange()">
                    <option value="">--Chọn quốc gia--</option>
                </select>
            </div>
            <div>
                <label for="city">Chọn thành phố:</label>
                <select id="city">
                    <option value="">--Vui lòng chọn quốc gia trước--</option>
                </select>
            </div>
            <div>
                <label for="cityInput">Hoặc nhập tên thành phố:</label>
                <input type="text" id="cityInput" placeholder="Nhập tên thành phố nếu không có trong danh sách">
            </div>
            <button onclick="updateLocation()">Cập nhật địa điểm</button>
            <div id="weatherInfo" class="weather-info">
                <div>Địa điểm đã chọn: <span id="selectedLocation">--</span></div>
                <div>Nhiệt độ: <span id="temperature">--</span>°C</div>
                <div>Độ ẩm: <span id="humidity">--</span>%</div>
                <div>Cập nhật lần cuối: <span id="lastUpdate">--</span></div>
            </div>
        </div>
        <script>
            window.onload = async function() {
                try {
                    const response = await fetch('https://restcountries.com/v3.1/all');
                    const countries = await response.json();
                    const countrySelect = document.getElementById('country');
                    countries
                        .sort((a, b) => a.name.common.localeCompare(b.name.common))
                        .forEach(country => {
                            const option = document.createElement('option');
                            option.value = country.cca2;
                            option.textContent = country.name.common;
                            countrySelect.appendChild(option);
                        });
                    countrySelect.value = 'VN';
                    handleCountryChange();
                    updateWeatherDisplay();
                } catch (error) {
                    console.error('Lỗi:', error);
                }
            };
            async function handleCountryChange() {
                const country = document.getElementById('country').value;
                const citySelect = document.getElementById('city');
                citySelect.innerHTML = '<option value="">Đang tải danh sách thành phố...</option>';
                if (!country) {
                    citySelect.innerHTML = '<option value="">--Vui lòng chọn quốc gia trước--</option>';
                    return;
                }
                try {
                    const username = 'conmeodit';
                    const response = await fetch(`http://api.geonames.org/searchJSON?country=${country}&featureClass=P&cities=cities1000&maxRows=1000&username=${username}`);
                    if (!response.ok) {
                        throw new Error(`Lỗi HTTP: ${response.status}`);
                    }
                    const data = await response.json();
                    if (!data.geonames || data.geonames.length === 0) {
                        throw new Error('Không tìm thấy thành phố nào cho quốc gia này');
                    }
                    citySelect.innerHTML = '<option value="">--Chọn thành phố--</option>';
                    data.geonames
                        .sort((a, b) => a.name.localeCompare(b.name))
                        .forEach(city => {
                            const option = document.createElement('option');
                            option.value = city.name;
                            option.textContent = city.name + (city.adminName1 ? `, ${city.adminName1}` : '');
                            citySelect.appendChild(option);
                        });
                } catch (error) {
                    console.error('Lỗi khi lấy danh sách thành phố:', error);
                    citySelect.innerHTML = `<option value="">Lỗi: ${error.message}</option>`;
                }
            }
            function updateLocation() {
                const country = document.getElementById('country').value;
                const selectedCity = document.getElementById('city').value;
                const manualCity = document.getElementById('cityInput').value.trim();
                const cityName = manualCity ? manualCity : selectedCity;
                if (!country || !cityName) {
                    alert('Vui lòng chọn quốc gia và nhập hoặc chọn thành phố.');
                    return;
                }
                const selectedCountry = document.getElementById('country').options[document.getElementById('country').selectedIndex].text;
                document.getElementById('selectedLocation').textContent = `${cityName}, ${selectedCountry}`;
                fetch('/update-location', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ city: cityName, country: country })
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('Cập nhật địa điểm thành công');
                        updateWeatherDisplay();
                    } else {
                        alert('Lỗi khi cập nhật địa điểm');
                    }
                })
                .catch(error => {
                    console.error('Lỗi:', error);
                    alert('Lỗi khi cập nhật địa điểm');
                });
            }
            function updateWeatherDisplay() {
                fetch('/weather-data')
                .then(response => response.json())
                .then(data => {
                    document.getElementById('temperature').textContent = data.temperature;
                    document.getElementById('humidity').textContent = data.humidity;
                    document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
                    const temp = parseFloat(data.temperature);
                    if (temp > 25) {
                        document.body.style.background = "linear-gradient(135deg, #FF4500, #FFA500)";
                    } else if (temp >= 15 && temp <= 25) {
                        document.body.style.background = "linear-gradient(135deg, #ADD8E6, #87CEFA)";
                    } else {
                        document.body.style.background = "linear-gradient(135deg, #00008B, #0000CD)";
                    }
                })
                .catch(error => console.error('Lỗi:', error));
            }
            setInterval(updateWeatherDisplay, 300000);
        </script>
    </body>
    </html>
)rawliteral";

void handleCORS() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

void handleRoot() {
    handleCORS();
    server.send(200, "text/html", index_html);
}

void handleWeatherData() {
    handleCORS();
    String json = "{";
    json += "\"temperature\":" + String(currentWeather.temperature) + ",";
    json += "\"humidity\":" + String(currentWeather.humidity) + ",";
    json += "\"description\":\"" + currentWeather.description + "\",";
    json += "\"icon\":\"" + currentWeather.icon + "\"";
    json += "}";
    server.send(200, "application/json", json);
}

void handleUpdateLocation() {
    handleCORS();
    if (server.method() == HTTP_OPTIONS) {
        server.send(200);
        return;
    }
    if (server.hasArg("plain")) {
        String body = server.arg("plain");
        StaticJsonDocument<200> doc;
        DeserializationError error = deserializeJson(doc, body);
        if (!error) {
            city = doc["city"].as<String>();
            countryCode = doc["country"].as<String>();
            getWeatherData();
            server.send(200, "application/json", "{\"success\":true}");
        } else {
            server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        }
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data received\"}");
    }
}

void setupServer() {
    server.on("/", HTTP_GET, handleRoot);
    server.on("/weather-data", HTTP_GET, handleWeatherData);
    server.on("/update-location", HTTP_POST, handleUpdateLocation);
    server.on("/update-location", HTTP_OPTIONS, []() {
        handleCORS();
        server.send(200);
    });
    server.begin();
}

void setup() {
    Serial.begin(115200);
    Wire.begin(SDA_PIN, SCL_PIN);
    WiFi.begin(ssid, password);
    int timeout = 30;
    while (WiFi.status() != WL_CONNECTED && timeout > 0) {
        delay(1000);
        Serial.print(".");
        timeout--;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nWiFi Connected!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("\nFailed to connect to WiFi!");
        return;
    }
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("SSD1306 allocation failed");
        for(;;);
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    timeClient.begin();
    setupServer();
    getWeatherData();
}

void drawAnalogClock(int cx, int cy, int r, int h, int m, int s) {
  display.drawCircle(cx, cy, r, WHITE);
  for (int i = 1; i <= 12; i++) {
    float angle = (i * 30 - 90) * PI / 180;
    int offset = 7.5;
    int x = cx + (r - offset) * cos(angle);
    int y = cy + (r - offset) * sin(angle);
    display.setTextSize(1);
    display.setCursor(x - 2, y - 3);
    display.print(i);
  }
  float hAngle = (h % 12 * 30) + (m * 0.5);
  float radH = hAngle * PI / 180;
  display.drawLine(cx, cy, cx + r * 0.5 * sin(radH), cy - r * 0.5 * cos(radH), WHITE);
  float mAngle = m * 6 + (s * 0.1);
  float radM = mAngle * PI / 180;
  display.drawLine(cx, cy, cx + r * 0.7 * sin(radM), cy - r * 0.7 * cos(radM), WHITE);
  float sAngle = s * 6;
  float radS = sAngle * PI / 180;
  display.drawLine(cx, cy, cx + r * 0.9 * sin(radS), cy - r * 0.9 * cos(radS), WHITE);
}

String urlEncode(const char* msg) {
  const char *hex = "0123456789ABCDEF";
  String encodedMsg = "";
  while (*msg) {
    char c = *msg;
    if (isalnum(c)) {
      encodedMsg += c;
    } else if (c == ' ') {
      encodedMsg += "%20";
    } else {
      encodedMsg += '%';
      encodedMsg += hex[(c >> 4) & 0x0F];
      encodedMsg += hex[c & 0x0F];
    }
    msg++;
  }
  return encodedMsg;
}

void getWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    WiFiClient client;
    HTTPClient http;
    String encodedCity = urlEncode(city.c_str());
    Serial.println("City sau khi URL encode: " + encodedCity);
    String serverPath = "http://api.weatherapi.com/v1/current.json?key=" + weatherApiKey + "&q=" + encodedCity + "&aqi=no";
    Serial.println("URL WeatherAPI: " + serverPath);
    http.begin(client, serverPath);
    int httpResponseCode = http.GET();
    if (httpResponseCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.println("Payload: " + payload);
      StaticJsonDocument<1024> doc;
      DeserializationError error = deserializeJson(doc, payload);
      if (!error) {
        currentWeather.temperature = doc["current"]["temp_c"].as<float>();
        currentWeather.humidity = doc["current"]["humidity"].as<int>();
        currentWeather.description = doc["current"]["condition"]["text"].as<String>();
        currentWeather.icon = doc["current"]["condition"]["icon"].as<String>();
        Serial.println("Weather updated successfully");
      } else {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.print("HTTP error code: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  }
}

void loop() {
    server.handleClient();
    timeClient.update();
    if (millis() - lastWeatherUpdate > weatherUpdateInterval) {
        getWeatherData();
        lastWeatherUpdate = millis();
    }
    if (timeClient.getEpochTime() == 0) {
        Serial.println("NTP time sync failed!");
        return;
    }
    int h = timeClient.getHours();
    int m = timeClient.getMinutes();
    int s = timeClient.getSeconds();
    String currentTime = timeClient.getFormattedTime();
    display.clearDisplay();
    drawAnalogClock(28, 32, 28.5, h, m, s);
    display.setTextSize(0.8);
    display.setCursor(60, 10);
    display.print(currentTime);
    display.setCursor(60, 20);
    display.print("City: ");
    display.print(String(city));
    display.setCursor(60, 30);
    display.print("Temp: ");
    display.print(String(currentWeather.temperature, 1));
    display.print("C");
    display.setCursor(60, 40);
    display.print("Hum: ");
    display.print(currentWeather.humidity);
    display.print("%");
    display.display();
    delay(1000);
}
