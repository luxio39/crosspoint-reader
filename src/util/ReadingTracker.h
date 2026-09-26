#pragma once

#include <stdint.h>
#include <time.h>

enum class TrackingEvent : uint8_t {
  BOOK_OPEN = 0,
  PROGRESS_UPDATE = 1,
  BOOK_CLOSE = 2,
  SLEEP_BUTTON = 3,
  SLEEP_TIMEOUT = 4
};

struct TrackingLogEntry {
  time_t timestamp;
  TrackingEvent eventType;
  uint32_t spineIndex;
  uint32_t chapterPageNumber;
  uint32_t chapterTotalPages;
  uint32_t spineOffset;
  char bookPath[128];
};

class ReadingTracker {
 public:
  static void init();
  static void logEvent(TrackingEvent event, uint32_t spineIndex = 0, uint32_t chapterPageNumber = 0,
                       uint32_t chapterTotalPages = 0, uint32_t spineOffset = 0, const char* bookPath = nullptr);

 private:
  static void flushToSD();

  static constexpr uint8_t BUFFER_SIZE = 20;
  static constexpr uint8_t FLUSH_THRESHOLD = 10;
  static TrackingLogEntry eventBuffer[BUFFER_SIZE];

  static uint8_t bufferCount;
  static char currentBookPath[128];
};