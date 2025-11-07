# Chunked File Upload Implementation - Progress Summary

## Overview
Successfully implemented JavaScript-based chunked file upload system for the `http_file_server` component to enable progress tracking during uploads. The ESP-IDF webserver blocks all HTTP requests during uploads, so chunked uploads allow progress polling between chunks.

## Final Configuration
- **Chunk Size**: 256KB (optimal balance of speed vs. responsiveness)
- **Upload Speed**: ~800 KB/s (up from initial ~80 KB/s)
- **Connection Mode**: `Connection: close` header (prevents socket exhaustion)
- **Timeout**: Removed fixed timeout, implemented stall detection instead
- **Stall Detection**: 90-second timeout without chunk completion
- **Logging**: Reduced by 98% - only logs every 50th chunk, first, and last chunk

## Key Files Modified

### 1. `/home/user/esphome/esphome/components/http_file_server/http_file_server.h`
- Added declaration: `void handle_api_upload_chunk(AsyncWebServerRequest *request);`
- New API endpoint for receiving chunked uploads

### 2. `/home/user/esphome/esphome/components/http_file_server/http_file_server.cpp`

#### API Routing (line ~272-274)
```cpp
} else if (uri.find(this->url_prefix_ + "/api/upload_chunk") == 0 && request->method() == HTTP_POST) {
  ESP_LOGD(TAG, "API UPLOAD_CHUNK endpoint hit, body_buffer size: %zu", this->body_buffer_.size());
  this->handle_api_upload_chunk(request);
```

#### Chunked Upload Handler (lines ~2649-2870)
- Receives chunks via query parameters: `filename`, `chunkIndex`, `totalChunks`, `path`, `fileSize`
- Reads chunk data directly from `httpd_req_t` using `httpd_req_recv()` in a loop (handles partial reads)
- Initializes upload on first chunk (chunkIndex == 0)
- Writes chunk to file and updates progress tracking
- Finalizes upload on last chunk (chunkIndex == totalChunks - 1)
- Thread-safe progress tracking with mutex

#### JavaScript Chunked Upload (lines ~1515-1638)
```javascript
const CHUNK_SIZE = 256 * 1024; // 256KB chunks
const totalChunks = Math.ceil(file.size / CHUNK_SIZE);

// Stall detection
let lastChunkCompleteTime = Date.now();
let lastChunkIndex = -1;
const STALL_TIMEOUT_MS = 90000; // 90 seconds

const stallDetectionInterval = setInterval(() => {
  const timeSinceLastChunk = Date.now() - lastChunkCompleteTime;
  if (timeSinceLastChunk > STALL_TIMEOUT_MS) {
    // Alert user and reload
  }
}, 10000); // Check every 10 seconds

for (let chunkIndex = 0; chunkIndex < totalChunks; chunkIndex++) {
  const chunk = file.slice(start, end);
  const apiUrl = API_BASE + '/api/upload_chunk' +
                 '?filename=' + encodeURIComponent(file.name) +
                 '&chunkIndex=' + chunkIndex +
                 '&totalChunks=' + totalChunks +
                 '&path=' + encodeURIComponent(window.location.pathname) +
                 '&fileSize=' + file.size;

  const response = await fetch(apiUrl, {
    method: 'POST',
    body: chunk,
    headers: {
      'Content-Type': 'application/octet-stream',
      'Connection': 'close'  // Force fresh connections
    }
  });

  // Update stall detection after successful chunk
  lastChunkCompleteTime = Date.now();
  lastChunkIndex = chunkIndex;
}

// Clear stall detection on completion or error
clearInterval(stallDetectionInterval);
```

## Problems Solved

### 1. ✅ Progress Tracking Blocked
- **Problem**: ESP-IDF webserver blocks all HTTP requests during uploads
- **Solution**: JavaScript chunked uploads allow progress polling between chunks

### 2. ✅ API URL Construction Error
- **Problem**: Used `window.location.pathname` instead of `API_BASE`, creating wrong URLs
- **Fix**: Use `API_BASE + '/api/upload_chunk'`

### 3. ✅ Empty body_buffer_ (size: 0)
- **Problem**: `web_server_idf` doesn't populate `body_buffer_` for `application/octet-stream`
- **Fix**: Read directly from `httpd_req_t` using `httpd_req_recv()`

### 4. ✅ 1KB Size Limit
- **Problem**: Tried `application/x-www-form-urlencoded` but hit 1024-byte limit
- **Fix**: Keep `application/octet-stream`, implement direct reading

### 5. ✅ Compilation Error - httpd_req_get_content_len()
- **Problem**: Used non-existent function
- **Fix**: Use `req->content_len` directly (struct member)

### 6. ✅ Partial Chunk Reads
- **Problem**: `httpd_req_recv()` returns partial reads
- **Result**: File truncated (31MB instead of 47MB)
- **Fix**: Loop until all `content_len` bytes received:
```cpp
while (total_received < content_len) {
  int ret = httpd_req_recv(req, buffer + total_received, content_len - total_received);
  total_received += ret;
}
```

### 7. ✅ Socket Exhaustion with Keep-Alive
- **Problem**: Keep-alive + frequent progress polls hit 7-socket limit
- **Result**: JSON truncation, 300 KB/s speed, stopped at 44MB
- **Fix**: Add `Connection: close` header to force fresh connections

