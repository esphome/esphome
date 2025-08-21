
#ifdef USE_ESP32
#ifndef USE_I2S_LEGACY

#include "decoder.h"
#include "esphome/core/log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_netif.h"
#include "lwip/api.h"
#include "lwip/dns.h"
#include "lwip/err.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mdns.h"
//#include "net_functions.h"

//#include <sys/time.h>

#include "driver/i2s_std.h"
#if CONFIG_USE_DSP_PROCESSOR
#include "dsp_processor.h"
#endif

// Opus decoder is implemented as a subcomponet from master git repo
#include "opus.h"

// flac decoder is implemented as a subcomponet from master git repo
#include "FLAC/stream_decoder.h"
#include "player.h"
#include "snapcast.h"

namespace esphome {
namespace snapclient {

static bool isCachedChunk = false;
static uint32_t cachedBlocks = 0;

static const char *const TAG = "snapclient";

static FLAC__StreamDecoder *flacDecoder = NULL;

const char *VERSION_STRING = "0.0.3";

void time_sync_msg_cb(void *args);
static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[],
                                                   size_t *bytes, void *client_data);
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
                                                     const FLAC__int32 *const buffer[], void *client_data);
static void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata,
                              void *client_data);
static void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status,
                           void *client_data);

static const int FAST_SYNC_LATENCY_BUF = 10000;      // in µs
static const int NORMAL_SYNC_LATENCY_BUF = 1000000;  // in µs

struct timeval tdif, tavg;

/* snapast parameters; configurable in menuconfig */
#define SNAPCAST_SERVER_USE_MDNS CONFIG_SNAPSERVER_USE_MDNS
char *SNAPCAST_SERVER_HOST;
uint16_t SNAPCAST_SERVER_PORT;
char *SNAPCAST_CLIENT_NAME;
#define SNAPCAST_USE_SOFT_VOL CONFIG_SNAPCLIENT_USE_SOFT_VOL

// static QueueHandle_t playerChunkQueueHandle = NULL;
SemaphoreHandle_t timeSyncSemaphoreHandle = NULL;

#if CONFIG_USE_DSP_PROCESSOR
#if CONFIG_SNAPCLIENT_DSP_FLOW_STEREO
dspFlows_t dspFlow = dspfStereo;
#endif
#if CONFIG_SNAPCLIENT_DSP_FLOW_BASSBOOST
dspFlows_t dspFlow = dspfBassBoost;
#endif
#if CONFIG_SNAPCLIENT_DSP_FLOW_BIAMP
dspFlows_t dspFlow = dspfBiamp;
#endif
#if CONFIG_SNAPCLIENT_DSP_FLOW_BASS_TREBLE_EQ
dspFlows_t dspFlow = dspfEQBassTreble;
#endif
#endif

audioDACdata_t audioDAC_data;
static QueueHandle_t audioDACQHdl = NULL;
SemaphoreHandle_t audioDACSemaphore = NULL;

typedef struct decoderData_s {
  uint32_t type;  // should be SNAPCAST_MESSAGE_CODEC_HEADER
                  // or SNAPCAST_MESSAGE_WIRE_CHUNK
  uint8_t *inData;
  tv_t timestamp;
  uint8_t *outData;
  uint32_t bytes;
} decoderData_t;

static char base_message_serialized[BASE_MESSAGE_SIZE];
static const esp_timer_create_args_t tSyncArgs = {.callback = &time_sync_msg_cb,
                                                  .dispatch_method = ESP_TIMER_TASK,
                                                  .name = "tSyncMsg",
                                                  .skip_unhandled_events = false};

struct netconn *lwipNetconn;

static int id_counter = 0;

static OpusDecoder *opusDecoder = NULL;

static decoderData_t decoderChunk = {
    .type = SNAPCAST_MESSAGE_INVALID,
    .inData = NULL,
    .timestamp = {0, 0},
    .outData = NULL,
    .bytes = 0,
};

static decoderData_t pcmChunk = {
    .type = SNAPCAST_MESSAGE_INVALID,
    .inData = NULL,
    .timestamp = {0, 0},
    .outData = NULL,
    .bytes = 0,
};

/**
 *
 */
void time_sync_msg_cb(void *args) {
  base_message_t base_message_tx;
  //  struct timeval now;
  int64_t now;
  // time_message_t time_message_tx = {{0, 0}};
  int rc1;

  // causes kernel panic, which shouldn't happen though?
  // Isn't it called from timer task instead of ISR?
  // xSemaphoreGive(timeSyncSemaphoreHandle);

  //  result = gettimeofday(&now, NULL);
  ////  ESP_LOGI(TAG, "time of day: %d", (int32_t)now.tv_sec +
  ///(int32_t)now.tv_usec);
  //  if (result) {
  //    ESP_LOGI(TAG, "Failed to gettimeofday");
  //
  //    return;
  //  }

  uint8_t *p_pkt = (uint8_t *) malloc(BASE_MESSAGE_SIZE + TIME_MESSAGE_SIZE);
  if (p_pkt == NULL) {
    ESP_LOGW(TAG, "%s: Failed to get memory for time sync message. Skipping this round.", __func__);

    return;
  }

  memset(p_pkt, 0, BASE_MESSAGE_SIZE + TIME_MESSAGE_SIZE);

  base_message_tx.type = SNAPCAST_MESSAGE_TIME;
  base_message_tx.id = id_counter++;
  base_message_tx.refersTo = 0;
  base_message_tx.received.sec = 0;
  base_message_tx.received.usec = 0;
  now = esp_timer_get_time();
  base_message_tx.sent.sec = now / 1000000;
  base_message_tx.sent.usec = now - base_message_tx.sent.sec * 1000000;
  base_message_tx.size = TIME_MESSAGE_SIZE;
  rc1 = base_message_serialize(&base_message_tx, (char *) &p_pkt[0], BASE_MESSAGE_SIZE);
  if (rc1) {
    ESP_LOGE(TAG, "Failed to serialize base message for time");

    return;
  }

  //  memset(&time_message_tx, 0, sizeof(time_message_tx));
  //  result = time_message_serialize(&time_message_tx,
  //  &p_pkt[BASE_MESSAGE_SIZE],
  //                                  TIME_MESSAGE_SIZE);
  //  if (result) {
  //    ESP_LOGI(TAG, "Failed to serialize time message");
  //
  //    return;
  //  }

  rc1 = netconn_write(lwipNetconn, p_pkt, BASE_MESSAGE_SIZE + TIME_MESSAGE_SIZE, NETCONN_NOCOPY);
  if (rc1 != ERR_OK) {
    ESP_LOGW(TAG, "error writing timesync msg");

    return;
  }

  free(p_pkt);

  //  ESP_LOGI(TAG, "%s: sent time sync message", __func__);

  //  xSemaphoreGiveFromISR(timeSyncSemaphoreHandle, &xHigherPriorityTaskWoken);
  //  if (xHigherPriorityTaskWoken) {
  //    portYIELD_FROM_ISR();
  //  }
}

/**
 *
 */
