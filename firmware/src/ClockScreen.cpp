#include "ClockScreen.h"

#include <math.h>
#include <time.h>

#include "BoardConfig.h"
#include "DisplayTheme.h"
#include "Icons.h"

namespace {
constexpr int16_t kScreenWidth = 240;
constexpr int16_t kScreenHeight = 320;
constexpr int16_t kMargin = 12;
constexpr int16_t kCardY = 92;
constexpr int16_t kCardHeight = 136;
constexpr int16_t kTimeY = 118;
constexpr int16_t kDateY = 182;
constexpr int16_t kPulseY = 60;

String twoDigits(int value) {
  char buffer[3];
  snprintf(buffer, sizeof(buffer), "%02d", value);
  return String(buffer);
}

String formatDate(const struct tm& local) {
  static const char* kDays[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
  static const char* kMonths[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                  "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
  const int weekday = local.tm_wday >= 0 && local.tm_wday < 7 ? local.tm_wday : 0;
  const int month = local.tm_mon >= 0 && local.tm_mon < 12 ? local.tm_mon : 0;
  return String(kDays[weekday]) + "  " + String(local.tm_mday) + " " +
         String(kMonths[month]);
}
}  // namespace

void ClockScreen::reset() {
  chromeDrawn_ = false;
  lastTime_ = "";
  lastDate_ = "";
  lastMeridiem_ = "";
}

void ClockScreen::drawChrome(TFT_eSPI& display, uint16_t accent) {
  theme::verticalGradient(display, 0, 0, kScreenWidth, kScreenHeight,
                          theme::tone(theme::kBackdrop), theme::tone(theme::kInk));

  const char* badge = "POWER SAVER";
  const int16_t badgeWidth = display.textWidth(badge, 2) + 46;
  const int16_t badgeX = kScreenWidth / 2 - badgeWidth / 2;
  theme::glassCard(display, badgeX, 24, badgeWidth, 26, 13,
                   theme::tone(theme::kBackdrop), accent, 0.5f);
  icons::power(display, badgeX + 19, 37, 14, accent);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(theme::tone(theme::kTextSoft));
  display.drawString(badge, badgeX + 32, 29, 2);

  theme::glassCard(display, kMargin, kCardY, kScreenWidth - kMargin * 2,
                   kCardHeight, 18, theme::tone(theme::kInk), accent, 0.25f);

  display.setTextDatum(TC_DATUM);
  display.setTextColor(theme::tone(theme::kMuted));
  display.drawString("PC IS OFF", kScreenWidth / 2, 250, 2);
  display.drawString("HOLD 2s TO WAKE IT", kScreenWidth / 2, 270, 2);
  display.setTextDatum(TL_DATUM);
  chromeDrawn_ = true;
}

void ClockScreen::drawTime(TFT_eSPI& display, uint16_t accent,
                           const String& timeText, const String& meridiem,
                           const String& dateText) {
  const int16_t inset = kMargin + 8;
  const int16_t width = kScreenWidth - inset * 2;
  theme::gradientRoundRect(display, inset, kTimeY - 6, width, 96, 12,
                           theme::mix(theme::tone(theme::kBackdrop),
                                      theme::tone(theme::kSurfaceTop), 0.55f),
                           theme::mix(theme::tone(theme::kBackdrop),
                                      theme::tone(theme::kSurfaceBottom), 0.75f));

  display.setTextDatum(TC_DATUM);
  const int16_t centerX = kScreenWidth / 2 - (meridiem.isEmpty() ? 0 : 12);
  display.setTextColor(theme::tone(theme::kBevelDark));
  display.drawString(timeText, centerX + 2, kTimeY + 2, 7);
  display.setTextColor(theme::mix(theme::tone(theme::kInk), accent, 0.5f));
  display.drawString(timeText, centerX - 1, kTimeY, 7);
  display.drawString(timeText, centerX + 1, kTimeY, 7);
  display.setTextColor(theme::tone(theme::kText));
  display.drawString(timeText, centerX, kTimeY, 7);

  if (!meridiem.isEmpty()) {
    display.setTextDatum(TL_DATUM);
    display.setTextColor(accent);
    display.drawString(meridiem, centerX + display.textWidth(timeText, 7) / 2 + 6,
                       kTimeY + 6, 2);
    display.setTextDatum(TC_DATUM);
  }

  display.setTextColor(theme::tone(theme::kTextSoft));
  display.drawString(dateText, kScreenWidth / 2, kDateY, 2);
  display.setTextDatum(TL_DATUM);
}

void ClockScreen::render(TFT_eSPI& display, uint32_t now, uint16_t accent,
                         bool timeValid) {
  if (!chromeDrawn_) drawChrome(display, accent);

  String timeText = "--:--";
  String meridiem;
  String dateText = "WAITING FOR TIME";
  if (timeValid) {
    const time_t utc = time(nullptr);
    struct tm local;
    localtime_r(&utc, &local);
    int hour = local.tm_hour;
    if (!BoardConfig::kUse24HourClock) {
      meridiem = hour < 12 ? "AM" : "PM";
      hour = hour % 12;
      if (hour == 0) hour = 12;
    }
    timeText = twoDigits(hour) + ":" + twoDigits(local.tm_min);
    dateText = formatDate(local);
  }

  if (timeText != lastTime_ || dateText != lastDate_ ||
      meridiem != lastMeridiem_) {
    drawTime(display, accent, timeText, meridiem, dateText);
    lastTime_ = timeText;
    lastDate_ = dateText;
    lastMeridiem_ = meridiem;
  }

  // Slow breathing dot: proof the device is awake without redrawing anything
  // expensive.
  const float breathe =
      (sinf(static_cast<float>(now % 3400) / 3400.0f * 6.2831853f) + 1.0f) * 0.5f;
  display.fillCircle(kScreenWidth / 2, kPulseY, 4,
                     theme::mix(theme::tone(theme::kInk), accent,
                                0.25f + breathe * 0.75f));
}
