// Khai báo các thư viện cần thiết
#include <Wire.h>             // Thư viện giao tiếp I2C
#include <Adafruit_GFX.h>     // Thư viện đồ họa cho màn hình OLED
#include <Adafruit_SSD1306.h> // Thư viện điều khiển màn hình OLED SSD1306
#include <WiFi.h>             // Thư viện kết nối WiFi
#include <NTPClient.h>        // Thư viện đồng bộ thời gian qua mạng
#include <WiFiUdp.h>          // Thư viện UDP cho NTP
#include <HTTPClient.h>       // Thư viện HTTP client
#include <ArduinoJson.h>      // Thư viện xử lý JSON
#include <WiFiClient.h>       // Thư viện client WiFi
#include <WebServer.h>        // Thư viện web server

// Thông tin WiFi
const char *ssid = "YOUR_SSID";         // Tên mạng WiFi
const char *password = "YOUR_PASSWORD"; // Mật khẩu WiFi

// Cấu hình màn hình OLED
#define SCREEN_WIDTH 128 // Chiều rộng màn hình 128 pixel
#define SCREEN_HEIGHT 64 // Chiều cao màn hình 64 pixel

// Định nghĩa chân I2C
#define SDA_PIN 21 // Chân SDA của I2C
#define SCL_PIN 22 // Chân SCL của I2C

// Biến lưu thời gian cập nhật dữ liệu thời tiết và khoảng thời gian cập nhật (5 phút)
unsigned long lastWeatherUpdate = 0;
const unsigned long weatherUpdateInterval = 300000;

// Cấu hình API key thời tiết và thông tin địa điểm ban đầu
const String weatherApiKey = "78ddca507b7f48bea8e72512250702";
String city = "Hanoi";     // changeable
String countryCode = "VN"; // changeable

// Khởi tạo đối tượng hiển thị OLED và máy chủ web (chạy ở cổng 80)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
WebServer server(80);

// Khởi tạo đối tượng NTP để đồng bộ thời gian từ pool.ntp.org với múi giờ GMT+7 (7*3600 giây) và cập nhật mỗi 60 giây
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 7 * 3600, 60000);

// Định nghĩa cấu trúc lưu trữ dữ liệu thời tiết
struct WeatherData
{
    float temperature;
    int humidity;
    String description;
    String icon;
} currentWeather;