static FLAC__StreamDecoderReadStatus read_callback(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[],
                                                   size_t *bytes, void *client_data) {
  snapcastSetting_t *scSet = (snapcastSetting_t *) client_data;
  //  decoderData_t *flacData;

  (void) scSet;

  // xQueueReceive(decoderReadQHdl, &flacData, portMAX_DELAY);
  // if (xQueueReceive(decoderReadQHdl, &flacData, pdMS_TO_TICKS(100)))
  if (decoderChunk.inData) {
    //     ESP_LOGI(TAG, "in flac read cb %ld %p", flacData->bytes,
    // flacData->inData);

    if (decoderChunk.bytes <= 0) {
      //      free_flac_data(flacData);

      return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
    }

    isCachedChunk = false;

    //    if (flacData->inData == NULL) {
    //      free_flac_data(flacData);
    //
    //      return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
    //    }

    if (decoderChunk.bytes <= *bytes) {
      memcpy(buffer, decoderChunk.inData, decoderChunk.bytes);
      *bytes = decoderChunk.bytes;

      // ESP_LOGW(TAG, "read all flac inData %d", *bytes);

      free(decoderChunk.inData);
      decoderChunk.inData = NULL;
      decoderChunk.bytes = 0;
    } else {
      memcpy(buffer, decoderChunk.inData, *bytes);

      memmove(decoderChunk.inData, decoderChunk.inData + *bytes, decoderChunk.bytes - *bytes);
      decoderChunk.bytes -= *bytes;
      decoderChunk.inData = (uint8_t *) realloc(decoderChunk.inData, decoderChunk.bytes);

      // ESP_LOGW(TAG, "didn't read all flac inData %d", *bytes);
      //      flacData->inData += *bytes;
      //      flacData->bytes -= *bytes;
    }

    // free_flac_data(flacData);

    // xQueueSend (flacReadQHdl, &flacData, portMAX_DELAY);

    // xSemaphoreGive(decoderReadSemaphore);

    // ESP_LOGE(TAG, "%s: data processed", __func__);

    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
  } else {
    return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
  }
}

/**
 *
 */
