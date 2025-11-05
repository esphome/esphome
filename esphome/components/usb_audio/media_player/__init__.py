from pathlib import Path

from esphome import automation
import esphome.codegen as cg
from esphome.components import audio, esp32, media_player, psram
import esphome.components.speaker.media_player as speaker_mp
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUFFER_SIZE,
    CONF_FILES,
    CONF_FORMAT,
    CONF_ID,
    CONF_NUM_CHANNELS,
    CONF_RAW_DATA_ID,
    CONF_SAMPLE_RATE,
    CONF_SPEAKER,
    CONF_TASK_STACK_IN_PSRAM,
)
from esphome.core import HexInt

from .. import CONF_USB_AUDIO_ID, USBAudioComponent, usb_audio_ns

CODEOWNERS = ["@ab-tools"]
DEPENDENCIES = ["usb_audio", "speaker"]
CONFLICTS_WITH = ["speaker.media_player"]


def AUTO_LOAD(config):
    return speaker_mp.AUTO_LOAD(config)


SpeakerMediaPlayerBase = speaker_mp.SpeakerMediaPlayer

AudioStreamInfo = audio.audio_ns.class_("AudioStreamInfo")

USBAudioMediaPlayer = usb_audio_ns.class_(
    "USBAudioMediaPlayer",
    SpeakerMediaPlayerBase,
    cg.Parented.template(USBAudioComponent),
)


CONFIG_SCHEMA = cv.All(
    media_player.media_player_schema(USBAudioMediaPlayer).extend(
        {
            cv.GenerateID(CONF_USB_AUDIO_ID): cv.use_id(USBAudioComponent),
            cv.Required(
                speaker_mp.CONF_ANNOUNCEMENT_PIPELINE
            ): speaker_mp.PIPELINE_SCHEMA,
            cv.Optional(speaker_mp.CONF_MEDIA_PIPELINE): speaker_mp.PIPELINE_SCHEMA,
            cv.Optional(CONF_BUFFER_SIZE, default=1_000_000): cv.int_range(
                min=4000, max=4_000_000
            ),
            cv.Optional(
                speaker_mp.CONF_CODEC_SUPPORT_ENABLED, default=psram.supported()
            ): cv.boolean,
            cv.Optional(CONF_FILES): cv.ensure_list(speaker_mp.MEDIA_FILE_TYPE_SCHEMA),
            cv.Optional(CONF_TASK_STACK_IN_PSRAM, default=False): cv.boolean,
            cv.Optional(speaker_mp.CONF_VOLUME_INCREMENT, default=0.05): cv.percentage,
            cv.Optional(speaker_mp.CONF_VOLUME_INITIAL, default=0.5): cv.percentage,
            cv.Optional(speaker_mp.CONF_VOLUME_MAX, default=1.0): cv.percentage,
            cv.Optional(speaker_mp.CONF_VOLUME_MIN, default=0.0): cv.percentage,
            cv.Optional(speaker_mp.CONF_ON_MUTE): automation.validate_automation(
                single=True
            ),
            cv.Optional(speaker_mp.CONF_ON_UNMUTE): automation.validate_automation(
                single=True
            ),
            cv.Optional(speaker_mp.CONF_ON_VOLUME): automation.validate_automation(
                single=True
            ),
        }
    ),
    cv.only_with_esp_idf,
    speaker_mp._validate_repeated_speaker,
)


FINAL_VALIDATE_SCHEMA = speaker_mp.FINAL_VALIDATE_SCHEMA


