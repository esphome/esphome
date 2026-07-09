set(GMF_AI_AUDIO_SUPPORTED_TARGETS
    esp32 esp32c3 esp32c5 esp32c6 esp32p4 esp32s2 esp32s3 esp32s31
)
if(IDF_TARGET IN_LIST GMF_AI_AUDIO_SUPPORTED_TARGETS)
    set(srcs "./src/esp_gmf_wn.c"
             "./src/esp_gmf_aec.c"
             "./src/esp_gmf_afe_manager.c"
             "./src/esp_gmf_afe.c"
             "./src/esp_gmf_doa.c"
             "./src/esp_gmf_vad.c"
             "./src/esp_gmf_ns.c"
             )

    idf_component_register(
        SRCS "${srcs}"
        INCLUDE_DIRS "./include"
        PRIV_INCLUDE_DIRS "./private_include"
        REQUIRES esp-sr esp_timer
    )
endif()