static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame,
                                                     const FLAC__int32 *const buffer[], void *client_data) {
  size_t i;
  snapcastSetting_t *scSet = (snapcastSetting_t *) client_data;

  size_t bytes = frame->header.blocksize * frame->header.channels * frame->header.bits_per_sample / 8;

  (void) decoder;

  if (isCachedChunk) {
    cachedBlocks += frame->header.blocksize;
  }

  //  ESP_LOGI(TAG, "in flac write cb %ld %d, pcmChunk.bytes %ld",
  //  frame->header.blocksize, bytes, pcmChunk.bytes);

  if (frame->header.channels != scSet->ch) {
    ESP_LOGE(TAG,
             "ERROR: frame header reports different channel count %ld than "
             "previous metadata block %d",
             frame->header.channels, scSet->ch);
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (frame->header.bits_per_sample != scSet->bits) {
    ESP_LOGE(TAG,
             "ERROR: frame header reports different bps %ld than previous "
             "metadata block %d",
             frame->header.bits_per_sample, scSet->bits);
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (buffer[0] == NULL) {
    ESP_LOGE(TAG, "ERROR: buffer [0] is NULL");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (buffer[1] == NULL) {
    ESP_LOGE(TAG, "ERROR: buffer [1] is NULL");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  pcmChunk.outData = (uint8_t *) realloc(pcmChunk.outData, pcmChunk.bytes + bytes);
  if (!pcmChunk.outData) {
    ESP_LOGE(TAG, "%s, failed to allocate PCM chunk payload", __func__);
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  for (i = 0; i < frame->header.blocksize; i++) {
    // write little endian
    pcmChunk.outData[pcmChunk.bytes + 4 * i] = (uint8_t) (buffer[0][i]);
    pcmChunk.outData[pcmChunk.bytes + 4 * i + 1] = (uint8_t) (buffer[0][i] >> 8);
    pcmChunk.outData[pcmChunk.bytes + 4 * i + 2] = (uint8_t) (buffer[1][i]);
    pcmChunk.outData[pcmChunk.bytes + 4 * i + 3] = (uint8_t) (buffer[1][i] >> 8);
  }

  pcmChunk.bytes += bytes;

  scSet->chkInFrames = frame->header.blocksize;

  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

/**
 *
 */
void metadata_callback(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
  snapcastSetting_t *scSet = (snapcastSetting_t *) client_data;

  (void) decoder;

  if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
    // ESP_LOGI(TAG, "in flac meta cb");

    // save for later
    scSet->sr = metadata->data.stream_info.sample_rate;
    scSet->ch = metadata->data.stream_info.channels;
    scSet->bits = (i2s_data_bit_width_t) metadata->data.stream_info.bits_per_sample;

    ESP_LOGI(TAG, "fLaC sampleformat: %ld:%d:%d", scSet->sr, scSet->bits, scSet->ch);

    // ESP_LOGE(TAG, "%s: data processed", __func__);
  }
}

/**
 *
 */
void error_callback(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
  (void) decoder, (void) client_data;

  ESP_LOGE(TAG, "Got error callback: %s\n", FLAC__StreamDecoderErrorStatusString[status]);
}

/**
 *
 */
void init_snapcast(QueueHandle_t audioQHdl, const char *name, const char *host, uint16_t port) {
  audioDACQHdl = audioQHdl;
  audioDACSemaphore = xSemaphoreCreateMutex();
  audioDAC_data.mute = true;
  audioDAC_data.volume = 100;
  SNAPCAST_CLIENT_NAME = (char *) name;
  SNAPCAST_SERVER_HOST = (char *) host;
  SNAPCAST_SERVER_PORT = port;
}

/**
 *
 */
extern "C" void audio_set_mute(bool mute) {
  xSemaphoreTake(audioDACSemaphore, portMAX_DELAY);
  if (mute != audioDAC_data.mute) {
    audioDAC_data.mute = mute;
    xQueueOverwrite(audioDACQHdl, &audioDAC_data);
  }
  xSemaphoreGive(audioDACSemaphore);
}

/**
 *
 */
void audio_set_volume(int volume) {
  xSemaphoreTake(audioDACSemaphore, portMAX_DELAY);
  if (volume != audioDAC_data.volume) {
    audioDAC_data.volume = volume;
    xQueueOverwrite(audioDACQHdl, &audioDAC_data);
  }
  xSemaphoreGive(audioDACSemaphore);
}

/**
 *
 */
void http_get_task(void *pvParameters) {
  char *start;
  base_message_t base_message_rx;
  hello_message_t hello_message;
  wire_chunk_message_t wire_chnk = {{0, 0}, 0, NULL};
  char *hello_message_serialized = NULL;
  int result;
  int64_t now, trx, tdif, ttx;
  time_message_t time_message_rx = {{0, 0}};
  int64_t tmpDiffToServer;
  int64_t lastTimeSync = 0;
  esp_timer_handle_t timeSyncMessageTimer = NULL;
  esp_err_t err = 0;
  server_settings_message_t server_settings_message;
  bool received_header = false;
  mdns_result_t *r;
  codec_type_t codec = NONE;
  snapcastSetting_t scSet;
  pcm_chunk_message_t *pcmData = NULL;
  ip_addr_t remote_ip;
  uint16_t remotePort = 0;
  int rc1 = ERR_OK, rc2 = ERR_OK;
  struct netbuf *firstNetBuf = NULL;
  uint16_t len;
  uint64_t timeout = FAST_SYNC_LATENCY_BUF;
  char *codecString = NULL;
  char *codecPayload = NULL;
  char *serverSettingsString = NULL;

  // create a timer to send time sync messages every x µs
  esp_timer_create(&tSyncArgs, &timeSyncMessageTimer);

#if CONFIG_SNAPCLIENT_USE_MDNS
  ESP_LOGI(TAG, "Enable mdns");
  mdns_init();
#endif

  while (1) {
    // do some house keeping
    {
      received_header = false;

      timeout = FAST_SYNC_LATENCY_BUF;

      esp_timer_stop(timeSyncMessageTimer);

      if (opusDecoder != NULL) {
        opus_decoder_destroy(opusDecoder);
        opusDecoder = NULL;
      }

      if (flacDecoder != NULL) {
        FLAC__stream_decoder_finish(flacDecoder);
        FLAC__stream_decoder_delete(flacDecoder);
        flacDecoder = NULL;
      }

      if (decoderChunk.inData) {
        free(decoderChunk.inData);
        decoderChunk.inData = NULL;
      }

      if (decoderChunk.outData) {
        free(decoderChunk.outData);
        decoderChunk.outData = NULL;
      }

      if (codecString) {
        free(codecString);
        codecString = NULL;
      }

      if (codecPayload) {
        free(codecPayload);
        codecPayload = NULL;
      }

      if (codecPayload) {
        free(serverSettingsString);
        serverSettingsString = NULL;
      }
    }

#if SNAPCAST_SERVER_USE_MDNS
    // Find snapcast server
    // Connect to first snapcast server found
    r = NULL;
    err = 0;
    while (!r || err) {
      ESP_LOGI(TAG, "Lookup snapcast service on network");
      esp_err_t err = mdns_query_ptr("_snapcast", "_tcp", 3000, 20, &r);
      if (err) {
        ESP_LOGE(TAG, "Query Failed");
        vTaskDelay(pdMS_TO_TICKS(1000));
      }

      if (!r) {
        ESP_LOGW(TAG, "No results found!");
        vTaskDelay(pdMS_TO_TICKS(1000));
      }
    }

    mdns_ip_addr_t *a = r->addr;
    if (a) {
      ip_addr_copy(remote_ip, (a->addr));
      remote_ip.type = a->addr.type;
      remotePort = r->port;
      ESP_LOGI(TAG, "Found %s:%d", ipaddr_ntoa(&remote_ip), remotePort);

      mdns_query_results_free(r);
    } else {
      mdns_query_results_free(r);

      ESP_LOGW(TAG, "No IP found in MDNS query");

      continue;
    }
#else
    // configure a failsafe snapserver according to CONFIG values
    struct sockaddr_in servaddr;

    servaddr.sin_family = AF_INET;
    inet_pton(AF_INET, SNAPCAST_SERVER_HOST, &(servaddr.sin_addr.s_addr));
    servaddr.sin_port = htons(SNAPCAST_SERVER_PORT);

#if USE_NETWORK_IPV6
    inet_pton(AF_INET, SNAPCAST_SERVER_HOST, &(remote_ip.u_addr.ip4.addr));
    remote_ip.type = IPADDR_TYPE_V4;
#else
    if (!(inet_pton(AF_INET, SNAPCAST_SERVER_HOST, &remote_ip) == 1)) {
      ESP_LOGE(TAG, "invalid snapcast server ip");
      return;
    }
#endif
    remotePort = SNAPCAST_SERVER_PORT;

    ESP_LOGI(TAG, "try connecting to static configuration %s:%d", ipaddr_ntoa(&remote_ip), remotePort);
#endif

    if (lwipNetconn != NULL) {
      netconn_delete(lwipNetconn);
      lwipNetconn = NULL;
    }

    lwipNetconn = netconn_new(NETCONN_TCP);
    if (lwipNetconn == NULL) {
      ESP_LOGE(TAG, "can't create netconn");

      continue;
    }

    rc1 = netconn_bind(lwipNetconn, (const ip_addr_t *) IPADDR_ANY, 0);
    if (rc1 != ERR_OK) {
      ESP_LOGE(TAG, "can't bind local IP");
    }

    rc2 = netconn_connect(lwipNetconn, &remote_ip, remotePort);
    if (rc2 != ERR_OK) {
      ESP_LOGE(TAG, "can't connect to remote %s:%d, err %d", ipaddr_ntoa(&remote_ip), remotePort, rc2);
    }

    if (rc1 != ERR_OK || rc2 != ERR_OK) {
      netconn_close(lwipNetconn);
      netconn_delete(lwipNetconn);
      lwipNetconn = NULL;

      continue;
    }

    ESP_LOGI(TAG, "netconn connected");

    if (reset_latency_buffer() < 0) {
      ESP_LOGE(TAG, "reset_diff_buffer: couldn't reset median filter long. STOP");
      return;
    }

    char mac_address[18];
    uint8_t base_mac[6];
    // Get MAC address for WiFi station
#if CONFIG_SNAPCLIENT_USE_INTERNAL_ETHERNET || CONFIG_SNAPCLIENT_USE_SPI_ETHERNET
    esp_read_mac(base_mac, ESP_MAC_ETH);
#else
    esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
#endif
    sprintf(mac_address, "%02X:%02X:%02X:%02X:%02X:%02X", base_mac[0], base_mac[1], base_mac[2], base_mac[3],
            base_mac[4], base_mac[5]);

    now = esp_timer_get_time();

    // init base message
    base_message_rx.type = SNAPCAST_MESSAGE_HELLO;
    base_message_rx.id = id_counter++;
    base_message_rx.refersTo = 0x0000;
    base_message_rx.sent.sec = now / 1000000;
    base_message_rx.sent.usec = now - base_message_rx.sent.sec * 1000000;
    base_message_rx.received.sec = 0;
    base_message_rx.received.usec = 0;
    base_message_rx.size = 0x00000000;

    // init hello message
    hello_message.mac = mac_address;
    hello_message.hostname = SNAPCAST_CLIENT_NAME;
    hello_message.version = (char *) VERSION_STRING;
    hello_message.client_name = "libsnapcast";
    hello_message.os = "esp32";
    hello_message.arch = "xtensa";
    hello_message.instance = 1;
    hello_message.id = mac_address;
    hello_message.protocol_version = 2;

    if (hello_message_serialized == NULL) {
      hello_message_serialized = hello_message_serialize(&hello_message, (size_t *) &(base_message_rx.size));
      if (!hello_message_serialized) {
        ESP_LOGE(TAG, "Failed to serialize hello message");
        return;
      }
    }

    result = base_message_serialize(&base_message_rx, base_message_serialized, BASE_MESSAGE_SIZE);
    if (result) {
      ESP_LOGE(TAG, "Failed to serialize base message");
      return;
    }

    rc1 = netconn_write(lwipNetconn, base_message_serialized, BASE_MESSAGE_SIZE, NETCONN_NOCOPY);
    if (rc1 != ERR_OK) {
      ESP_LOGE(TAG, "netconn failed to send base message");

      continue;
    }
    rc1 = netconn_write(lwipNetconn, hello_message_serialized, base_message_rx.size, NETCONN_NOCOPY);
    if (rc1 != ERR_OK) {
      ESP_LOGE(TAG, "netconn failed to send hello message");

      continue;
    }

    ESP_LOGI(TAG, "netconn sent hello message");

    free(hello_message_serialized);
    hello_message_serialized = NULL;

    // init default setting
    scSet.buf_ms = 500;
    scSet.codec = NONE;
    scSet.bits = (i2s_data_bit_width_t) 16;
    scSet.ch = 2;
    scSet.sr = 44100;
    scSet.chkInFrames = 0;
    scSet.volume = 0;
    scSet.muted = true;

    uint64_t startTime, endTime;
    //    size_t currentPos = 0;
    size_t typedMsgCurrentPos = 0;
    uint32_t typedMsgLen = 0;
    uint32_t offset = 0;
    uint32_t payloadOffset = 0;
    uint32_t tmpData = 0;
    int32_t payloadDataShift = 0;

    static const uint8_t BASE_MESSAGE_STATE = 0;
    static const uint8_t TYPED_MESSAGE_STATE = 1;

    // 0 ... base message, 1 ... typed message
    uint32_t state = BASE_MESSAGE_STATE;
    uint32_t internalState = 0;

    firstNetBuf = NULL;

    while (1) {
      rc2 = netconn_recv(lwipNetconn, &firstNetBuf);
      if (rc2 != ERR_OK) {
        if (rc2 == ERR_CONN) {
          netconn_close(lwipNetconn);

          // restart and try to reconnect
          break;
        }

        if (firstNetBuf != NULL) {
          netbuf_delete(firstNetBuf);

          firstNetBuf = NULL;
        }
        continue;
      }

      // now parse the data
      netbuf_first(firstNetBuf);
      do {
        // currentPos = 0;

        rc1 = netbuf_data(firstNetBuf, (void **) &start, &len);
        if (rc1 == ERR_OK) {
          // ESP_LOGI (TAG, "netconn rx,"
          // "data len: %d, %d", len, netbuf_len(firstNetBuf) -
          // currentPos);
        } else {
          ESP_LOGE(TAG, "netconn rx, couldn't get data");

          continue;
        }

        while (len > 0) {
          rc1 = ERR_OK;  // probably not necessary

          switch (state) {
            // decode base message
            case BASE_MESSAGE_STATE: {
              switch (internalState) {
                case 0:
                  base_message_rx.type = *start & 0xFF;
                  internalState++;
                  break;

                case 1:
                  base_message_rx.type |= (*start & 0xFF) << 8;
                  internalState++;
                  break;

                case 2:
                  base_message_rx.id = *start & 0xFF;
                  internalState++;
                  break;

                case 3:
                  base_message_rx.id |= (*start & 0xFF) << 8;
                  internalState++;
                  break;

                case 4:
                  base_message_rx.refersTo = *start & 0xFF;
                  internalState++;
                  break;

                case 5:
                  base_message_rx.refersTo |= (*start & 0xFF) << 8;
                  internalState++;
                  break;

                case 6:
                  base_message_rx.sent.sec = *start & 0xFF;
                  internalState++;
                  break;

                case 7:
                  base_message_rx.sent.sec |= (*start & 0xFF) << 8;
                  internalState++;
                  break;

                case 8:
                  base_message_rx.sent.sec |= (*start & 0xFF) << 16;
                  internalState++;
                  break;

                case 9:
                  base_message_rx.sent.sec |= (*start & 0xFF) << 24;
                  internalState++;
                  break;

                case 10:
                  base_message_rx.sent.usec = *start & 0xFF;
                  internalState++;
                  break;

                case 11:
                  base_message_rx.sent.usec |= (*start & 0xFF) << 8;
                  internalState++;
                  break;

                case 12:
                  base_message_rx.sent.usec |= (*start & 0xFF) << 16;
                  internalState++;
                  break;

                case 13:
                  base_message_rx.sent.usec |= (*start & 0xFF) << 24;
                  internalState++;
                  break;

                case 14:
                  base_message_rx.received.sec = *start & 0xFF;
                  internalState++;
                  break;

                case 15:
                  base_message_rx.received.sec |= (*start & 0xFF) << 8;
                  internalState++;
                  break;

                case 16:
                  base_message_rx.received.sec |= (*start & 0xFF) << 16;
                  internalState++;
                  break;

                case 17:
                  base_message_rx.received.sec |= (*start & 0xFF) << 24;
                  internalState++;
                  break;

                case 18:
                  base_message_rx.received.usec = *start & 0xFF;
                  internalState++;
                  break;

                case 19:
                  base_message_rx.received.usec |= (*start & 0xFF) << 8;
                  internalState++;
                  break;

                case 20:
                  base_message_rx.received.usec |= (*start & 0xFF) << 16;
                  internalState++;
                  break;

                case 21:
                  base_message_rx.received.usec |= (*start & 0xFF) << 24;
                  internalState++;
                  break;

                case 22:
                  base_message_rx.size = *start & 0xFF;
                  internalState++;
                  break;

                case 23:
                  base_message_rx.size |= (*start & 0xFF) << 8;
                  internalState++;
                  break;

                case 24:
                  base_message_rx.size |= (*start & 0xFF) << 16;
                  internalState++;
                  break;

                case 25:
                  base_message_rx.size |= (*start & 0xFF) << 24;
                  internalState = 0;

                  now = esp_timer_get_time();

                  base_message_rx.received.sec = now / 1000000;
                  base_message_rx.received.usec = now - base_message_rx.received.sec * 1000000;

                  typedMsgCurrentPos = 0;

                  //                   ESP_LOGI(TAG,"BM type %d ts %d.%d",
                  //                   base_message_rx.type,
                  //                   base_message_rx.received.sec,
                  //                   base_message_rx.received.usec);
                  // ESP_LOGI(TAG,"%d, %d.%d", base_message_rx.type,
                  //                   base_message_rx.received.sec,
                  //                   base_message_rx.received.usec);
                  // ESP_LOGI(TAG,"%d, %llu", base_message_rx.type,
                  //       1000000ULL * base_message_rx.received.sec +
                  // base_message_rx.received.usec);

                  state = TYPED_MESSAGE_STATE;
                  break;
              }

              // currentPos++;++;
              len--;
              start++;

              break;
            }

            // decode typed message
            case TYPED_MESSAGE_STATE: {
              switch (base_message_rx.type) {
                case SNAPCAST_MESSAGE_WIRE_CHUNK: {
                  switch (internalState) {
                    case 0: {
                      wire_chnk.timestamp.sec = *start & 0xFF;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 1: {
                      wire_chnk.timestamp.sec |= (*start & 0xFF) << 8;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 2: {
                      wire_chnk.timestamp.sec |= (*start & 0xFF) << 16;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 3: {
                      wire_chnk.timestamp.sec |= (*start & 0xFF) << 24;

                      // ESP_LOGI(TAG,
                      // "wire chunk time sec: %d",
                      // wire_chnk.timestamp.sec);

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 4: {
                      wire_chnk.timestamp.usec = (*start & 0xFF);

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 5: {
                      wire_chnk.timestamp.usec |= (*start & 0xFF) << 8;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 6: {
                      wire_chnk.timestamp.usec |= (*start & 0xFF) << 16;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 7: {
                      wire_chnk.timestamp.usec |= (*start & 0xFF) << 24;

                      // ESP_LOGI(TAG,
                      // "wire chunk time usec: %d",
                      // wire_chnk.timestamp.usec);

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 8: {
                      wire_chnk.size = (*start & 0xFF);

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 9: {
                      wire_chnk.size |= (*start & 0xFF) << 8;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 10: {
                      wire_chnk.size |= (*start & 0xFF) << 16;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 11: {
                      wire_chnk.size |= (*start & 0xFF) << 24;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      // TODO: we could use wire chunk directly maybe?
                      decoderChunk.bytes = wire_chnk.size;
                      while (!decoderChunk.inData) {
                        decoderChunk.inData = (uint8_t *) malloc(decoderChunk.bytes);
                        if (!decoderChunk.inData) {
                          ESP_LOGW(TAG, "malloc decoderChunk.inData failed, wait "
                                        "1ms and try again");

                          vTaskDelay(pdMS_TO_TICKS(1));
                        }
                      }

                      payloadOffset = 0;

#if 0
                       ESP_LOGI(TAG, "chunk with size: %u, at time %ld.%ld",
                                             wire_chnk.size,
                                             wire_chnk.timestamp.sec,
                                             wire_chnk.timestamp.usec);
#endif

                      if (len == 0) {
                        break;
                      }
                    }

                    case 12: {
                      size_t tmp_size;

                      if ((base_message_rx.size - typedMsgCurrentPos) <= len) {
                        tmp_size = base_message_rx.size - typedMsgCurrentPos;
                      } else {
                        tmp_size = len;
                      }

                      if (received_header == true) {
                        switch (codec) {
                          case OPUS:
                          case FLAC: {
                            memcpy(&decoderChunk.inData[payloadOffset], start, tmp_size);
                            payloadOffset += tmp_size;
                            decoderChunk.outData = NULL;
                            decoderChunk.type = SNAPCAST_MESSAGE_WIRE_CHUNK;

                            break;
                          }

                          case PCM: {
                            size_t _tmp = tmp_size;

                            offset = 0;

                            if (pcmData == NULL) {
                              if (allocate_pcm_chunk_memory(&pcmData, wire_chnk.size) < 0) {
                                pcmData = NULL;
                              }

                              tmpData = 0;
                              payloadDataShift = 3;
                              payloadOffset = 0;
                            }

                            while (_tmp--) {
                              tmpData |= ((uint32_t) start[offset++] << (8 * payloadDataShift));

                              payloadDataShift--;
                              if (payloadDataShift < 0) {
                                payloadDataShift = 3;

                                if ((pcmData) && (pcmData->fragment->payload)) {
                                  volatile uint32_t *sample;
                                  uint8_t dummy1;
                                  uint32_t dummy2 = 0;

                                  // TODO: find a more
                                  // clever way to do this,
                                  // best would be to
                                  // actually store it the
                                  // right way in the first
                                  // place
                                  dummy1 = tmpData >> 24;
                                  dummy2 |= (uint32_t) dummy1 << 16;
                                  dummy1 = tmpData >> 16;
                                  dummy2 |= (uint32_t) dummy1 << 24;
                                  dummy1 = tmpData >> 8;
                                  dummy2 |= (uint32_t) dummy1 << 0;
                                  dummy1 = tmpData >> 0;
                                  dummy2 |= (uint32_t) dummy1 << 8;
                                  tmpData = dummy2;

                                  sample = (volatile uint32_t *) (&(pcmData->fragment->payload[payloadOffset]));
                                  *sample = (volatile uint32_t) tmpData;

                                  payloadOffset += 4;
                                }

                                tmpData = 0;
                              }
                            }

                            break;
                          }

                          default: {
                            ESP_LOGE(TAG, "Decoder (1) not supported");

                            return;

                            break;
                          }
                        }
                      }

                      typedMsgCurrentPos += tmp_size;
                      start += tmp_size;
                      // currentPos += tmp_size;
                      len -= tmp_size;

                      if (typedMsgCurrentPos >= base_message_rx.size) {
                        if (received_header == true) {
                          switch (codec) {
                            case OPUS: {
                              int frame_size = -1;
                              int samples_per_frame;
                              opus_int16 *audio = NULL;

                              samples_per_frame = opus_packet_get_samples_per_frame(decoderChunk.inData, scSet.sr);
                              if (samples_per_frame < 0) {
                                ESP_LOGE(TAG, "couldn't get samples per frame count "
                                              "of packet");
                              }

                              scSet.chkInFrames = samples_per_frame;

                              // ESP_LOGW(TAG, "%d, %llu, %llu",
                              // samples_per_frame, 1000000ULL *
                              // samples_per_frame / scSet.sr,
                              // 1000000ULL *
                              // wire_chnk.timestamp.sec +
                              // wire_chnk.timestamp.usec);

                              // ESP_LOGW(TAG, "got OPUS decoded chunk size: %ld
                              // " "frames from encoded chunk with size %d,
                              // allocated audio buffer %d", scSet.chkInFrames,
                              // wire_chnk.size, samples_per_frame);

                              size_t bytes;
                              do {
                                bytes = samples_per_frame * (scSet.ch * scSet.bits >> 3);

                                while ((audio = (opus_int16 *) realloc(audio, bytes)) == NULL) {
                                  ESP_LOGE(TAG,
                                           "couldn't realloc memory for OPUS "
                                           "audio %d",
                                           bytes);

                                  vTaskDelay(pdMS_TO_TICKS(1));
                                }

                                frame_size = opus_decode(opusDecoder, decoderChunk.inData, decoderChunk.bytes,
                                                         (opus_int16 *) audio, samples_per_frame, 0);

                                samples_per_frame <<= 1;
                              } while (frame_size < 0);

                              free(decoderChunk.inData);
                              decoderChunk.inData = NULL;

                              pcm_chunk_message_t *new_pcmChunk = NULL;

                              // ESP_LOGW(TAG, "OPUS decode: %d", frame_size);

                              if (allocate_pcm_chunk_memory(&new_pcmChunk, bytes) < 0) {
                                pcmData = NULL;
                              } else {
                                new_pcmChunk->timestamp = wire_chnk.timestamp;

                                if (new_pcmChunk->fragment->payload) {
                                  volatile uint32_t *sample;
                                  uint32_t tmpData;
                                  uint32_t cnt = 0;

                                  for (int i = 0; i < bytes; i += 4) {
                                    sample = (volatile uint32_t *) (&(new_pcmChunk->fragment->payload[i]));
                                    tmpData = (((uint32_t) audio[cnt] << 16) & 0xFFFF0000) |
                                              (((uint32_t) audio[cnt + 1] << 0) & 0x0000FFFF);
                                    *sample = (volatile uint32_t) tmpData;

                                    cnt += 2;
                                  }
                                }

                                free(audio);
                                audio = NULL;

#if CONFIG_USE_DSP_PROCESSOR
                                if (new_pcmChunk->fragment->payload) {
                                  dsp_processor_worker(new_pcmChunk->fragment->payload, new_pcmChunk->fragment->size,
                                                       scSet.sr);
                                }
#endif

                                insert_pcm_chunk(new_pcmChunk);
                              }

                              if (player_send_snapcast_setting(&scSet) != pdPASS) {
                                ESP_LOGE(TAG, "Failed to notify "
                                              "sync task about "
                                              "codec. Did you "
                                              "init player?");

                                return;
                              }

                              break;
                            }

                            case FLAC: {
                              isCachedChunk = true;
                              cachedBlocks = 0;

                              while (decoderChunk.bytes > 0) {
                                if (FLAC__stream_decoder_process_single(flacDecoder) == 0) {
                                  ESP_LOGE(TAG,
                                           "%s: FLAC__stream_decoder_process_single "
                                           "failed",
                                           __func__);

                                  // TODO: should insert some abort condition?
                                  vTaskDelay(pdMS_TO_TICKS(10));
                                }
                              }

                              // alternating chunk sizes need time stamp repair
                              if ((cachedBlocks > 0) && (scSet.sr != 0)) {
                                uint64_t diffUs = 1000000ULL * cachedBlocks / scSet.sr;

                                uint64_t timestamp = 1000000ULL * wire_chnk.timestamp.sec + wire_chnk.timestamp.usec;

                                timestamp = timestamp - diffUs;

                                wire_chnk.timestamp.sec = timestamp / 1000000ULL;
                                wire_chnk.timestamp.usec = timestamp % 1000000ULL;
                              }

                              pcm_chunk_message_t *new_pcmChunk;
                              int32_t ret = allocate_pcm_chunk_memory(&new_pcmChunk, pcmChunk.bytes);

                              scSet.chkInFrames = FLAC__stream_decoder_get_blocksize(flacDecoder);

                              // ESP_LOGE (TAG, "block size: %ld",
                              // scSet.chkInFrames * scSet.bits / 8 * scSet.ch);
                              // ESP_LOGI(TAG, "new_pcmChunk with size %ld",
                              // new_pcmChunk->totalSize);

                              if (ret == 0) {
                                pcm_chunk_fragment_t *fragment = new_pcmChunk->fragment;
                                uint32_t fragmentCnt = 0;

                                if (fragment->payload != NULL) {
                                  uint32_t frames = pcmChunk.bytes / (scSet.ch * (scSet.bits / 8));

                                  for (int i = 0; i < frames; i++) {
                                    // TODO: for now fragmented payload is not
                                    // supported and the whole chunk is expected
                                    // to be in the first fragment
                                    uint32_t tmpData;
                                    memcpy(&tmpData, &pcmChunk.outData[fragmentCnt], (scSet.ch * (scSet.bits / 8)));

                                    if (fragment != NULL) {
                                      volatile uint32_t *test =
                                          (volatile uint32_t *) (&(fragment->payload[fragmentCnt]));
                                      *test = (volatile uint32_t) tmpData;
                                    }

                                    fragmentCnt += (scSet.ch * (scSet.bits / 8));
                                    if (fragmentCnt >= fragment->size) {
                                      fragmentCnt = 0;

                                      fragment = fragment->nextFragment;
                                    }
                                  }
                                }

                                new_pcmChunk->timestamp = wire_chnk.timestamp;

#if CONFIG_USE_DSP_PROCESSOR
                                if (new_pcmChunk->fragment->payload) {
                                  dsp_processor_worker(new_pcmChunk->fragment->payload, new_pcmChunk->fragment->size,
                                                       scSet.sr);
                                }

#endif

                                insert_pcm_chunk(new_pcmChunk);
                              }

                              free(pcmChunk.outData);
                              pcmChunk.outData = NULL;
                              pcmChunk.bytes = 0;

                              if (player_send_snapcast_setting(&scSet) != pdPASS) {
                                ESP_LOGE(TAG, "Failed to "
                                              "notify "
                                              "sync task "
                                              "about "
                                              "codec. Did you "
                                              "init player?");

                                return;
                              }

                              break;
                            }

                            case PCM: {
                              size_t decodedSize = wire_chnk.size;

                              // ESP_LOGW(TAG, "got PCM chunk,"
                              //               "typedMsgCurrentPos %d",
                              //               typedMsgCurrentPos);

                              if (pcmData) {
                                pcmData->timestamp = wire_chnk.timestamp;
                              }

                              scSet.chkInFrames = decodedSize / ((size_t) scSet.ch * (size_t) (scSet.bits / 8));

                              // ESP_LOGW(TAG,
                              //          "got PCM decoded chunk size: %ld
                              //          frames", scSet.chkInFrames);

                              if (player_send_snapcast_setting(&scSet) != pdPASS) {
                                ESP_LOGE(TAG, "Failed to notify "
                                              "sync task about "
                                              "codec. Did you "
                                              "init player?");

                                return;
                              }

#if CONFIG_USE_DSP_PROCESSOR
                              if ((pcmData) && (pcmData->fragment->payload)) {
                                dsp_processor_worker(pcmData->fragment->payload, pcmData->fragment->size, scSet.sr);
                              }
#endif

                              if (pcmData) {
                                insert_pcm_chunk(pcmData);
                              }

                              pcmData = NULL;

                              free(decoderChunk.inData);
                              decoderChunk.inData = NULL;

                              break;
                            }

                            default: {
                              ESP_LOGE(TAG, "Decoder (2) not "
                                            "supported");

                              return;

                              break;
                            }
                          }
                        }

                        state = BASE_MESSAGE_STATE;
                        internalState = 0;

                        typedMsgCurrentPos = 0;
                      }

                      break;
                    }

                    default: {
                      ESP_LOGE(TAG, "wire chunk decoder "
                                    "shouldn't get here");

                      break;
                    }
                  }

                  break;
                }

                case SNAPCAST_MESSAGE_CODEC_HEADER: {
                  switch (internalState) {
                    case 0: {
                      typedMsgLen = *start & 0xFF;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 1: {
                      typedMsgLen |= (*start & 0xFF) << 8;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 2: {
                      typedMsgLen |= (*start & 0xFF) << 16;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 3: {
                      typedMsgLen |= (*start & 0xFF) << 24;

                      codecString = (char *) malloc(typedMsgLen + 1);  // allocate memory for
                                                                       // codec string
                      if (codecString == NULL) {
                        ESP_LOGE(TAG, "couldn't get memory "
                                      "for codec string");

                        return;
                      }

                      offset = 0;
                      // ESP_LOGI(TAG,
                      // "codec header string is %d long",
                      // typedMsgLen);

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 4: {
                      if (len >= typedMsgLen) {
                        memcpy(&codecString[offset], start, typedMsgLen);

                        offset += typedMsgLen;

                        typedMsgCurrentPos += typedMsgLen;
                        start += typedMsgLen;
                        // currentPos += typedMsgLen;
                        len -= typedMsgLen;
                      } else {
                        memcpy(&codecString[offset], start, typedMsgLen);

                        offset += len;

                        typedMsgCurrentPos += len;
                        start += len;
                        // currentPos += len;
                        len -= len;
                      }

                      if (offset == typedMsgLen) {
                        // NULL terminate string
                        codecString[typedMsgLen] = 0;

                        // ESP_LOGI (TAG, "got codec string: %s", tmp);

                        if (strcmp(codecString, "opus") == 0) {
                          codec = OPUS;
                        } else if (strcmp(codecString, "flac") == 0) {
                          codec = FLAC;
                        } else if (strcmp(codecString, "pcm") == 0) {
                          codec = PCM;
                        } else {
                          codec = NONE;

                          ESP_LOGI(TAG, "Codec : %s not supported", codecString);
                          ESP_LOGI(TAG, "Change encoder codec to "
                                        "opus, flac or pcm in "
                                        "/etc/snapserver.conf on "
                                        "server");

                          return;
                        }

                        free(codecString);
                        codecString = NULL;

                        internalState++;
                      }

                      if (len == 0) {
                        break;
                      }
                    }

                    case 5: {
                      typedMsgLen = *start & 0xFF;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 6: {
                      typedMsgLen |= (*start & 0xFF) << 8;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 7: {
                      typedMsgLen |= (*start & 0xFF) << 16;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 8: {
                      typedMsgLen |= (*start & 0xFF) << 24;

                      codecPayload = (char *) malloc(typedMsgLen);  // allocate memory
                                                                    // for codec payload
                      if (codecPayload == NULL) {
                        ESP_LOGE(TAG, "couldn't get memory "
                                      "for codec payload");

                        return;
                      }

                      offset = 0;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 9: {
                      if (len >= typedMsgLen) {
                        memcpy(&codecPayload[offset], start, typedMsgLen);

                        offset += typedMsgLen;

                        typedMsgCurrentPos += typedMsgLen;
                        start += typedMsgLen;
                        // currentPos += typedMsgLen;
                        len -= typedMsgLen;
                      } else {
                        memcpy(&codecPayload[offset], start, len);

                        offset += len;

                        typedMsgCurrentPos += len;
                        start += len;
                        // currentPos += len;
                        len -= len;
                      }

                      if (offset == typedMsgLen) {
                        // first ensure everything is set up
                        // correctly and resources are
                        // available

                        if (flacDecoder != NULL) {
                          FLAC__stream_decoder_finish(flacDecoder);
                          FLAC__stream_decoder_delete(flacDecoder);
                          flacDecoder = NULL;
                        }

                        if (opusDecoder != NULL) {
                          opus_decoder_destroy(opusDecoder);
                          opusDecoder = NULL;
                        }

                        if (codec == OPUS) {
                          uint16_t channels;
                          uint32_t rate;
                          uint16_t bits;

                          memcpy(&rate, codecPayload + 4, sizeof(rate));
                          memcpy(&bits, codecPayload + 8, sizeof(bits));
                          memcpy(&channels, codecPayload + 10, sizeof(channels));

                          scSet.codec = codec;
                          scSet.bits = (i2s_data_bit_width_t) bits;
                          scSet.ch = channels;
                          scSet.sr = rate;

                          ESP_LOGI(TAG, "Opus sample format: %ld:%d:%d\n", rate, bits, channels);

                          int error = 0;

                          opusDecoder = opus_decoder_create(scSet.sr, scSet.ch, &error);
                          if (error != 0) {
                            ESP_LOGI(TAG, "Failed to init opus coder");
                            return;
                          }

                          ESP_LOGI(TAG, "Initialized opus Decoder: %d", error);
                        } else if (codec == FLAC) {
                          decoderChunk.bytes = typedMsgLen;
                          decoderChunk.inData = (uint8_t *) malloc(decoderChunk.bytes);
                          memcpy(decoderChunk.inData, codecPayload, typedMsgLen);
                          decoderChunk.outData = NULL;
                          decoderChunk.type = SNAPCAST_MESSAGE_CODEC_HEADER;

                          flacDecoder = FLAC__stream_decoder_new();
                          if (flacDecoder == NULL) {
                            ESP_LOGE(TAG, "Failed to init flac decoder");
                            return;
                          }

                          FLAC__StreamDecoderInitStatus init_status = FLAC__stream_decoder_init_stream(
                              flacDecoder, read_callback, NULL, NULL, NULL, NULL, write_callback, metadata_callback,
                              error_callback, &scSet);
                          if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
                            ESP_LOGE(TAG, "ERROR: initializing decoder: %s\n",
                                     FLAC__StreamDecoderInitStatusString[init_status]);

                            return;
                          }

                          FLAC__stream_decoder_process_until_end_of_metadata(flacDecoder);

                          // ESP_LOGI(TAG, "%s: processed codec header",
                          // __func__);
                        } else if (codec == PCM) {
                          uint16_t channels;
                          uint32_t rate;
                          uint16_t bits;

                          memcpy(&channels, codecPayload + 22, sizeof(channels));
                          memcpy(&rate, codecPayload + 24, sizeof(rate));
                          memcpy(&bits, codecPayload + 34, sizeof(bits));

                          scSet.codec = codec;
                          scSet.bits = (i2s_data_bit_width_t) bits;
                          scSet.ch = channels;
                          scSet.sr = rate;

                          ESP_LOGI(TAG, "pcm sampleformat: %ld:%d:%d", scSet.sr, scSet.bits, scSet.ch);
                        } else {
                          ESP_LOGE(TAG, "codec header decoder "
                                        "shouldn't get here after "
                                        "codec string was detected");

                          return;
                        }

                        free(codecPayload);
                        codecPayload = NULL;

                        if (player_send_snapcast_setting(&scSet) != pdPASS) {
                          ESP_LOGE(TAG, "Failed to notify sync task. "
                                        "Did you init player?");

                          return;
                        }

                        // ESP_LOGI(TAG, "done codec header msg");

                        state = BASE_MESSAGE_STATE;
                        internalState = 0;

                        received_header = true;
                        esp_timer_stop(timeSyncMessageTimer);
                        if (!esp_timer_is_active(timeSyncMessageTimer)) {
                          esp_timer_start_periodic(timeSyncMessageTimer, timeout);
                        }
                      }

                      break;
                    }

                    default: {
                      ESP_LOGE(TAG, "codec header decoder "
                                    "shouldn't get here");

                      break;
                    }
                  }

                  break;
                }

                case SNAPCAST_MESSAGE_SERVER_SETTINGS: {
                  switch (internalState) {
                    case 0: {
                      typedMsgLen = *start & 0xFF;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 1: {
                      typedMsgLen |= (*start & 0xFF) << 8;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 2: {
                      typedMsgLen |= (*start & 0xFF) << 16;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 3: {
                      typedMsgLen |= (*start & 0xFF) << 24;

                      // ESP_LOGI(TAG,"server settings string is %lu"
                      //              " long", typedMsgLen);

                      // now get some memory for server settings
                      // string
                      serverSettingsString = (char *) malloc(typedMsgLen + 1);
                      if (serverSettingsString == NULL) {
                        ESP_LOGE(TAG, "couldn't get memory for "
                                      "server settings string");
                      }

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      offset = 0;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 4: {
                      size_t tmpSize = base_message_rx.size - typedMsgCurrentPos;

                      if (len > 0) {
                        if (tmpSize < len) {
                          if (serverSettingsString) {
                            memcpy(&serverSettingsString[offset], start, tmpSize);
                          }
                          offset += tmpSize;

                          start += tmpSize;
                          // currentPos += tmpSize;  // will be
                          //  incremented by 1
                          //  later so -1 here
                          typedMsgCurrentPos += tmpSize;
                          len -= tmpSize;
                        } else {
                          if (serverSettingsString) {
                            memcpy(&serverSettingsString[offset], start, len);
                          }
                          offset += len;

                          start += len;
                          // currentPos += len;  // will be incremented
                          //  by 1 later so -1
                          //  here
                          typedMsgCurrentPos += len;
                          len = 0;
                        }
                      }

                      if (typedMsgCurrentPos >= base_message_rx.size) {
                        if (serverSettingsString) {
                          // ESP_LOGI(TAG, "done server settings %lu/%lu",
                          //                offset,
                          //                typedMsgLen);

                          // NULL terminate string
                          serverSettingsString[typedMsgLen] = 0;

                          // ESP_LOGI(TAG, "got string: %s",
                          // serverSettingsString);

                          result = server_settings_message_deserialize(&server_settings_message, serverSettingsString);
                          if (result) {
                            ESP_LOGE(TAG,
                                     "Failed to read server "
                                     "settings: %d",
                                     result);
                          } else {
                            // log mute state, buffer, latency
                            ESP_LOGI(TAG, "Buffer length:  %ld", server_settings_message.buffer_ms);
                            ESP_LOGI(TAG, "Latency:        %ld", server_settings_message.latency);
                            ESP_LOGI(TAG, "Mute:           %d", server_settings_message.muted);
                            ESP_LOGI(TAG, "Setting volume: %ld", server_settings_message.volume);
                          }

                          // Volume setting using ADF HAL
                          // abstraction
                          if (scSet.muted != server_settings_message.muted) {
#if SNAPCAST_USE_SOFT_VOL
                            if (server_settings_message.muted) {
                              dsp_processor_set_volome(0.0);
                            } else {
                              dsp_processor_set_volome((double) server_settings_message.volume / 100);
                            }
#endif
                            audio_set_mute(server_settings_message.muted);
                          }

                          if (scSet.volume != server_settings_message.volume) {
#if SNAPCAST_USE_SOFT_VOL
                            if (!server_settings_message.muted) {
                              dsp_processor_set_volome((double) server_settings_message.volume / 100);
                            }
#else
                            audio_set_volume(server_settings_message.volume);
#endif
                          }

                          scSet.cDacLat_ms = server_settings_message.latency;
                          scSet.buf_ms = server_settings_message.buffer_ms;
                          scSet.muted = server_settings_message.muted;
                          scSet.volume = server_settings_message.volume;

                          if (player_send_snapcast_setting(&scSet) != pdPASS) {
                            ESP_LOGE(TAG, "Failed to notify sync task. "
                                          "Did you init player?");

                            return;
                          }

                          free(serverSettingsString);
                          serverSettingsString = NULL;
                        }

                        state = BASE_MESSAGE_STATE;
                        internalState = 0;

                        typedMsgCurrentPos = 0;
                      }

                      break;
                    }

                    default: {
                      ESP_LOGE(TAG, "server settings decoder "
                                    "shouldn't get here");

                      break;
                    }
                  }

                  break;
                }

                case SNAPCAST_MESSAGE_STREAM_TAGS: {
                  size_t tmpSize = base_message_rx.size - typedMsgCurrentPos;

                  if (tmpSize < len) {
                    start += tmpSize;
                    // currentPos += tmpSize;
                    typedMsgCurrentPos += tmpSize;
                    len -= tmpSize;
                  } else {
                    start += len;
                    // currentPos += len;

                    typedMsgCurrentPos += len;
                    len = 0;
                  }

                  if (typedMsgCurrentPos >= base_message_rx.size) {
                    // ESP_LOGI(TAG,
                    // "done stream tags with length %d %d %d",
                    // base_message_rx.size, currentPos,
                    // tmpSize);

                    typedMsgCurrentPos = 0;
                    // currentPos = 0;

                    state = BASE_MESSAGE_STATE;
                    internalState = 0;
                  }

                  break;
                }

                case SNAPCAST_MESSAGE_TIME: {
                  switch (internalState) {
                    case 0: {
                      time_message_rx.latency.sec = *start;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 1: {
                      time_message_rx.latency.sec |= (int32_t) *start << 8;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 2: {
                      time_message_rx.latency.sec |= (int32_t) *start << 16;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 3: {
                      time_message_rx.latency.sec |= (int32_t) *start << 24;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 4: {
                      time_message_rx.latency.usec = *start;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 5: {
                      time_message_rx.latency.usec |= (int32_t) *start << 8;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 6: {
                      time_message_rx.latency.usec |= (int32_t) *start << 16;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;

                      internalState++;

                      if (len == 0) {
                        break;
                      }
                    }

                    case 7: {
                      time_message_rx.latency.usec |= (int32_t) *start << 24;

                      typedMsgCurrentPos++;
                      start++;
                      // currentPos++;
                      len--;
                      if (typedMsgCurrentPos >= base_message_rx.size) {
                        // ESP_LOGI(TAG, "done time message");

                        typedMsgCurrentPos = 0;

                        state = BASE_MESSAGE_STATE;
                        internalState = 0;

                        trx = (int64_t) base_message_rx.received.sec * 1000000LL +
                              (int64_t) base_message_rx.received.usec;
                        ttx = (int64_t) base_message_rx.sent.sec * 1000000LL + (int64_t) base_message_rx.sent.usec;
                        tdif = trx - ttx;
                        trx =
                            (int64_t) time_message_rx.latency.sec * 1000000LL + (int64_t) time_message_rx.latency.usec;
                        tmpDiffToServer = (trx - tdif) / 2;

                        int64_t diff;

                        // clear diffBuffer if last update is
                        // older than a minute
                        diff = now - lastTimeSync;
                        if (diff > 60000000LL) {
                          ESP_LOGW(TAG, "Last time sync older "
                                        "than a minute. "
                                        "Clearing time buffer");

                          reset_latency_buffer();

                          timeout = FAST_SYNC_LATENCY_BUF;

                          esp_timer_stop(timeSyncMessageTimer);
                          if (received_header == true) {
                            if (!esp_timer_is_active(timeSyncMessageTimer)) {
                              esp_timer_start_periodic(timeSyncMessageTimer, timeout);
                            }
                          }
                        }

                        player_latency_insert(tmpDiffToServer);

                        // ESP_LOGI(TAG, "Current latency:%lld:",
                        // tmpDiffToServer);

                        // store current time
                        lastTimeSync = now;

                        if (received_header == true) {
                          if (!esp_timer_is_active(timeSyncMessageTimer)) {
                            esp_timer_start_periodic(timeSyncMessageTimer, timeout);
                          }

                          bool is_full = false;
                          latency_buffer_full(&is_full, portMAX_DELAY);
                          if ((is_full == true) && (timeout < NORMAL_SYNC_LATENCY_BUF)) {
                            timeout = NORMAL_SYNC_LATENCY_BUF;

                            ESP_LOGI(TAG, "latency buffer full");

                            if (esp_timer_is_active(timeSyncMessageTimer)) {
                              esp_timer_stop(timeSyncMessageTimer);
                            }

                            esp_timer_start_periodic(timeSyncMessageTimer, timeout);
                          } else if ((is_full == false) && (timeout > FAST_SYNC_LATENCY_BUF)) {
                            timeout = FAST_SYNC_LATENCY_BUF;

                            ESP_LOGI(TAG, "latency buffer not full");

                            if (esp_timer_is_active(timeSyncMessageTimer)) {
                              esp_timer_stop(timeSyncMessageTimer);
                            }

                            esp_timer_start_periodic(timeSyncMessageTimer, timeout);
                          }
                        }
                      } else {
                        ESP_LOGE(TAG,
                                 "error time message, this "
                                 "shouldn't happen! %d %ld",
                                 typedMsgCurrentPos, base_message_rx.size);

                        typedMsgCurrentPos = 0;

                        state = BASE_MESSAGE_STATE;
                        internalState = 0;
                      }

                      break;
                    }

                    default: {
                      ESP_LOGE(TAG,
                               "time message decoder shouldn't "
                               "get here %d %ld %ld",
                               typedMsgCurrentPos, base_message_rx.size, internalState);

                      break;
                    }
                  }

                  break;
                }

                default: {
                  typedMsgCurrentPos++;
                  start++;
                  // currentPos++;
                  len--;

                  if (typedMsgCurrentPos >= base_message_rx.size) {
                    ESP_LOGI(TAG, "done unknown typed message %d", base_message_rx.type);

                    state = BASE_MESSAGE_STATE;
                    internalState = 0;

                    typedMsgCurrentPos = 0;
                  }

                  break;
                }
              }

              break;
            }

            default: {
              break;
            }
          }

          if (rc1 != ERR_OK) {
            break;
          }
        }
      } while (netbuf_next(firstNetBuf) >= 0);

      netbuf_delete(firstNetBuf);

      if (rc1 != ERR_OK) {
        ESP_LOGE(TAG, "Data error, closing netconn");

        netconn_close(lwipNetconn);

        break;
      }
    }
  }
}

}  // namespace snapclient
}  // namespace esphome

#endif
#endif