async def to_code(config):
    components_dir = Path(__file__).resolve().parents[2]
    cg.add_build_flag(f'-I"{components_dir}"')

    if config[speaker_mp.CONF_CODEC_SUPPORT_ENABLED]:
        cg.add_define("USE_AUDIO_FLAC_SUPPORT", True)
        cg.add_define("USE_AUDIO_MP3_SUPPORT", True)

        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM", 16)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM", 64)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM", 64)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_TX_ENABLED", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_TX_BA_WIN", 32)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_AMPDU_RX_ENABLED", True)
        esp32.add_idf_sdkconfig_option("CONFIG_ESP_WIFI_RX_BA_WIN", 32)

        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCP_SND_BUF_DEFAULT", 65534)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCP_WND_DEFAULT", 65534)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCP_RECVMBOX_SIZE", 64)
        esp32.add_idf_sdkconfig_option("CONFIG_LWIP_TCPIP_RECVMBOX_SIZE", 64)

        esp32.add_idf_sdkconfig_option("CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP", True)

    var = await media_player.new_media_player(config)
    await cg.register_component(var, config)

    await cg.register_parented(var, config[CONF_USB_AUDIO_ID])

    cg.add_define("USE_OTA_STATE_CALLBACK")

    cg.add(var.set_buffer_size(config[CONF_BUFFER_SIZE]))

    cg.add(var.set_task_stack_in_psram(config[CONF_TASK_STACK_IN_PSRAM]))
    if config[CONF_TASK_STACK_IN_PSRAM]:
        esp32.add_idf_sdkconfig_option(
            "CONFIG_SPIRAM_ALLOW_STACK_EXTERNAL_MEMORY", True
        )

    cg.add(var.set_volume_increment(config[speaker_mp.CONF_VOLUME_INCREMENT]))
    cg.add(var.set_volume_initial(config[speaker_mp.CONF_VOLUME_INITIAL]))
    cg.add(var.set_volume_max(config[speaker_mp.CONF_VOLUME_MAX]))
    cg.add(var.set_volume_min(config[speaker_mp.CONF_VOLUME_MIN]))

    announcement_pipeline_config = config[speaker_mp.CONF_ANNOUNCEMENT_PIPELINE]
    ann_speaker = await cg.get_variable(announcement_pipeline_config[CONF_SPEAKER])
    cg.add(var.set_announcement_speaker(ann_speaker))
    ann_sample_rate = announcement_pipeline_config.get(CONF_SAMPLE_RATE)
    ann_channels = announcement_pipeline_config.get(CONF_NUM_CHANNELS)
    if ann_sample_rate is not None and ann_channels is not None:
        cg.add(
            ann_speaker.set_audio_stream_info(
                AudioStreamInfo(16, ann_channels, ann_sample_rate)
            )
        )
    if announcement_pipeline_config[CONF_FORMAT] != "NONE":
        cg.add(
            var.set_announcement_format(
                speaker_mp._get_supported_format_struct(
                    announcement_pipeline_config, "ANNOUNCEMENT"
                )
            )
        )

    if media_pipeline_config := config.get(speaker_mp.CONF_MEDIA_PIPELINE):
        media_spk = await cg.get_variable(media_pipeline_config[CONF_SPEAKER])
        cg.add(var.set_media_speaker(media_spk))
        media_sample_rate = media_pipeline_config.get(CONF_SAMPLE_RATE)
        media_channels = media_pipeline_config.get(CONF_NUM_CHANNELS)
        if media_sample_rate is not None and media_channels is not None:
            cg.add(
                media_spk.set_audio_stream_info(
                    AudioStreamInfo(16, media_channels, media_sample_rate)
                )
            )
        if media_pipeline_config[CONF_FORMAT] != "NONE":
            cg.add(
                var.set_media_format(
                    speaker_mp._get_supported_format_struct(
                        media_pipeline_config, "MEDIA"
                    )
                )
            )

    if on_mute := config.get(speaker_mp.CONF_ON_MUTE):
        await automation.build_automation(var.get_mute_trigger(), [], on_mute)
    if on_unmute := config.get(speaker_mp.CONF_ON_UNMUTE):
        await automation.build_automation(var.get_unmute_trigger(), [], on_unmute)
    if on_volume := config.get(speaker_mp.CONF_ON_VOLUME):
        await automation.build_automation(
            var.get_volume_trigger(), [(cg.float_, "x")], on_volume
        )

    for file_config in config.get(CONF_FILES, []):
        data, media_file_type = speaker_mp._read_audio_file_and_type(file_config)

        rhs = [HexInt(x) for x in data]
        prog_arr = cg.progmem_array(file_config[CONF_RAW_DATA_ID], rhs)

        media_files_struct = cg.StructInitializer(
            audio.AudioFile,
            ("data", prog_arr),
            ("length", len(rhs)),
            ("file_type", media_file_type),
        )

        cg.new_Pvariable(file_config[CONF_ID], media_files_struct)
