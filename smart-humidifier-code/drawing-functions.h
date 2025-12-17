#pragma once

void drawBluetoothIcon(int x, int y, uint16_t color) {
  tft.fillRect(x + 6, y + 1, 2, 14, color);
  tft.drawLine(x + 7, y + 1, x + 13, y + 5, color);
  tft.drawLine(x + 7, y + 1, x + 7, y + 7, color);
  tft.drawLine(x + 7, y + 7, x + 13, y + 5, color);
  tft.drawLine(x + 7, y + 8, x + 13, y + 10, color);
  tft.drawLine(x + 7, y + 8, x + 7, y + 14, color);
  tft.drawLine(x + 7, y + 14, x + 13, y + 10, color);
  tft.drawLine(x + 1, y + 11, x + 7, y + 4, color);
  tft.drawLine(x + 2, y + 11, x + 8, y + 4, color);
  tft.drawLine(x + 1, y + 4, x + 7, y + 11, color);
  tft.drawLine(x + 2, y + 4, x + 8, y + 11, color);
}

void drawTemperatureIcon(int x, int y, uint16_t color) {
  tft.fillCircle(x, y + 18, 7, color);
  tft.fillRect(x - 3, y, 6, 18, color);
  tft.fillRect(x - 1, y + 4, 2, 14, ST77XX_RED);
  tft.fillCircle(x, y + 18, 4, ST77XX_RED);
}

void drawWaterDropIcon(int x, int y, uint16_t color) {
  tft.fillCircle(x, y + 15, 6, color);
  tft.fillTriangle(x, y, x - 6, y + 15, x + 6, y + 15, color);
}

void drawModeIcon(int x, int y, uint16_t color) {
  tft.drawCircle(x, y, 7, color);
  tft.fillCircle(x, y, 2, color);
  tft.drawLine(x - 10, y, x - 7, y, color);
  tft.drawLine(x + 7, y, x + 10, y, color);
  tft.drawLine(x, y - 10, x, y - 7, color);
  tft.drawLine(x, y + 7, x, y + 10, color);
  tft.drawLine(x - 7, y - 7, x - 10, y - 10, color);
  tft.drawLine(x + 7, y + 7, x + 10, y + 10, color);
  tft.drawLine(x + 7, y - 7, x + 10, y - 10, color);
  tft.drawLine(x - 7, y + 7, x - 10, y + 10, color);
}

void drawTargetIcon(int x, int y, uint16_t color) {
  tft.drawCircle(x, y, 7, color);
  tft.drawCircle(x, y, 4, color);
  tft.fillCircle(x, y, 2, color);
  tft.drawLine(x - 10, y, x - 8, y, color);
  tft.drawLine(x + 8, y, x + 10, y, color);
  tft.drawLine(x, y - 10, x, y - 8, color);
  tft.drawLine(x, y + 8, x, y + 10, color);
}

void showSplashScreen() {
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextSize(2);

  int16_t x1, y1;
  uint16_t w1, h1;
  tft.getTextBounds("Smart", 0, 0, &x1, &y1, &w1, &h1);
  int xPos1 = (160 - w1) / 2;
  int yPos1 = (128 / 2) - h1 - 10;

  tft.setCursor(xPos1, yPos1);
  tft.setTextColor(ST77XX_CYAN);
  tft.println("Smart");

  tft.getTextBounds("Humidifier", 0, 0, &x1, &y1, &w1, &h1);
  int xPos2 = (160 - w1) / 2;
  int yPos2 = yPos1 + h1 + 5;

  tft.setCursor(xPos2, yPos2);
  tft.setTextColor(ST77XX_GREEN);
  tft.println("Humidifier");

  delay(2000);
}