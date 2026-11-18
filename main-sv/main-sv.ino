
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <LittleFS.h>


const char* tenWiFi = "Tên Cr";
const char* matKhau = "12345678";

IPAddress ip(192, 168, 4, 1);
IPAddress gateway(192, 168, 4, 1);
IPAddress subnet(255, 255, 255, 0);


DNSServer dnsServer;
WebServer server(80);


String loaiFile(String path) {
  if (path.endsWith(".html")) return "text/html";
  if (path.endsWith(".jpg")) return "image/jpeg";
  if (path.endsWith(".png")) return "image/png";
  if (path.endsWith(".mp4")) return "video/mp4";

  return "text/plain";
}


void phatVideo() {

  File file = LittleFS.open("/video.mp4", "r");

  if (!file) {
    server.send(404, "text/plain", "Khong tim thay video!");
    return;
  }

  size_t fileSize = file.size();

  if (fileSize == 0) {
    file.close();
    server.send(500, "text/plain", "Video rong!");
    return;
  }

  size_t start = 0;
  size_t end = fileSize - 1;

  bool partial = false;
  bool rangeLoi = false;


  String range = server.header("Range");

  if (range.startsWith("bytes=")) {

    String phan = range.substring(6);

    int gach = phan.indexOf('-');

    if (gach < 0 || phan.indexOf(',') >= 0) {

      rangeLoi = true;

    } else {

      String dau = phan.substring(0, gach);
      String cuoi = phan.substring(gach + 1);

      if (dau.length() == 0) {

        size_t suffix = cuoi.toInt();

        if (suffix == 0) {
          rangeLoi = true;
        } else {
          start = suffix >= fileSize ? 0 : fileSize - suffix;
        }

      } else {

        start = strtoull(dau.c_str(), nullptr, 10);

        if (cuoi.length() > 0) {
          end = strtoull(cuoi.c_str(), nullptr, 10);
        }
      }

      if (start >= fileSize || end < start) {
        rangeLoi = true;
      }

      if (end >= fileSize) {
        end = fileSize - 1;
      }

      partial = !rangeLoi;
    }
  }

 
  if (rangeLoi) {

    server.sendHeader(
      "Content-Range",
      "bytes */" + String(fileSize)
    );

    server.sendHeader("Accept-Ranges", "bytes");

    server.send(416, "text/plain", "");

    file.close();
    return;
  }

  size_t contentLength = end - start + 1;

 
  if (!file.seek(start)) {
    file.close();
    server.send(500, "text/plain", "Loi doc video!");
    return;
  }

 
  server.sendHeader("Accept-Ranges", "bytes");

  server.sendHeader(
    "Cache-Control",
    "public, max-age=3600"
  );

  if (partial) {

    String contentRange =
      "bytes " + String(start) +
      "-" + String(end) +
      "/" + String(fileSize);

    server.sendHeader("Content-Range", contentRange);
  }

 
  server.setContentLength(contentLength);

  server.send(
    partial ? 206 : 200,
    "video/mp4",
    ""
  );

 
  server.setContentLength(CONTENT_LENGTH_NOT_SET);

 
  auto &client = server.client();

  uint8_t buffer[4096];

  size_t remaining = contentLength;

  while (remaining > 0 && client.connected()) {

    size_t canDoc = remaining > sizeof(buffer)
                    ? sizeof(buffer)
                    : remaining;

    size_t n = file.read(buffer, canDoc);

    if (n == 0) {
      break;
    }

    size_t daGui = 0;

    while (daGui < n && client.connected()) {

      size_t gui = client.write(
        buffer + daGui,
        n - daGui
      );

      if (gui == 0) {
        delay(1);
      } else {
        daGui += gui;
      }
    }

    if (daGui < n) {
      break;
    }

    remaining -= n;

    yield();
  }

  file.close();
}


void guiFile(String path) {

  if (!LittleFS.exists(path)) {
    server.send(404, "text/plain", "Khong tim thay file!");
    return;
  }

  File file = LittleFS.open(path, "r");

  server.streamFile(file, loaiFile(path));

  file.close();
}

void trangChu() {
  guiFile("/index.html");
}

void xuLyKhongTimThay() {

  String path = server.uri();

  if (LittleFS.exists(path)) {
    guiFile(path);
    return;
  }

  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}


void setup() {

  Serial.begin(115200);
  delay(500);

  
  if (!LittleFS.begin(false)) {
    Serial.println("Loi LittleFS!");
    return;
  }

  Serial.println("LittleFS OK!");

  WiFi.mode(WIFI_AP);

  WiFi.setSleep(false);

  WiFi.softAPConfig(ip, gateway, subnet);

  WiFi.softAP(tenWiFi, matKhau, 6, false, 4);

  Serial.println("WiFi da phat!");
  dnsServer.start(53, "*", ip);

  const char* headerKeys[] = {
    "Range"
  };

  server.collectHeaders(headerKeys, 1);

  server.on("/", HTTP_GET, trangChu);

  server.on("/video.mp4", HTTP_GET, phatVideo);

  server.onNotFound(xuLyKhongTimThay);

  server.begin();

  Serial.println("Captive Portal OK!");
  Serial.println("Video Range OK!");
  Serial.println("IP: 192.168.4.1");
}


void loop() {

  dnsServer.processNextRequest();

  server.handleClient();

  delay(1);
}