// Trang HTML được lưu trong bộ nhớ flash (PROGMEM)
const char index_html[] PROGMEM = R"rawliteral(
    <!DOCTYPE html>
    <html lang="en">
    <head>
        <meta charset="UTF-8">
        <title>Weather Station Control</title>
        <style>
        /* Định nghĩa các biến CSS toàn cục để dễ dàng thay đổi màu sắc */
        :root {
            --text-color: #ffffff;
            --background-gradient-start: #87ceeb;
            --background-gradient-end: #4682b4;
        }

        /* Thiết lập style cho phần body của trang */
        body {
            font-family: "Inter", Arial, sans-serif;
            max-width: 800px;
            margin: 0 auto; /* Căn giữa container */
            padding: 20px;
            /* Tạo gradient động cho background */
            background: linear-gradient(135deg, var(--background-gradient-start), var(--background-gradient-end));
            color: var(--text-color);
            min-height: 100vh;
            /* Sử dụng flexbox để căn giữa nội dung */
            display: flex;
            justify-content: center;
            align-items: center;
            /* Thêm hiệu ứng chuyển động cho background */
            transition: all 0.5s ease;
            background-size: 400% 400%;
            animation: gradientFlow 15s ease infinite;
        }

        /* Định nghĩa animation cho hiệu ứng gradient chảy */
        @keyframes gradientFlow {
            0% { background-position: 0% 50%; }
            50% { background-position: 100% 50%; }
            100% { background-position: 0% 50%; }
        }

        /* Container chính với hiệu ứng kính mờ (glassmorphism) */
        .container {
            background: rgba(255, 255, 255, 0.2);
            backdrop-filter: blur(10px); /* Tạo hiệu ứng kính mờ */
            border-radius: 16px;
            border: 1px solid rgba(255, 255, 255, 0.3);
            padding: 40px;
            width: 100%;
            box-shadow: 0 15px 35px rgba(0, 0, 0, 0.1);
            /* Hiệu ứng xuất hiện từ từ */
            transition: all 0.4s ease;
            opacity: 0;
            transform: translateY(20px);
            animation: fadeIn 0.6s ease-out forwards;
        }

        /* Animation cho hiệu ứng xuất hiện */
        @keyframes fadeIn {
            to {
                opacity: 1;
                transform: translateY(0);
            }
        }

        /* Style cho các phần tử input và select */
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

        /* Hiệu ứng khi focus vào input/select */
        input:focus, select:focus {
            outline: none;
            box-shadow: 0 0 0 3px rgba(255, 255, 255, 0.3);
            transform: scale(1.02);
        }

        /* Style cho nút bấm */
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

        /* Hiệu ứng gợn sóng khi click button */
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

        /* Animation cho hiệu ứng gợn sóng */
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

        /* Style cho phần hiển thị thông tin thời tiết */
        .weather-info {
            background: rgba(255, 255, 255, 0.1);
            backdrop-filter: blur(10px);
            border-radius: 10px;
            padding: 25px;
            margin-top: 20px;
            /* Sử dụng CSS Grid để layout responsive */
            display: grid;
            grid-template-columns: repeat(auto-fit, minmax(200px, 1fr));
            gap: 15px;
        }

        /* Style cho từng ô thông tin thời tiết */
        .weather-info > div {
            background: rgba(0, 0, 0, 0.1);
            padding: 15px;
            border-radius: 8px;
            text-align: center;
            transition: all 0.3s cubic-bezier(0.25, 0.46, 0.45, 0.94);
            box-shadow: 0 2px 5px rgba(0, 0, 0, 0.1);
        }

        /* Hiệu ứng hover cho ô thông tin */
        .weather-info > div:hover {
            transform: translateY(-5px) scale(1.02);
            box-shadow: 0 8px 15px rgba(0, 0, 0, 0.2);
        }

        /* Hiệu ứng cho các giá trị thời tiết */
        .weather-info span {
            display: inline-block;
            transition: all 0.3s ease;
        }

        /* Animation khi giá trị được cập nhật */
        .value-update {
            animation: valuePulse 0.8s ease;
        }

        @keyframes valuePulse {
            0% { transform: scale(1); }
            50% { transform: scale(1.1); }
            100% { transform: scale(1); }
        }

        /* Style cho trạng thái loading của select */
        select.loading {
            position: relative;
            color: transparent;
        }

        /* Hiệu ứng loading spinner */
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

        /* Animation cho loading spinner */
        @keyframes spin {
            0% { transform: translate(-50%, -50%) rotate(0deg); }
            100% { transform: translate(-50%, -50%) rotate(360deg); }
        }

        /* Responsive design cho màn hình nhỏ */
        @media (max-width: 600px) {
            .container {
                padding: 20px;
                margin: 10px;
            }
        }

        /* Tối ưu hiệu năng rendering */
        * {
            will-change: transform, opacity;
            backface-visibility: hidden;
        }
    </style>
    </head>
    <body>
        <div class="container">
            <h2>Weather Station Control</h2>
    
            <!-- Phần chọn quốc gia -->
            <div>
                <label for="country">Chọn quốc gia:</label>
                <select id="country" onchange="handleCountryChange()">
                    <option value="">--Chọn quốc gia--</option>
                </select>
            </div>
    
            <!-- Phần chọn thành phố -->
            <div>
                <label for="city">Chọn thành phố:</label>
                <select id="city">
                    <option value="">--Vui lòng chọn quốc gia trước--</option>
                </select>
            </div>
    
            <!-- Input thủ công cho tên thành phố -->
            <div>
                <label for="cityInput">Hoặc nhập tên thành phố:</label>
                <input type="text" id="cityInput" placeholder="Nhập tên thành phố nếu không có trong danh sách">
            </div>
    
            <button onclick="updateLocation()">Cập nhật địa điểm</button>
    
            <!-- Hiển thị thông tin thời tiết -->
            <div id="weatherInfo" class="weather-info">
                <div>Địa điểm đã chọn: <span id="selectedLocation">--</span></div>
                <div>Nhiệt độ: <span id="temperature">--</span>°C</div>
                <div>Độ ẩm: <span id="humidity">--</span>%</div>
                <div>Cập nhật lần cuối: <span id="lastUpdate">--</span></div>
            </div>
        </div>
    
        <script>
            // Khởi tạo trang web khi tải xong
            window.onload = async function() {
                try {
                    // Lấy danh sách quốc gia từ API RestCountries
                    const response = await fetch('https://restcountries.com/v3.1/all');
                    const countries = await response.json();
                    
                    const countrySelect = document.getElementById('country');
                    // Sắp xếp và thêm các quốc gia vào dropdown
                    countries
                        .sort((a, b) => a.name.common.localeCompare(b.name.common))
                        .forEach(country => {
                            const option = document.createElement('option');
                            option.value = country.cca2;
                            option.textContent = country.name.common;
                            countrySelect.appendChild(option);
                        });
                    
                    // Thiết lập Việt Nam là quốc gia mặc định
                    countrySelect.value = 'VN';
                    // Tải danh sách thành phố của Việt Nam
                    handleCountryChange();
                    // Cập nhật thông tin thời tiết ban đầu
                    updateWeatherDisplay();
                } catch (error) {
                    console.error('Lỗi:', error);
                }
            };
    
            // Xử lý sự kiện khi người dùng chọn quốc gia
            async function handleCountryChange() {
                const country = document.getElementById('country').value;
                const citySelect = document.getElementById('city');
                citySelect.innerHTML = '<option value="">Đang tải danh sách thành phố...</option>';
    
                if (!country) {
                    citySelect.innerHTML = '<option value="">--Vui lòng chọn quốc gia trước--</option>';
                    return;
                }
    
                try {
                    // Sử dụng API GeoNames để lấy danh sách thành phố
                    const username = 'conmeodit';
                    const response = await fetch(`http://api.geonames.org/searchJSON?country=${country}&featureClass=P&cities=cities1000&maxRows=1000&username=${username}`);
    
                    if (!response.ok) {
                        throw new Error(`Lỗi HTTP: ${response.status}`);
                    }
    
                    const data = await response.json();
    
                    if (!data.geonames || data.geonames.length === 0) {
                        throw new Error('Không tìm thấy thành phố nào cho quốc gia này');
                    }
    
                    // Cập nhật dropdown thành phố
                    citySelect.innerHTML = '<option value="">--Chọn thành phố--</option>';
    
                    // Sắp xếp và thêm các thành phố vào dropdown
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
    
            // Hàm gửi yêu cầu cập nhật địa điểm đến máy chủ
            function updateLocation() {
                // Lấy giá trị của quốc gia và thành phố từ các trường input
                const country = document.getElementById('country').value;
                const selectedCity = document.getElementById('city').value;
                const manualCity = document.getElementById('cityInput').value.trim();
                // Ưu tiên sử dụng tên thành phố được nhập tay nếu có, ngược lại sử dụng thành phố được chọn
                const cityName = manualCity ? manualCity : selectedCity;
    
                // Nếu chưa chọn quốc gia hoặc chưa nhập/chọn thành phố, hiển thị cảnh báo
                if (!country || !cityName) {
                    alert('Vui lòng chọn quốc gia và nhập hoặc chọn thành phố.');
                    return;
                }
    
                // Lấy tên quốc gia để hiển thị thông tin
                const selectedCountry = document.getElementById('country').options[document.getElementById('country').selectedIndex].text;
                document.getElementById('selectedLocation').textContent = `${cityName}, ${selectedCountry}`;
    
                // Gửi yêu cầu POST đến endpoint '/update-location' với dữ liệu địa điểm (thành phố và quốc gia)
                fetch('/update-location', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ city: cityName, country: country })
                })
                .then(response => response.json())
                .then(data => {
                    if (data.success) {
                        alert('Cập nhật địa điểm thành công');
                        // Sau khi cập nhật thành công, lấy lại dữ liệu thời tiết mới
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
    
            // Hàm lấy dữ liệu thời tiết từ máy chủ và cập nhật giao diện hiển thị
            function updateWeatherDisplay() {
                fetch('/weather-data')
                .then(response => response.json())
                .then(data => {
                    // Cập nhật thông tin nhiệt độ, độ ẩm và thời gian cập nhật
                    document.getElementById('temperature').textContent = data.temperature;
                    document.getElementById('humidity').textContent = data.humidity;
                    document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
    
                    // Chuyển đổi nhiệt độ sang kiểu số thực để so sánh
                    const temp = parseFloat(data.temperature);
    
                    /* 
                     * Dựa trên giá trị nhiệt độ, thay đổi giao diện nền của trang:
                     * - Nếu nhiệt độ > 25°C: sử dụng gradient màu đỏ và cam.
                     * - Nếu nhiệt độ từ 15°C đến 25°C: sử dụng gradient màu xanh nhạt.
                     * - Nếu nhiệt độ < 15°C: sử dụng gradient màu xanh đậm.
                     */
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
    
            // Tự động cập nhật dữ liệu thời tiết mỗi 5 phút (300000 mili giây)
            setInterval(updateWeatherDisplay, 300000);
        </script>
    </body>
    </html>
    )rawliteral";

// Hàm thiết lập các header CORS cho các phản hồi từ máy chủ
void handleCORS()
{
    // Cho phép mọi nguồn (domain) truy cập tài nguyên
    server.sendHeader("Access-Control-Allow-Origin", "*");
    // Cho phép các phương thức GET, POST và OPTIONS được sử dụng
    server.sendHeader("Access-Control-Allow-Methods", "GET,POST,OPTIONS");
    // Cho phép header Content-Type trong các yêu cầu gửi lên máy chủ
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// Hàm trả về trang HTML chính khi có yêu cầu đến đường dẫn "/"
void handleRoot()
{
    // Thiết lập header CORS cho phản hồi
    handleCORS();
    // Gửi phản hồi với mã 200 (OK), kiểu nội dung là text/html và nội dung được lấy từ biến index_html
    server.send(200, "text/html", index_html);
}

// Hàm trả về dữ liệu thời tiết ở dạng JSON
void handleWeatherData()
{
    // Thiết lập header CORS cho phản hồi
    handleCORS();
    // Xây dựng chuỗi JSON chứa các thông số thời tiết
    String json = "{";
    json += "\"temperature\":" + String(currentWeather.temperature) + ",";
    json += "\"humidity\":" + String(currentWeather.humidity) + ",";
    json += "\"description\":\"" + currentWeather.description + "\",";
    json += "\"icon\":\"" + currentWeather.icon + "\"";
    json += "}";
    // Gửi phản hồi với mã 200 (OK), kiểu nội dung là application/json và nội dung là chuỗi JSON vừa tạo
    server.send(200, "application/json", json);
}

// Hàm xử lý yêu cầu cập nhật địa điểm từ client
void handleUpdateLocation()
{
    // Thiết lập header CORS cho phản hồi
    handleCORS();

    // Nếu yêu cầu sử dụng phương thức HTTP OPTIONS (cho preflight của CORS),
    // chỉ cần gửi phản hồi 200 mà không xử lý dữ liệu thêm
    if (server.method() == HTTP_OPTIONS)
    {
        server.send(200);
        return;
    }

    // Kiểm tra xem dữ liệu có được gửi dưới dạng plain text hay không
    if (server.hasArg("plain"))
    {
        // Lấy nội dung dữ liệu gửi từ client
        String body = server.arg("plain");

        // Tạo một StaticJsonDocument với dung lượng 200 byte để chứa dữ liệu JSON
        StaticJsonDocument<200> doc;
        // Phân tích cú pháp chuỗi JSON nhận được
        DeserializationError error = deserializeJson(doc, body);

        // Nếu không có lỗi trong quá trình phân tích JSON
        if (!error)
        {
            // Lấy giá trị của "city" và "country" từ JSON và cập nhật vào các biến toàn cục
            city = doc["city"].as<String>();
            countryCode = doc["country"].as<String>();

            // Gọi hàm lấy dữ liệu thời tiết mới dựa trên địa điểm đã cập nhật
            getWeatherData();

            // Gửi phản hồi với mã 200 (OK) và trả về JSON báo thành công
            server.send(200, "application/json", "{\"success\":true}");
        }
        else
        {
            // Nếu JSON không hợp lệ, gửi phản hồi với mã 400 (Bad Request)
            server.send(400, "application/json", "{\"success\":false,\"error\":\"Invalid JSON\"}");
        }
    }
    else
    {
        // Nếu không có dữ liệu nào được gửi từ client, gửi phản hồi lỗi với mã 400
        server.send(400, "application/json", "{\"success\":false,\"error\":\"No data received\"}");
    }
}

// Hàm thiết lập các đường dẫn (route) cho máy chủ web
void setupServer()
{
    // Đăng ký xử lý yêu cầu GET đến đường dẫn "/" bằng hàm handleRoot
    server.on("/", HTTP_GET, handleRoot);
    // Đăng ký xử lý yêu cầu GET đến đường dẫn "/weather-data" bằng hàm handleWeatherData
    server.on("/weather-data", HTTP_GET, handleWeatherData);
    // Đăng ký xử lý yêu cầu POST đến đường dẫn "/update-location" bằng hàm handleUpdateLocation
    server.on("/update-location", HTTP_POST, handleUpdateLocation);

    // Đăng ký xử lý yêu cầu OPTIONS (thường dùng cho preflight CORS) cho "/update-location"
    server.on("/update-location", HTTP_OPTIONS, []()
              {
                  // Thiết lập header CORS và gửi phản hồi 200 để xác nhận yêu cầu
                  handleCORS();
                  server.send(200); });

    // Khởi động máy chủ để lắng nghe các yêu cầu từ client
    server.begin();
}

// Hàm vẽ đồng hồ analog lên màn hình OLED
// cx, cy: tọa độ tâm đồng hồ; r: bán kính đồng hồ; h, m, s: giờ, phút, giây hiện tại
void drawAnalogClock(int cx, int cy, int r, int h, int m, int s)
{
    // Vẽ đường tròn làm khung của đồng hồ
    display.drawCircle(cx, cy, r, WHITE);

    // Vẽ số từ 1 đến 12 lên đồng hồ
    for (int i = 1; i <= 12; i++)
    {
        // Tính góc cho số i (mỗi số cách nhau 30°; trừ 90° để số 12 nằm ở trên cùng)
        float angle = (i * 30 - 90) * PI / 180;
        int offset = 7.5; // Khoảng cách offset từ biên đồng hồ để không vẽ sát mép
        // Tính tọa độ x, y cho số i dựa trên góc và bán kính
        int x = cx + (r - offset) * cos(angle);
        int y = cy + (r - offset) * sin(angle);
        // Đặt kích thước chữ và vị trí con trỏ
        display.setTextSize(1);
        display.setCursor(x - 2, y - 3);
        // In số lên màn hình
        display.print(i);
    }

    // Tính góc cho kim giờ
    // Mỗi giờ tương đương 30° và mỗi phút làm tăng thêm 0.5°
    float hAngle = (h % 12 * 30) + (m * 0.5);
    float radH = hAngle * PI / 180; // Chuyển đổi góc sang radian
    // Vẽ kim giờ với độ dài 0.5 lần bán kính đồng hồ
    display.drawLine(cx, cy, cx + r * 0.5 * sin(radH), cy - r * 0.5 * cos(radH), WHITE);

    // Tính góc cho kim phút
    // Mỗi phút tương đương 6° và mỗi giây làm tăng thêm 0.1°
    float mAngle = m * 6 + (s * 0.1);
    float radM = mAngle * PI / 180; // Chuyển đổi góc sang radian
    // Vẽ kim phút với độ dài 0.7 lần bán kính đồng hồ
    display.drawLine(cx, cy, cx + r * 0.7 * sin(radM), cy - r * 0.7 * cos(radM), WHITE);

    // Tính góc cho kim giây (mỗi giây ứng với 6°)
    float sAngle = s * 6;
    float radS = sAngle * PI / 180; // Chuyển đổi góc sang radian
    // Vẽ kim giây với độ dài 0.9 lần bán kính đồng hồ
    display.drawLine(cx, cy, cx + r * 0.9 * sin(radS), cy - r * 0.9 * cos(radS), WHITE);
}

// Hàm mã hóa URL: chuyển đổi các ký tự không an toàn thành dạng %XX
String urlEncode(const char *msg)
{
    // Chuỗi ký tự chứa các giá trị hex để chuyển đổi các ký tự đặc biệt
    const char *hex = "0123456789ABCDEF";
    String encodedMsg = "";
    // Duyệt qua từng ký tự trong chuỗi msg
    while (*msg)
    {
        char c = *msg;
        // Nếu ký tự là chữ hoặc số, thêm trực tiếp vào chuỗi đã mã hóa
        if (isalnum(c))
        {
            encodedMsg += c;
        }
        // Nếu ký tự là khoảng trắng, thay thế bằng "%20"
        else if (c == ' ')
        {
            encodedMsg += "%20";
        }
        // Nếu ký tự là ký tự đặc biệt, chuyển đổi thành dạng %XX
        else
        {
            encodedMsg += '%';
            encodedMsg += hex[(c >> 4) & 0x0F]; // Lấy 4 bit cao và chuyển thành ký tự hex
            encodedMsg += hex[c & 0x0F];        // Lấy 4 bit thấp và chuyển thành ký tự hex
        }
        msg++; // Chuyển sang ký tự tiếp theo
    }
    return encodedMsg;
}

// Hàm lấy dữ liệu thời tiết từ API WeatherAPI
void getWeatherData()
{
    // Kiểm tra xem thiết bị có kết nối WiFi hay không
    if (WiFi.status() == WL_CONNECTED)
    {
        WiFiClient client; // Tạo đối tượng WiFiClient để quản lý kết nối mạng
        HTTPClient http;   // Tạo đối tượng HTTPClient để gửi yêu cầu HTTP

        // Mã hóa tên thành phố để đảm bảo các ký tự đặc biệt không làm hỏng URL
        String encodedCity = urlEncode(city.c_str());
        Serial.println("City sau khi URL encode: " + encodedCity);

        // Xây dựng URL truy vấn đến API WeatherAPI với key API, tên thành phố đã mã hóa và tham số aqi=no
        String serverPath = "http://api.weatherapi.com/v1/current.json?key=" + weatherApiKey + "&q=" + encodedCity + "&aqi=no";
        Serial.println("URL WeatherAPI: " + serverPath);

        // Bắt đầu kết nối HTTP đến URL được xây dựng
        http.begin(client, serverPath);
        // Gửi yêu cầu GET và lưu mã phản hồi HTTP trả về
        int httpResponseCode = http.GET();
        if (httpResponseCode == HTTP_CODE_OK)
        {
            // Nếu yêu cầu thành công (mã 200), lấy nội dung phản hồi dưới dạng chuỗi
            String payload = http.getString();
            Serial.println("Payload: " + payload);

            // Tạo một tài liệu JSON với dung lượng 1024 byte để chứa dữ liệu trả về
            StaticJsonDocument<1024> doc;
            // Phân tích cú pháp chuỗi JSON từ payload
            DeserializationError error = deserializeJson(doc, payload);
            if (!error)
            {
                // Cập nhật các trường dữ liệu thời tiết từ JSON vào biến toàn cục currentWeather
                currentWeather.temperature = doc["current"]["temp_c"].as<float>();
                currentWeather.humidity = doc["current"]["humidity"].as<int>();
                currentWeather.description = doc["current"]["condition"]["text"].as<String>();
                currentWeather.icon = doc["current"]["condition"]["icon"].as<String>();
                Serial.println("Cập nhật thời tiết thành công");
            }
            else
            {
                // Nếu có lỗi khi phân tích JSON, in lỗi ra Serial
                Serial.print("Lỗi phân tích JSON: ");
                Serial.println(error.c_str());
            }
        }
        else
        {
            // Nếu yêu cầu không thành công, in mã lỗi HTTP ra Serial
            Serial.print("Mã lỗi HTTP: ");
            Serial.println(httpResponseCode);
        }
        // Kết thúc phiên làm việc HTTP để giải phóng tài nguyên
        http.end();
    }
}

void setup()
{
    // Khởi tạo Serial để debug
    Serial.begin(115200);

    // Khởi tạo giao tiếp I2C cho màn hình OLED
    Wire.begin(SDA_PIN, SCL_PIN);

    // Kết nối đến WiFi
    WiFi.begin(ssid, password);
    int timeout = 30;
    while (WiFi.status() != WL_CONNECTED && timeout > 0)
    {
        delay(1000);
        Serial.print(".");
        timeout--;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        Serial.println("\nKết nối WiFi thành công!");
        Serial.print("Địa chỉ IP: ");
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println("\nKhông thể kết nối WiFi!");
        return;
    }

    // Khởi tạo màn hình OLED, nếu không thành công thì dừng chương trình
    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
    {
        Serial.println("Khởi tạo SSD1306 thất bại");
        for (;;)
            ;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);

    // Bắt đầu đồng bộ thời gian với NTP
    timeClient.begin();

    // Thiết lập máy chủ web và lấy dữ liệu thời tiết ban đầu
    setupServer();
    getWeatherData();
}

void loop()
{
    // Xử lý các yêu cầu từ client đến máy chủ web
    server.handleClient();

    // Cập nhật thời gian từ máy chủ NTP
    timeClient.update();

    // Cập nhật dữ liệu thời tiết theo chu kỳ (mỗi 5 phút)
    if (millis() - lastWeatherUpdate > weatherUpdateInterval)
    {
        getWeatherData();
        lastWeatherUpdate = millis();
    }

    // Nếu chưa lấy được thời gian NTP thì dừng vòng lặp
    if (timeClient.getEpochTime() == 0)
    {
        Serial.println("Đồng bộ thời gian NTP thất bại!");
        return;
    }

    // Lấy giờ, phút, giây hiện tại
    int h = timeClient.getHours();
    int m = timeClient.getMinutes();
    int s = timeClient.getSeconds();
    String currentTime = timeClient.getFormattedTime();

    // Xóa màn hình, vẽ đồng hồ và hiển thị thông tin thời gian, địa điểm và thời tiết
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
