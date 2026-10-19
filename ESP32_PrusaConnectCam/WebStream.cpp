/**
   @file stream.cpp

   @brief stream library.

   Inspiration is from git repository https://gist.github.com/me-no-dev/d34fba51a8f059ac559bf62002e61aa3

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/
#include "WebStream.h"

#define PART_BOUNDARY "123456789000000000000987654321"                                          ///< Must be unique for each stream
static const char *STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;   ///< content type for stream
static const char *STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";                             ///< boundary for stream
static const char *STREAM_PART = "Content-Type: %s\r\nContent-Length: %u\r\n\r\n";              ///< part for stream
static const char *JPG_CONTENT_TYPE = "image/jpeg";                                           ///< content type for jpg

/**
 * @brief Construct a new Async Buffer Response:: Async Buffer Response object
 * 
 * @param buf 
 * @param len 
 * @param contentType 
 */
AsyncBufferResponse::AsyncBufferResponse(uint8_t *buf, size_t len, const char *contentType) {
  _buf = buf;
  _len = len;
  _callback = nullptr;
  _code = 200;
  _contentLength = _len;
  _contentType = contentType;
  _index = 0;
}

/**
 * @brief Destroy the Async Buffer Response:: Async Buffer Response object
 * 
 */
AsyncBufferResponse::~AsyncBufferResponse() {
  if (_buf != nullptr) {
    free(_buf);
  }
}

/**
 * @brief Check if source is valid
 * 
 * @return bool
 */
bool AsyncBufferResponse::_sourceValid() const {
  return _buf != nullptr;
}

/**
 * @brief Fill buffer
 * 
 * @param buf 
 * @param maxLen 
 * @return size_t 
 */
size_t AsyncBufferResponse::_fillBuffer(uint8_t *buf, size_t maxLen) {
  size_t ret = _content(buf, maxLen, _index);
  if (ret != RESPONSE_TRY_AGAIN) {
    _index += ret;
  }
  return ret;
}

/**
 * @brief Content
 * 
 * @param buffer 
 * @param maxLen 
 * @param index 
 * @return size_t 
 */
size_t AsyncBufferResponse::_content(uint8_t *buffer, size_t maxLen, size_t index) {
  if ((_buf == nullptr) || (index >= _len)) {
    return 0;
  }

  size_t len = min(maxLen, _len - index);
  memcpy(buffer, _buf + index, len);
  if ((index + len) == _len) {
    free(_buf);
    _buf = nullptr;
  }
  return len;
}

/**
 * @brief Construct a new Async Frame Response:: Async Frame Response object
 * 
 * @param frame 
 * @param contentType 
 */
AsyncFrameResponse::AsyncFrameResponse(camera_fb_t *frame, const char *contentType) {
  _callback = nullptr;
  _code = 200;
  _contentLength = frame->len;
  _contentType = contentType;
  _index = 0;
  fb = frame;
}

/**
 * @brief Destroy the Async Frame Response:: Async Frame Response object
 * 
 */
AsyncFrameResponse::~AsyncFrameResponse() {
  if (fb != nullptr) {
    esp_camera_fb_return(fb);
  }
}

/**
 * @brief Check if source is valid
 * 
 * @return bool
 */
bool AsyncFrameResponse::_sourceValid() const {
  return fb != nullptr;
}

/**
 * @brief Fill buffer
 * 
 * @param buf 
 * @param maxLen 
 * @return size_t 
 */
size_t AsyncFrameResponse::_fillBuffer(uint8_t *buf, size_t maxLen) {
  size_t ret = _content(buf, maxLen, _index);
  if (ret != RESPONSE_TRY_AGAIN) {
    _index += ret;
  }
  return ret;
}

/**
 * @brief Content
 * 
 * @param buffer 
 * @param maxLen 
 * @param index 
 * @return size_t 
 */
size_t AsyncFrameResponse::_content(uint8_t *buffer, size_t maxLen, size_t index) {
  if ((fb == nullptr) || (index >= fb->len)) {
    return 0;
  }

  size_t len = min(maxLen, fb->len - index);
  memcpy(buffer, fb->buf + index, len);
  if ((index + len) == fb->len) {
    esp_camera_fb_return(fb);
    fb = nullptr;
  }
  return len;
}

/**
 * @brief Construct a new Async Jpeg Stream Response:: Async Jpeg Stream Response object
 * 
 * @param i_cam 
 * @param i_log 
 */