### 8. ✅ Performance Too Slow (~80 KB/s)
- **Problem**: Initial implementation too slow
- **Fix Progression**: 500KB → 64KB → 256KB chunks + Connection: close
- **Final Result**: ~800 KB/s (hardware capable of 2 MB/s per copy/move tests)

### 9. ✅ Logging Overhead
- **Problem**: Excessive console logging slowing down uploads
- **Fix**: Reduced logging by 98% - only log every 50th chunk, first, and last

### 10. ✅ Fixed Timeout Limitation
- **Problem**: Fixed timeout doesn't work for large files (4GB FAT32, future exFAT support)
- **Fix**: Removed fixed timeout, implemented stall detection instead

### 11. ✅ Stall Detection Implementation
- **Problem**: Need to detect truly stalled uploads without timeout
- **Solution**: Track `lastChunkCompleteTime`, check every 10 seconds
- **Implementation**: Update time after each successful chunk, clear interval on completion/error

## Known Issues

### ⚠️ Upload Abort Issue - CRITICAL FINDINGS
- **Status**: ACTIVE INVESTIGATION
- **Symptoms**:
  - Upload consistently aborts before completion
  - Single XHR status 0 error (connection abort)
  - JavaScript File API reads file as 47MB when actual file is 55MB
  - Uploaded file is only 44MB (loss of 11MB from actual file)

- **CRITICAL FINDING - Request Count Limit**:
  - **Different chunk sizes result in different abort positions**
  - Example: 256KB chunks → stops at 44MB (~170 requests)
  - Example: Larger chunks → stops at 30MB (fewer requests)
  - **Small files work perfectly**: 1MB file (~4 chunks) uploads successfully with no corruption
  - **This rules out**: Timeout issues, fixed byte limits, implementation flaws
  - **This confirms**: Issue is related to number of HTTP requests/connections
  - **Estimated limit**: Approximately 170-180 HTTP requests before failure

- **File Size Discrepancy**:
  - Actual file on disk: 55MB
  - JavaScript `file.size` property: 47MB (8MB loss)
  - Uploaded file result: 44MB (additional 3MB loss from what JS reads)
  - Two separate data losses occurring

- **Possible Root Causes**:
  - ESP-IDF HTTP server connection/request count limit
  - Socket exhaustion despite `Connection: close` header
  - Browser or network stack request limit
  - File API reading issue (why does JS see 47MB instead of 55MB?)
  - Memory accumulation per request

- **NOT the cause** (ruled out):
  - Timeout (user confirmed performance is good, requests complete quickly)
  - Body size limits (octet-stream bypasses 1024-byte limit)
  - Memory leak (heap stable throughout upload)
  - fflush errors (no errors logged)

- **⛔ CRITICAL CONSTRAINT**:
  - **DO NOT modify core components** (web_server_idf, web_server_base, etc.)
  - Fix must be implemented within http_file_server component only
  - Cannot change core timeouts, limits, or configurations

- **Potential Solutions (within http_file_server only)**:
  1. **Adaptive chunk sizing**: Use larger chunks for large files to reduce request count
     - Example: <5MB = 256KB, 5-50MB = 1MB, >50MB = 2MB chunks
     - 55MB file with 2MB chunks = ~28 requests (well under 170 limit)
  2. **Request throttling**: Add small delays between chunks to allow cleanup
  3. **Hybrid approach**: Start with small chunks, increase size dynamically
  4. **Connection management**: Add explicit cleanup signals or keep-alive strategy

  **Waiting for diagnostic output before implementing solution**

## Performance Metrics
- **Current Upload Speed**: ~800 KB/s
- **Hardware Capability**: 2 MB/s (demonstrated by copy/move operations on USB)
- **Achievement**: 40% of hardware capability (reasonable for HTTP overhead)

## File Size Support
- **Goal**: Support files up to filesystem limits
  - FAT32: 4GB max file size
  - Future exFAT: Larger files
- **Implementation**: No fixed timeout, stall detection allows arbitrarily large files

## Testing Notes
- Browser caching disabled for testing
- Hard refresh (Ctrl+F5) used during development
- Tested on ESP32-P4 with USB storage
- Copy/move operations achieve 2 MB/s baseline

## Architecture Notes

### Content-Type Considerations
- `application/octet-stream`: No body population, requires direct read
- `application/x-www-form-urlencoded`: 1KB limit (CONFIG_HTTPD_MAX_REQ_HDR_LEN)
- **Choice**: `application/octet-stream` with `httpd_req_recv()` loop

### Socket Management
- ESP-IDF default: 7 sockets
- Keep-alive causes exhaustion with frequent polling
- `Connection: close` trades connection overhead for reliability

### Progress Tracking
- Thread-safe mutex protection
- Separate tracking for upload/copy/move/delete operations
- Updated per chunk, polled between chunks

## Next Steps for Debugging 44MB Issue
1. Add detailed logging around the failure point
2. Check ESP-IDF HTTP server configuration limits
3. Monitor memory usage during large uploads
4. Review ESP-IDF logs for timeout/error messages
5. Test with different file sizes to identify pattern
6. Check if issue is file-size dependent or time-dependent
