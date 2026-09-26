#include "ReadingTracker.h"

#include <HalStorage.h>
#include <Logging.h>
#include <fcntl.h>
#include <string.h>

#include "HalClock.h"

const std::string STATS_FILE_PATH = "/.crosspoint/reading_sessions.csv";

TrackingLogEntry ReadingTracker::eventBuffer[BUFFER_SIZE];
uint8_t ReadingTracker::bufferCount = 0;
char ReadingTracker::currentBookPath[128] = {0};

void ReadingTracker::init() {
  if (!Storage.ready()) {
    LOG_ERR("TRK", "Storage not ready, skipping tracker init");
    return;
  }

  if (!Storage.exists(STATS_FILE_PATH.c_str())) {
    HalFile file;
    if (Storage.openFileForWrite("TRK", STATS_FILE_PATH, file)) {
      const char* header =
          "TIMESTAMP,EVENT_TYPE,BOOK_PATH,SPINE_INDEX,CHAPTER_PAGE_NUMBER,CHAPTER_TOTAL_PAGES,SPINE_OFFSET\n";
      file.write(header, strlen(header));
      file.close();
      LOG_INF("TRK", "Created new reading stats CSV at: %s", STATS_FILE_PATH.c_str());
    }
  }
}

void ReadingTracker::logEvent(TrackingEvent event, uint32_t spineIndex, uint32_t chapterPageNumber,
                              uint32_t chapterTotalPages, uint32_t spineOffset, const char* bookPath) {
  time_t timestamp;
  if (!halClock.utcTime(timestamp)) {
    timestamp = time(nullptr);
  }

  if (event == TrackingEvent::BOOK_OPEN && bookPath != nullptr) {
    strncpy(currentBookPath, bookPath, sizeof(currentBookPath) - 1);
    currentBookPath[sizeof(currentBookPath) - 1] = '\0';
  }

  if (bufferCount >= BUFFER_SIZE) {
    LOG_ERR("TRK", "Buffer full, dropping oldest reading event");
    memmove(&eventBuffer[0], &eventBuffer[1], sizeof(TrackingLogEntry) * (BUFFER_SIZE - 1));
    bufferCount = BUFFER_SIZE - 1;
  }

  TrackingLogEntry& entry = eventBuffer[bufferCount];
  entry.timestamp = timestamp;
  entry.eventType = event;
  entry.spineIndex = spineIndex;
  entry.chapterPageNumber = chapterPageNumber;
  entry.chapterTotalPages = chapterTotalPages;
  entry.spineOffset = spineOffset;

  strncpy(entry.bookPath, currentBookPath, sizeof(entry.bookPath));

  if (event == TrackingEvent::BOOK_CLOSE) {
    currentBookPath[0] = '\0';
  }

  bufferCount++;

  bool isTerminalEvent = (event == TrackingEvent::BOOK_CLOSE || event == TrackingEvent::SLEEP_BUTTON ||
                          event == TrackingEvent::SLEEP_TIMEOUT);

  if (isTerminalEvent || bufferCount >= FLUSH_THRESHOLD) {
    flushToSD();
  }
}

void ReadingTracker::flushToSD() {
  if (bufferCount == 0) return;

  if (!Storage.ready()) {
    LOG_ERR("TRK", "Cannot flush: Storage not ready. Retaining %d events.", bufferCount);
    return;
  }

  HalFile file = Storage.open(STATS_FILE_PATH.c_str(), O_WRONLY | O_CREAT | O_APPEND);
  if (!file) {
    LOG_ERR("TRK", "Could not open stats file for append: %s", STATS_FILE_PATH.c_str());
    return;
  }

  char lineBuffer[256];
  for (uint8_t i = 0; i < bufferCount; i++) {
    int len = snprintf(lineBuffer, sizeof(lineBuffer), "%lu,%u,\"%s\",%lu,%lu,%lu,%lu\n",
                       (unsigned long)eventBuffer[i].timestamp, (unsigned int)eventBuffer[i].eventType,
                       eventBuffer[i].bookPath, (unsigned long)eventBuffer[i].spineIndex,
                       (unsigned long)eventBuffer[i].chapterPageNumber, (unsigned long)eventBuffer[i].chapterTotalPages,
                       (unsigned long)eventBuffer[i].spineOffset);

    if (len > 0 && len < sizeof(lineBuffer)) {
      file.write((const uint8_t*)lineBuffer, (size_t)len);
    }
  }

  file.flush();
  file.close();
  bufferCount = 0;
}