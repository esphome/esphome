var group__group__board__libs =
[
    [ "xensiv_dps3xx_config_t", "group__group__board__libs.html#structxensiv__dps3xx__config__t", [
      [ "dev_mode", "group__group__board__libs.html#a9bc857866fe9e53b9cfb14877b716806", null ],
      [ "pressure_rate", "group__group__board__libs.html#a564c597b64e379d64f87e40a29e3cedc", null ],
      [ "temperature_rate", "group__group__board__libs.html#a098258956b8cd8fac13cb96d27e6f380", null ],
      [ "pressure_oversample", "group__group__board__libs.html#ad35ab3377cb6752d200e4452e47d1f54", null ],
      [ "temperature_oversample", "group__group__board__libs.html#a96eb8100c59d9e8844bbe545aac1298f", null ],
      [ "interrupt_triggers", "group__group__board__libs.html#a6d9a5c116ead8d5f6a0a40753fffb036", null ],
      [ "fifo_enable", "group__group__board__libs.html#a350507231575dd078daf9af1c2be1181", null ],
      [ "data_timeout", "group__group__board__libs.html#a16ede26c8d6888c9590ef85fcc3da5fb", null ],
      [ "i2c_timeout", "group__group__board__libs.html#a037b7f25ded2298380454133e2e414d2", null ]
    ] ],
    [ "xensiv_dps3xx_i2c_comm_t", "group__group__board__libs.html#structxensiv__dps3xx__i2c__comm__t", [
      [ "read", "group__group__board__libs.html#a20bb7919c71415036542152f087bafbe", null ],
      [ "write", "group__group__board__libs.html#a9f0f7543ccefefce49c5ea1c723066d9", null ],
      [ "delay", "group__group__board__libs.html#a6bcb44d755e20aadbd4e53698dce5671", null ],
      [ "context", "group__group__board__libs.html#aafc3b21a1ebc68151c27418bc6b068a1", null ]
    ] ],
    [ "xensiv_dps3xx_t", "group__group__board__libs.html#structxensiv__dps3xx__t", [
      [ "comm", "group__group__board__libs.html#a02f723782d87ee5bfb2d7d2ce469dc0d", null ],
      [ "user_config", "group__group__board__libs.html#a14d1c7c3c12c3d2a1c1c877cfd52f0d9", null ],
      [ "i2c_address", "group__group__board__libs.html#a0441c0ae7fbe99284d3a509061b38649", null ],
      [ "temp_scaled", "group__group__board__libs.html#abbe4a6db7840e7d84ab9b0a83b85a6a1", null ],
      [ "meas_cfg", "group__group__board__libs.html#a575da846e379018961862b25bbe5ed2f", null ],
      [ "tmp_osr_scale_coeff", "group__group__board__libs.html#ae6b516406a4edda52e218a401d7642cc", null ],
      [ "prs_osr_scale_coeff", "group__group__board__libs.html#a6540ec2f68b4837d54cd1cbce39bd71b", null ],
      [ "calib_coeffs", "group__group__board__libs.html#a486fb33dceec704100acd4ae88f2ba50", null ],
      [ "tmp_ext", "group__group__board__libs.html#a2995f7ce7b79a8398dafdfe41cd43789", null ]
    ] ],
    [ "XENSIV_DPS3XX_RSLT_ERR_WRITE_TOO_LARGE", "group__group__board__libs.html#gacadecd3b20432fe5460f32627b021865", null ],
    [ "XENSIV_DPS3XX_RSLT_ERR_DATA_NOT_READY", "group__group__board__libs.html#gac3c4b7bc4b618ff2de4bbd6533a7e1f6", null ],
    [ "xensiv_dps3xx_i2c_read_t", "group__group__board__libs.html#ga5026ff94c8abcf470391ab86c1cd467e", null ],
    [ "xensiv_dps3xx_i2c_write_t", "group__group__board__libs.html#ga349d54fbbb82a15c1e01b38ec092f443", null ],
    [ "xensiv_dps3xx_delay_t", "group__group__board__libs.html#ga92c2bce90f9f8ca2d6dbcc4e6834e859", null ],
    [ "xensiv_dps3xx_i2c_addr_t", "group__group__board__libs.html#ga788a5c414e6bf81811f38bd5d776774c", [
      [ "XENSIV_DPS3XX_I2C_ADDR_DEFAULT", "group__group__board__libs.html#gga788a5c414e6bf81811f38bd5d776774ca83077c0196f24e69990a6cbb91de4d58", null ],
      [ "XENSIV_DPS3XX_I2C_ADDR_ALT", "group__group__board__libs.html#gga788a5c414e6bf81811f38bd5d776774caecc199babe92b6855d3e21a8d15c2991", null ]
    ] ],
    [ "xensiv_dps3xx_mode_t", "group__group__board__libs.html#gae46378d0671f785477c887caa6d7ce7b", [
      [ "XENSIV_DPS3XX_MODE_IDLE", "group__group__board__libs.html#ggae46378d0671f785477c887caa6d7ce7ba606d167e09f48d3fba8abfc783d9de6c", null ],
      [ "XENSIV_DPS3XX_MODE_COMMAND_PRESSURE", "group__group__board__libs.html#ggae46378d0671f785477c887caa6d7ce7ba053a4777b41d6b53765f964973718948", null ],
      [ "XENSIV_DPS3XX_MODE_COMMAND_TEMPERATURE", "group__group__board__libs.html#ggae46378d0671f785477c887caa6d7ce7bac6964b2e502951d8889e1b76a672bea0", null ],
      [ "XENSIV_DPS3XX_MODE_BACKGROUND_PRESSURE", "group__group__board__libs.html#ggae46378d0671f785477c887caa6d7ce7ba259e0bffcf8f950503985cf6fded5f65", null ],
      [ "XENSIV_DPS3XX_MODE_BACKGROUND_TEMPERATURE", "group__group__board__libs.html#ggae46378d0671f785477c887caa6d7ce7ba5acbaa8ddc56334ee42186d837179511", null ],
      [ "XENSIV_DPS3XX_MODE_BACKGROUND_ALL", "group__group__board__libs.html#ggae46378d0671f785477c887caa6d7ce7ba81702de6a624340a367163c99f64aad7", null ]
    ] ],
    [ "xensiv_dps3xx_interrupt_t", "group__group__board__libs.html#ga7f4cd1eb12314503556e214b4abc4c1e", [
      [ "XENSIV_DPS3XX_INT_NONE", "group__group__board__libs.html#gga7f4cd1eb12314503556e214b4abc4c1ea128bff80eb1f13e7ff7a63cf5fcf9595", null ],
      [ "XENSIV_DPS3XX_INT_HL", "group__group__board__libs.html#gga7f4cd1eb12314503556e214b4abc4c1ea6c32f4e8291558c30b016ed92fbc6c25", null ],
      [ "XENSIV_DPS3XX_INT_FIFO", "group__group__board__libs.html#gga7f4cd1eb12314503556e214b4abc4c1eafcb5f5095d79722bf3e1172371bf6510", null ],
      [ "XENSIV_DPS3XX_INT_TMP", "group__group__board__libs.html#gga7f4cd1eb12314503556e214b4abc4c1ea4706bb239e887b493088896247655ef1", null ],
      [ "XENSIV_DPS3XX_INT_PRS", "group__group__board__libs.html#gga7f4cd1eb12314503556e214b4abc4c1eaebc124b1e2285db0158096b375931f53", null ]
    ] ],
    [ "xensiv_dps3xx_oversample_t", "group__group__board__libs.html#gac62e1f63f2b4192729b4d81f2915de09", [
      [ "XENSIV_DPS3XX_OVERSAMPLE_1", "group__group__board__libs.html#ggac62e1f63f2b4192729b4d81f2915de09a692de31fdcd054e0693b34962e03d1b6", null ],
      [ "XENSIV_DPS3XX_OVERSAMPLE_2", "group__group__board__libs.html#ggac62e1f63f2b4192729b4d81f2915de09a282393a53cc8399e38b6cf2648dd7dbd", null ],
      [ "XENSIV_DPS3XX_OVERSAMPLE_4", "group__group__board__libs.html#ggac62e1f63f2b4192729b4d81f2915de09acf370f3b49854f635f13721e6249fdf1", null ],
      [ "XENSIV_DPS3XX_OVERSAMPLE_8", "group__group__board__libs.html#ggac62e1f63f2b4192729b4d81f2915de09ac086fdbc50da33a3b82db23c4f062502", null ],
      [ "XENSIV_DPS3XX_OVERSAMPLE_16", "group__group__board__libs.html#ggac62e1f63f2b4192729b4d81f2915de09a8e96f9da8d153c2dd9db3124c62ae5c2", null ],
      [ "XENSIV_DPS3XX_OVERSAMPLE_32", "group__group__board__libs.html#ggac62e1f63f2b4192729b4d81f2915de09a057487233ce3e5f5e3fbd285aa7ab175", null ],
      [ "XENSIV_DPS3XX_OVERSAMPLE_64", "group__group__board__libs.html#ggac62e1f63f2b4192729b4d81f2915de09aad3d638ea6d2e70e316937bedb77d3ed", null ],
      [ "XENSIV_DPS3XX_OVERSAMPLE_128", "group__group__board__libs.html#ggac62e1f63f2b4192729b4d81f2915de09af403f2f6b9ce9825fbb6673813673daf", null ]
    ] ],
    [ "xensiv_dps3xx_rate_t", "group__group__board__libs.html#gaa0efe68320a8e8beef69e07dab55c555", [
      [ "XENSIV_DPS3XX_RATE_1", "group__group__board__libs.html#ggaa0efe68320a8e8beef69e07dab55c555a19658bf5bf2e316b1e89ef5182cff42f", null ],
      [ "XENSIV_DPS3XX_RATE_2", "group__group__board__libs.html#ggaa0efe68320a8e8beef69e07dab55c555a7b926d844b5ab5444881e1a9a2aad13e", null ],
      [ "XENSIV_DPS3XX_RATE_4", "group__group__board__libs.html#ggaa0efe68320a8e8beef69e07dab55c555aa751ed1e30b4fb7b32a742496b969d81", null ],
      [ "XENSIV_DPS3XX_RATE_8", "group__group__board__libs.html#ggaa0efe68320a8e8beef69e07dab55c555a45d6441bb997ee89747b48215232b380", null ],
      [ "XENSIV_DPS3XX_RATE_16", "group__group__board__libs.html#ggaa0efe68320a8e8beef69e07dab55c555a4b3ecc473b95b0da5a838f3379900e7e", null ],
      [ "XENSIV_DPS3XX_RATE_32", "group__group__board__libs.html#ggaa0efe68320a8e8beef69e07dab55c555aadb9f4d4a21b5d5a0c59c56ee3ab1d75", null ],
      [ "XENSIV_DPS3XX_RATE_64", "group__group__board__libs.html#ggaa0efe68320a8e8beef69e07dab55c555ae58fb6b4296293af19524675e4160046", null ],
      [ "XENSIV_DPS3XX_RATE_128", "group__group__board__libs.html#ggaa0efe68320a8e8beef69e07dab55c555ac6cd91842c22d3fa7c572284933b991a", null ]
    ] ],
    [ "mtb_xensiv_dps3xx_init_i2c", "group__group__board__libs.html#ga4fe86e5b638b41c99fde96b0153efb30", null ],
    [ "mtb_xensiv_dps3xx_reg_read", "group__group__board__libs.html#ga298d65417261594c3c7ee0a504adb265", null ],
    [ "mtb_xensiv_dps3xx_reg_write", "group__group__board__libs.html#ga33b7b0a5864efd53b23aea745090c7e0", null ],
    [ "mtb_xensiv_dps3xx_calc_temperature", "group__group__board__libs.html#gaf9a907b86d536d3cead47bd2264cdc84", null ],
    [ "mtb_xensiv_dps3xx_calc_pressure", "group__group__board__libs.html#gadb41af406f5147a09904b51a1d791243", null ],
    [ "xensiv_dps3xx_init_i2c", "group__group__board__libs.html#gaa2b10dc2493b218911352227d71b370c", null ],
    [ "xensiv_dps3xx_get_config", "group__group__board__libs.html#ga2e4efdcf568b410cbf4cc4d089464fce", null ],
    [ "xensiv_dps3xx_set_config", "group__group__board__libs.html#ga43a72071921d3059a68342f486a21d05", null ],
    [ "xensiv_dps3xx_get_revision_id", "group__group__board__libs.html#ga57f74c704f6b0885e66900a738af3947", null ],
    [ "xensiv_dps3xx_read", "group__group__board__libs.html#gaf1b8cb406b43a8732fe365dda37558d0", null ],
    [ "xensiv_dps3xx_check_ready", "group__group__board__libs.html#gabec006d96526dc6cf237b239abc2a96d", null ],
    [ "xensiv_dps3xx_free", "group__group__board__libs.html#gac7b9471f1027ff38e38503eaaf570b6f", null ],
    [ "_xensiv_dps3xx_reg_read", "group__group__board__libs.html#ga3ea9c368a3d42802edbab04451be728f", null ],
    [ "_xensiv_dps3xx_reg_write", "group__group__board__libs.html#ga070366e4effebd1a1d7eba0bba6ca103", null ],
    [ "_xensiv_dps3xx_calc_temperature", "group__group__board__libs.html#ga82efb4ea32412698adbfbdbb2fc88a73", null ],
    [ "_xensiv_dps3xx_calc_pressure", "group__group__board__libs.html#ga168b3f6eb6b8ed726168d78b3c806f92", null ]
];