AsyncJpegStreamResponse::AsyncJpegStreamResponse(Camera *i_cam, Logs *i_log) {
  _callback = nullptr;
  _code = 200;
  _contentLength = 0;
  _contentType = STREAM_CONTENT_TYPE;
  _sendContentLength = false;
  _chunked = true;
  _index = 0;
  lastAsyncRequest = 0;
  FrameBuf = NULL;
  FrameLen = 0;
  FrameSent = 0;
  FrameIdx = -1;
  FrameSeq = UINT32_MAX;
  camera = i_cam;
  log = i_log;
  camera->SetStreamStatus(true);
}

/**
 * @brief Destroy the Async Jpeg Stream Response:: Async Jpeg Stream Response object
 * 
 */
AsyncJpegStreamResponse::~AsyncJpegStreamResponse() {
  if (FrameIdx >= 0) {
    camera->StreamReleaseFrame(FrameIdx);
  }
  camera->SetStreamStatus(false);
}

/**
 * @brief Check if source is valid
 * 
 * @return bool
 */
bool AsyncJpegStreamResponse::_sourceValid() const {
  return true;
}

/**
 * @brief Fill buffer
 * 
 * @param buf 
 * @param maxLen 
 * @return size_t 
 */
size_t AsyncJpegStreamResponse::_fillBuffer(uint8_t *buf, size_t maxLen) {
  size_t ret = _content(buf, maxLen, _index);
  if (ret != RESPONSE_TRY_AGAIN) {
    _index += ret;
  }
  return ret;
}

/**
 * @brief Content - get picture from camera and send it to client
 * 
 * @param buffer 
 * @param maxLen 
 * @param index 
 * @return size_t 
 */
size_t AsyncJpegStreamResponse::_content(uint8_t *buffer, size_t maxLen, size_t index) {
  if ((FrameIdx < 0) || (FrameSent == FrameLen)) {
    /* previous frame sent completely: statistics and release of the frame buffer */
    if (FrameIdx >= 0) {
      uint64_t end = (uint64_t)micros();
      uint32_t frameTimeMs = (uint32_t)((end - lastAsyncRequest) / 1000);
      if (frameTimeMs > 0) {
        camera->StreamSetFrameFps(1000.0 / frameTimeMs);
      }
      camera->StreamSetFrameSize(FrameLen / 1024);
      lastAsyncRequest = end;
      camera->StreamReleaseFrame(FrameIdx);
      FrameIdx = -1;
    }

    /* check space for headers */
    if (maxLen < (strlen(STREAM_BOUNDARY) + strlen(STREAM_PART) + strlen(JPG_CONTENT_TYPE) + 8)) {
      return RESPONSE_TRY_AGAIN;
    }

    /* stop the stream when the MCU is too hot. The client can start it again later */
    if (true == McuTemperature.ThermalThrottling) {
      log->AddEvent(LogLevel_Warning, F("Stream stopped, MCU temperature is too high"));
      return 0;
    }

    /* take a frame newer than the last one sent. The capture task captures it while the previous frame is sent,
       so the WEB server usually does not wait. Returning 0 ends the stream */
    uint8_t idx = 0;
    if (false == camera->StreamAcquireFrame(FrameSeq, &FrameBuf, &FrameLen, &idx, &FrameSeq, CAMERA_STREAM_FRAME_TIMEOUT)) {
      log->AddEvent(LogLevel_Error, F("Stream capture frame failed"));
      return 0;
    }
    FrameIdx = idx;
    FrameSent = 0;

    /* send boundary */
    size_t blen = 0;
    if (index) {
      blen = strlen(STREAM_BOUNDARY);
      memcpy(buffer, STREAM_BOUNDARY, blen);
      buffer += blen;
    }

    /* send header */
    size_t hlen = sprintf((char *)buffer, STREAM_PART, JPG_CONTENT_TYPE, FrameLen);
    buffer += hlen;

    /* send the beginning of the frame */
    size_t dlen = maxLen - hlen - blen;
    if (dlen > FrameLen) {
      dlen = FrameLen;
    }
    memcpy(buffer, FrameBuf, dlen);
    FrameSent = dlen;
    return blen + hlen + dlen;
  }

  /* send the next part of the frame */
  size_t available = FrameLen - FrameSent;
  if (maxLen > available) {
    maxLen = available;
  }
  memcpy(buffer, FrameBuf + FrameSent, maxLen);
  FrameSent += maxLen;
  return maxLen;
}

/* EOF */
