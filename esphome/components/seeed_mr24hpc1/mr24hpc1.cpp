#include "esphome/core/log.h"
#include "mr24hpc1.h"

#include <utility>
#ifdef USE_NUMBER
#include "esphome/components/number/number.h"
#endif
#ifdef USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

namespace esphome {
namespace mr24hpc1 {

static const char *TAG = "mr24hpc1";

// 计算CRC校验码
static uint8_t get_frame_crc_sum(uint8_t *data, int len)
{
    unsigned int crc_sum = 0;
    for (int i = 0; i < len - 3; i++)
    {
        crc_sum += data[i];
    }
    return crc_sum & 0xff;
}

// 检查校验码是否正确
static int get_frame_check_status(uint8_t *data, int len)
{
    uint8_t crc_sum = get_frame_crc_sum(data, len);
    uint8_t verified = data[len - 3];
    return (verified == crc_sum) ? 1 : 0;
}

// 打印数据帧
static void show_frame_data(uint8_t *data, int len)
{
    printf("[%s] FRAME: %d, ", __FUNCTION__, len);
    for (int i = 0; i < len; i++)
    {
        printf("%02X ", data[i] & 0xff);
    }
    printf("\r\n");
}

// 打印组件的配置数据。dump_config()会以易读的格式将组件的所有配置项打印出来,包括配置的键值对。
void mr24hpc1Component::dump_config() { 
    ESP_LOGCONFIG(TAG, "MR24HPC1:");
#ifdef USE_TEXT_SENSOR
    LOG_TEXT_SENSOR("  ", "HeartbeatTextSensor", this->heartbeat_state_text_sensor_);
    LOG_TEXT_SENSOR(" ", "ProductModelTextSensor", this->product_model_text_sensor_);
    LOG_TEXT_SENSOR(" ", "ProductIDTextSensor", this->product_id_text_sensor_);
    LOG_TEXT_SENSOR(" ", "HardwareModelTextSensor", this->hardware_model_text_sensor_);
    LOG_TEXT_SENSOR(" ", "FirwareVerisonTextSensor", this->firware_version_text_sensor_);
    LOG_TEXT_SENSOR(" ", "KeepAwaySensor", this->keep_away_text_sensor_);
    LOG_TEXT_SENSOR(" ", "MotionStatusSensor", this->motion_status_text_sensor_);
#endif
#ifdef USE_BINARY_SENSOR
    LOG_BINARY_SENSOR(" ", "SomeoneExistsBinarySensor", this->someoneExists_binary_sensor_);
#endif
#ifdef USE_SENSOR
    LOG_SENSOR(" ", "CustomPresenceOfDetectionSensor", this->custom_presence_of_detection_sensor_);
#endif
}

// 初始化函数
void mr24hpc1Component::setup() {
    s_power_on_status = 0;
    sg_init_flag = true;
    ESP_LOGCONFIG(TAG, "uart_settings is 115200");
    this->check_uart_settings(115200);

    memset(this->c_product_mode, 0, PRODUCT_BUF_MAX_SIZE);
    memset(this->c_product_id, 0, PRODUCT_BUF_MAX_SIZE);
    memset(this->c_firmware_version, 0, PRODUCT_BUF_MAX_SIZE);
    memset(this->c_hardware_model, 0, PRODUCT_BUF_MAX_SIZE);
}

// 组件回调函数,它会在每次循环被调用
void mr24hpc1Component::update() {
    if (!sg_init_flag)                // setup函数执行完毕
        return;
    if (sg_init_flag && (255 != sg_heartbeat_flag))  // sg_heartbeat_flag初值是255，所以首次不执行，先执行上电检查的内容
    {
        this->heartbeat_state_text_sensor_->publish_state(s_heartbeat_str[sg_heartbeat_flag]);
        sg_heartbeat_flag = 0;
    }
    if (s_power_on_status < 4)  // 上电后状态检查
    {
        if (s_output_info_switch_flag == OUTPUT_SWITCH_INIT)  // 上电状态检查第一项
        {
            sg_start_query_data = CUSTOM_FUNCTION_QUERY_RADAR_OUITPUT_INFORMATION_SWITCH;  // 自定义函数查询雷达输出信息开关
            sg_start_query_data_max = CUSTOM_FUNCTION_MAX;
        }
        else if (s_output_info_switch_flag == OUTPUT_SWTICH_OFF)  // 当底层开放参数按钮关闭，上电状态检查第二项
        {
            sg_start_query_data = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;
            sg_start_query_data_max = STANDARD_FUNCTION_MAX;
        }
        else if (s_output_info_switch_flag == OUTPUT_SWTICH_ON)   // 当底层开放参数按钮开启，上电状态检查第二项
        {
            sg_start_query_data = CUSTOM_FUNCTION_QUERY_RADAR_OUITPUT_INFORMATION_SWITCH;
            sg_start_query_data_max = CUSTOM_FUNCTION_MAX;
        }
        s_power_on_status++;  // 共有四项内容检查
    }
    else
    {
        sg_start_query_data = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;
        sg_start_query_data_max = STANDARD_FUNCTION_QUERY_HARDWARE_MODE;
    }
}

// 主循环
void mr24hpc1Component::loop() {
    uint8_t byte;

    // 串口是否有数据
    while (this->available())
    {
        this->read_byte(&byte);
        this->R24_split_data_frame(byte);  // 拆分数据帧
    }

    // !s_output_info_switch_flag = !OUTPUT_SWITCH_INIT = !0 = 1  (上电检查第一项——检查是否开启底层开放参数)
    if (!s_output_info_switch_flag && sg_start_query_data == CUSTOM_FUNCTION_QUERY_RADAR_OUITPUT_INFORMATION_SWITCH)
    {
        // 检查底层开放参数的按钮是否开启，如果开启
        this->get_radar_output_information_switch();  // 该函数配合R24_split_data_frame会改变s_output_info_switch_flag的状态，ON或OFF
        sg_start_query_data++;    // 此时 sg_start_query_data = CUSTOM_FUNCTION_QUERY_PRESENCE_OF_DETECTION_RANGE  sg_start_query_data_max = CUSTOM_FUNCTION_MAX
    }
    // 当底层开放参数的开关处于关闭状态，sg_start_query_data的值应该在限定范围之内
    if ((s_output_info_switch_flag == OUTPUT_SWTICH_OFF) && (sg_start_query_data <= sg_start_query_data_max) && (sg_start_query_data >= STANDARD_FUNCTION_QUERY_PRODUCT_MODE))
    {
        switch (sg_start_query_data)
        {
            case STANDARD_FUNCTION_QUERY_PRODUCT_MODE:
                if (strlen(this->c_product_mode) > 0)
                {
                    this->product_model_text_sensor_->publish_state(this->c_product_mode);  // 发布产品型号
                }
                else
                {
                    this->get_product_mode();  // 查询产品型号
                }
                break;
            case STANDARD_FUNCTION_QUERY_PRODUCT_ID:
                if (strlen(this->c_product_id) > 0)
                {
                    this->product_id_text_sensor_->publish_state(this->c_product_id);  // 发布产品ID
                }
                else
                {
                    this->get_product_id();  // 查询产品ID
                }
                break;
            case STANDARD_FUNCTION_QUERY_FIRMWARE_VERDION:
                if (strlen(this->c_firmware_version) > 0)
                {
                    this->firware_version_text_sensor_->publish_state(this->c_firmware_version);  // 发布固件版本号
                }
                else
                {
                    this->get_firmware_version();  // 查询估计版本号
                }
                break;
            case STANDARD_FUNCTION_QUERY_HARDWARE_MODE:
                if (strlen(this->c_hardware_model) > 0)
                {
                    this->hardware_model_text_sensor_->publish_state(this->c_hardware_model);  // 发布硬件型号
                }
                else
                {
                    this->get_hardware_model();  // 查询硬件型号
                }
                break;
            case STANDARD_FUNCTION_MAX:
                this->get_heartbeat_packet();
                break;
        }
        sg_start_query_data++;
    }
    if (sg_start_query_data > CUSTOM_FUNCTION_MAX) sg_start_query_data = STANDARD_FUNCTION_QUERY_PRODUCT_MODE;
}

// 拆分数据帧
void mr24hpc1Component::R24_split_data_frame(uint8_t value)
{
    switch (sg_recv_data_state)
    {
        case FRAME_IDLE:                    // 初始值
            if (FRAME_HEADER1_VALUE == value)
            {
                sg_recv_data_state = FRAME_HEADER2;
            }
            break;
        case FRAME_HEADER2:
            if (FRAME_HEADER2_VALUE == value)
            {
                sg_frame_buf[0] = FRAME_HEADER1_VALUE;
                sg_frame_buf[1] = FRAME_HEADER2_VALUE;
                sg_recv_data_state = FRAME_CTL_WORLD;
            }
            else
            {
                sg_recv_data_state = FRAME_IDLE;
                ESP_LOGD(TAG, "FRAME_IDLE ERROR value:%x", value);
            }
            break;
        case FRAME_CTL_WORLD:
            sg_frame_buf[2] = value;
            sg_recv_data_state = FRAME_CMD_WORLD;
            break;
        case FRAME_CMD_WORLD:
            sg_frame_buf[3] = value;
            sg_recv_data_state = FRAME_DATA_LEN_H;
            break;
        case FRAME_DATA_LEN_H:
            if (value <= 4)
            {
                sg_data_len = value * 256;
                sg_frame_buf[4] = value;
                sg_recv_data_state = FRAME_DATA_LEN_L;
            }
            else
            {
                sg_data_len = 0;
                sg_recv_data_state = FRAME_IDLE;
                ESP_LOGD(TAG, "FRAME_DATA_LEN_H ERROR value:%x", value);
            }
            break;
        case FRAME_DATA_LEN_L:
            sg_data_len += value;
            if (sg_data_len > 32)
            {
                ESP_LOGD(TAG, "len=%d, FRAME_DATA_LEN_L ERROR value:%x", sg_data_len, value);
                sg_data_len = 0;
                sg_recv_data_state = FRAME_IDLE;
            }
            else
            {
                sg_frame_buf[5] = value;
                sg_frame_len = 6;
                sg_recv_data_state = FRAME_DATA_BYTES;
            }
            break;
        case FRAME_DATA_BYTES:
            sg_data_len -= 1;
            sg_frame_buf[sg_frame_len++] = value;
            if (sg_data_len <= 0)
            {
                sg_recv_data_state = FRAME_DATA_CRC;
            }
            break;
        case FRAME_DATA_CRC:
            sg_frame_buf[sg_frame_len++] = value;
            sg_recv_data_state = FRAME_TAIL1;
            break;
        case FRAME_TAIL1:
            if (FRAME_TAIL1_VALUE == value)
            {
                sg_recv_data_state = FRAME_TAIL2;
            }
            else
            {
                sg_recv_data_state = FRAME_IDLE;
                sg_frame_len = 0;
                sg_data_len = 0;
                ESP_LOGD(TAG, "FRAME_TAIL1 ERROR value:%x", value);
            }
            break;
        case FRAME_TAIL2:
            if (FRAME_TAIL2_VALUE == value)
            {
                sg_frame_buf[sg_frame_len++] = FRAME_TAIL1_VALUE;
                sg_frame_buf[sg_frame_len++] = FRAME_TAIL2_VALUE;
                memcpy(sg_frame_prase_buf, sg_frame_buf, sg_frame_len);
                if (get_frame_check_status(sg_frame_prase_buf, sg_frame_len))
                {
                    this->R24_parse_data_frame(sg_frame_prase_buf, sg_frame_len);
                }
                else
                {
                    ESP_LOGD(TAG, "frame check failer!");
                }
            }
            else
            {
                ESP_LOGD(TAG, "FRAME_TAIL2 ERROR value:%x", value);
            }
            memset(sg_frame_prase_buf, 0, FRAME_BUF_MAX_SIZE);
            memset(sg_frame_buf, 0, FRAME_BUF_MAX_SIZE);
            sg_frame_len = 0;
            sg_data_len = 0;
            sg_recv_data_state = FRAME_IDLE;
            break;
        default:
            sg_recv_data_state = FRAME_IDLE;
    }
}

// 解析产品信息相关的数据帧
void mr24hpc1Component::R24_frame_parse_product_Information(uint8_t *data)
{
    uint8_t product_len = 0;
    if (data[FRAME_COMMAND_WORD_INDEX] == 0xA1)
    {
        product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
        if (product_len < PRODUCT_BUF_MAX_SIZE)
        {
            memset(this->c_product_mode, 0, PRODUCT_BUF_MAX_SIZE);
            memcpy(this->c_product_mode, &data[FRAME_DATA_INDEX], product_len);
            ESP_LOGD(TAG, "Reply: get product_mode :%s", this->c_product_mode);
            this->product_model_text_sensor_->publish_state(this->c_product_mode);
        }
        else
        {
            ESP_LOGD(TAG, "Reply: get product_mode length too long!");
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0xA2)
    {
        product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
        if (product_len < PRODUCT_BUF_MAX_SIZE)
        {
            memset(this->c_product_id, 0, PRODUCT_BUF_MAX_SIZE);
            memcpy(this->c_product_id, &data[FRAME_DATA_INDEX], product_len);
            this->product_id_text_sensor_->publish_state(this->c_product_id);
            ESP_LOGD(TAG, "Reply: get productId :%s", this->c_product_id);
        }
        else
        {
            ESP_LOGD(TAG, "Reply: get productId length too long!");
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0xA3)
    {
        product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
        if (product_len < PRODUCT_BUF_MAX_SIZE)
        {
            memset(this->c_hardware_model, 0, PRODUCT_BUF_MAX_SIZE);
            memcpy(this->c_hardware_model, &data[FRAME_DATA_INDEX], product_len);
            this->hardware_model_text_sensor_->publish_state(this->c_hardware_model);
            ESP_LOGD(TAG, "Reply: get hardware_model :%s", this->c_hardware_model);
        }
        else
        {
            ESP_LOGD(TAG, "Reply: get hardwareModel length too long!");
        }
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0xA4)
    {
        product_len = data[FRAME_COMMAND_WORD_INDEX + 1] * 256 + data[FRAME_COMMAND_WORD_INDEX + 2];
        if (product_len < PRODUCT_BUF_MAX_SIZE)
        {

            memset(this->c_firmware_version, 0, PRODUCT_BUF_MAX_SIZE);
            memcpy(this->c_firmware_version, &data[FRAME_DATA_INDEX], product_len);
            this->firware_version_text_sensor_->publish_state(this->c_firmware_version);
            ESP_LOGD(TAG, "Reply: get firmware_version :%s", this->c_firmware_version);
        }
        else
        {
            ESP_LOGD(TAG, "Reply: get firmwareVersion length too long!");
        }
    }
}

// 解析底层开放参数
void mr24hpc1Component::R24_frame_parse_open_underlying_information(uint8_t *data)
{
    if (data[FRAME_COMMAND_WORD_INDEX] == 0x00)
    {
        // id(output_info_switch).publish_state(data[FRAME_DATA_INDEX]);  // 底层开放参数开光状态更新
        if (data[FRAME_DATA_INDEX])
        {
            s_output_info_switch_flag = OUTPUT_SWTICH_ON;
        }
        else
        {
            s_output_info_switch_flag = OUTPUT_SWTICH_OFF;
        }
        ESP_LOGD(TAG, "Reply: output switch %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x01)
    {
        // if (sg_spatial_static_value_bak != data[FRAME_DATA_INDEX])
        // {
        //     sg_spatial_static_value_bak = data[FRAME_DATA_INDEX];
        //     id(custom_spatial_static_value).publish_state(sg_spatial_static_value_bak);
        // }
        // if (sg_static_distance_bak != data[FRAME_DATA_INDEX + 1])
        // {
        //     sg_static_distance_bak = data[FRAME_DATA_INDEX + 1];
        //     id(custom_static_distance).publish_state(sg_static_distance_bak * 0.5);
        // }
        // if (sg_spatial_motion_value_bak != data[FRAME_DATA_INDEX + 2])
        // {
        //     sg_spatial_motion_value_bak = data[FRAME_DATA_INDEX + 2];
        //     id(custom_spatial_motion_value).publish_state(sg_spatial_motion_value_bak);
        // }
        // if (sg_motion_distance_bak != data[FRAME_DATA_INDEX + 3])
        // {
        //     sg_motion_distance_bak = data[FRAME_DATA_INDEX + 3];
        //     id(custom_motion_distance).publish_state(sg_motion_distance_bak * 0.5);
        // }
        // if (sg_motion_speed_bak != data[FRAME_DATA_INDEX + 4])
        // {
        //     sg_motion_speed_bak = data[FRAME_DATA_INDEX + 4];
        //     id(custom_motion_speed).publish_state((sg_motion_speed_bak - 10) * 0.5);
        // }
        // ESP_LOGD(TAG, "Reply: get output info %d  %d  %d  %d", data[FRAME_DATA_INDEX], data[FRAME_DATA_INDEX + 1], data[FRAME_DATA_INDEX + 2], data[FRAME_DATA_INDEX + 3]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x06)
    {
        // none:0x00  close_to:0x01  far_away:0x02
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->keep_away_text_sensor_->publish_state(s_keep_away_str[data[FRAME_DATA_INDEX]]);
        }
        ESP_LOGD(TAG, "Report:  moving direction  %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x07)
    {
        // if (sg_movementSigns_bak != data[FRAME_DATA_INDEX])
        // {
        //     this->movementSigns->publish_state(data[FRAME_DATA_INDEX]);
        //     sg_movementSigns_bak = data[FRAME_DATA_INDEX];
        // }
        // ESP_LOGD(TAG, "Report: get movementSigns %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x08)
    {
        // id(custom_judgment_threshold_exists).publish_state(data[FRAME_DATA_INDEX]);
        // ESP_LOGD(TAG, "Reply: set judgment threshold exists %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x09)
    {
        // id(custom_motion_amplitude_trigger_threshold).publish_state(data[FRAME_DATA_INDEX]);
        // ESP_LOGD(TAG, "Reply: set motion amplitude trigger threshold %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0a)
    {
        // if (id(custom_presence_of_perception_boundary).has_index(data[FRAME_DATA_INDEX] - 1))
        // {
        //     id(custom_presence_of_perception_boundary).publish_state(s_presence_of_perception_boundary_str[data[FRAME_DATA_INDEX] - 1]);
        // }
        // ESP_LOGD(TAG, "Reply: set presence awareness boundary %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0b)
    {
        // if (id(custom_motion_trigger_boundary).has_index(data[FRAME_DATA_INDEX] - 1))
        // {
        //     id(custom_motion_trigger_boundary).publish_state(s_motion_trig_boundary_str[data[FRAME_DATA_INDEX] - 1]);
        // }
        // ESP_LOGD(TAG, "Reply: set motion trigger boundary %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0c)
    {
        // uint32_t motion_trigger_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_motion_trigger_time_bak != motion_trigger_time)
        // {
        //     sg_motion_trigger_time_bak = motion_trigger_time;
        //     id(custom_motion_trigger_time).publish_state(motion_trigger_time);
        // }
        // ESP_LOGD(TAG, "Reply: set motion trigger time %u", motion_trigger_time);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0d)
    {
        // uint32_t move_to_rest_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_move_to_rest_time_bak != move_to_rest_time)
        // {
        //     id(custom_movement_to_rest_time).publish_state(move_to_rest_time);
        //     sg_move_to_rest_time_bak = move_to_rest_time;
        // }
        // ESP_LOGD(TAG, "Reply: set movement to rest time %u", move_to_rest_time);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0e)
    {
        // uint32_t enter_unmanned_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_enter_unmanned_time_bak != enter_unmanned_time)
        // {
        //     id(custom_time_of_enter_unmanned).publish_state(enter_unmanned_time);
        //     sg_enter_unmanned_time_bak = enter_unmanned_time;
        // }
        // ESP_LOGD(TAG, "Reply: set Time of entering unmanned state %u", enter_unmanned_time);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x80)
    {
        if (data[FRAME_DATA_INDEX])
        {
            s_output_info_switch_flag = OUTPUT_SWTICH_ON;
        }
        else
        {
            s_output_info_switch_flag = OUTPUT_SWTICH_OFF;
        }
        // id(output_info_switch).publish_state(data[FRAME_DATA_INDEX]);
        ESP_LOGD(TAG, "Reply: get output switch %d", data[FRAME_DATA_INDEX]);
    } 
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x81) {
        // if (sg_spatial_static_value_bak != data[FRAME_DATA_INDEX]) {
        //     sg_spatial_static_value_bak = data[FRAME_DATA_INDEX];
        //     id(custom_spatial_static_value).publish_state(sg_spatial_static_value_bak);
        // }
        // ESP_LOGD(TAG, "Reply: get spatial static value %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x82) {
        // if (sg_spatial_motion_value_bak != data[FRAME_DATA_INDEX]) {
        //     sg_spatial_motion_value_bak = data[FRAME_DATA_INDEX];
        //     id(custom_spatial_motion_value).publish_state(sg_spatial_motion_value_bak);
        // }
        // ESP_LOGD(TAG, "Reply: get spatial motion amplitude %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x83)
    {
        this->custom_presence_of_detection_sensor_->publish_state(s_presence_of_detection_range_str[data[FRAME_DATA_INDEX]]);
        ESP_LOGD(TAG, "Reply: get Presence of detection range %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x84) { 
        // sg_motion_distance_bak = data[FRAME_DATA_INDEX];
        // id(custom_motion_distance).publish_state(sg_motion_distance_bak * 0.5);
        // ESP_LOGD(TAG, "Report: get distance of moving object %lf", data[FRAME_DATA_INDEX]*0.5);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x85) {  
        // if (sg_motion_speed_bak != data[FRAME_DATA_INDEX]) {
        //     sg_motion_speed_bak = data[FRAME_DATA_INDEX];
        //     id(custom_motion_speed).publish_state((sg_motion_speed_bak - 10) * 0.5);
        // }
        // ESP_LOGD(TAG, "Reply: get target movement speed %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x86)
    {
        ESP_LOGD(TAG, "Reply: get keep_away %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x87)
    {
        // if (sg_movementSigns_bak != data[FRAME_DATA_INDEX])
        // {
        //     this->movementSigns->publish_state(data[FRAME_DATA_INDEX]);
        //     sg_movementSigns_bak = data[FRAME_DATA_INDEX];
        // }
        // ESP_LOGD(TAG, "Reply: get movementSigns %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x88)
    {
        // id(custom_judgment_threshold_exists).publish_state(data[FRAME_DATA_INDEX]);
        // ESP_LOGD(TAG, "Reply: get judgment threshold exists %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x89)
    {
        // id(custom_motion_amplitude_trigger_threshold).publish_state(data[FRAME_DATA_INDEX]);
        // ESP_LOGD(TAG, "Reply: get motion amplitude trigger threshold setting %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8a)
    {
        // if (id(custom_presence_of_perception_boundary).has_index(data[FRAME_DATA_INDEX] - 1))
        // {
        //     id(custom_presence_of_perception_boundary).publish_state(s_presence_of_perception_boundary_str[data[FRAME_DATA_INDEX] - 1]);
        // }
        // ESP_LOGD(TAG, "Reply: get presence awareness boundary %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8b)
    {
        // if (id(custom_motion_trigger_boundary).has_index(data[FRAME_DATA_INDEX] - 1))
        // {
        //     id(custom_motion_trigger_boundary).publish_state(s_motion_trig_boundary_str[data[FRAME_DATA_INDEX] - 1]);
        // }
        // ESP_LOGD(TAG, "Reply: get motion trigger boundary %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8c)
    {
        // uint32_t motion_trigger_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_motion_trigger_time_bak != motion_trigger_time)
        // {
        //     id(custom_motion_trigger_time).publish_state(motion_trigger_time);
        //     sg_motion_trigger_time_bak = motion_trigger_time;
        // }
        // ESP_LOGD(TAG, "Reply: get motion trigger time %u", motion_trigger_time);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8d)
    {
        // uint32_t move_to_rest_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_move_to_rest_time_bak != move_to_rest_time)
        // {
        //     id(custom_movement_to_rest_time).publish_state(move_to_rest_time);
        //     sg_move_to_rest_time_bak = move_to_rest_time;
        // }
        // ESP_LOGD(TAG, "Reply: get movement to rest time %u", move_to_rest_time);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8e)
    {
        // uint32_t enter_unmanned_time = (uint32_t)(data[FRAME_DATA_INDEX] << 24) + (uint32_t)(data[FRAME_DATA_INDEX + 1] << 16) + (uint32_t)(data[FRAME_DATA_INDEX + 2] << 8) + data[FRAME_DATA_INDEX + 3];
        // if (sg_enter_unmanned_time_bak != enter_unmanned_time)
        // {
        //     id(custom_time_of_enter_unmanned).publish_state(enter_unmanned_time);
        //     sg_enter_unmanned_time_bak = enter_unmanned_time;
        // }
        // ESP_LOGD(TAG, "Reply: get Time of entering unmanned state %u", enter_unmanned_time);
    }
}

// 解析数据帧
void mr24hpc1Component::R24_parse_data_frame(uint8_t *data, uint8_t len)
{
    switch (data[FRAME_CONTROL_WORD_INDEX])
    {
        case 0x01:
        {
            if (data[FRAME_COMMAND_WORD_INDEX] == 0x01)
            {
                ESP_LOGD(TAG, "Reply: query Heartbeat packet");
            }
            else if (data[FRAME_COMMAND_WORD_INDEX] == 0x02)
            {
                ESP_LOGD(TAG, "Reply: query reset packet");
            }
        }
        break;
        case 0x02:
        {
            this->R24_frame_parse_product_Information(data);
        }
        break;
        case 0x05:
        {
            // this->R24_frame_parse_work_status(data);
        }
        break;
        case 0x08:
        {
            this->R24_frame_parse_open_underlying_information(data);
        }
        break;
        case 0x80:
        {
            this->R24_frame_parse_human_information(data);
        }
        break;
        default:
            ESP_LOGD(TAG, "control world:0x%02X not found", data[FRAME_CONTROL_WORD_INDEX]);
        break;
    }
}

void mr24hpc1Component::R24_frame_parse_human_information(uint8_t *data)
{
    if (data[FRAME_COMMAND_WORD_INDEX] == 0x01)
    {
        this->someoneExists_binary_sensor_->publish_state(s_someoneExists_str[data[FRAME_DATA_INDEX]]);
        ESP_LOGD(TAG, "Report: someoneExists %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x02)
    {
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->motion_status_text_sensor_->publish_state(s_motion_status_str[data[FRAME_DATA_INDEX]]);
        }
        ESP_LOGD(TAG, "Report: motion_status %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x03)
    {
        // if (sg_movementSigns_bak != data[FRAME_DATA_INDEX])
        // {
        //     this->movementSigns->publish_state(data[FRAME_DATA_INDEX]);
        //     sg_movementSigns_bak = data[FRAME_DATA_INDEX];
        // }
        // ESP_LOGD(TAG, "Report: movementSigns %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0A)
    {
        // none:0x00  1s:0x01 30s:0x02 1min:0x03 2min:0x04 5min:0x05 10min:0x06 30min:0x07 1hour:0x08
        // if (data[FRAME_DATA_INDEX] < 9 && data[FRAME_DATA_INDEX] >= 0)
        // {
        //     id(unmanned_time).publish_state(s_unmanned_time_str[data[FRAME_DATA_INDEX]]);
        // }
        // ESP_LOGD(TAG, "Reply: set enter unmanned time %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x0B)
    {
        // none:0x00  close_to:0x01  far_away:0x02
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->keep_away_text_sensor_->publish_state(s_keep_away_str[data[FRAME_DATA_INDEX]]);
        }
        ESP_LOGD(TAG, "Report:  moving direction  %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x81)
    {
        this->someoneExists_binary_sensor_->publish_state(s_someoneExists_str[data[FRAME_DATA_INDEX]]);
        ESP_LOGD(TAG, "Reply: get someoneExists %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x82)
    {
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->motion_status_text_sensor_->publish_state(s_motion_status_str[data[FRAME_DATA_INDEX]]);
        }
        ESP_LOGD(TAG, "Reply: get motion_status %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x83)
    {
        // if (sg_movementSigns_bak != data[FRAME_DATA_INDEX])
        // {
        //     this->movementSigns->publish_state(data[FRAME_DATA_INDEX]);
        //     sg_movementSigns_bak = data[FRAME_DATA_INDEX];
        // }
        // ESP_LOGD(TAG, "Reply: get movementSigns %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8A)
    {
        // none:0x00  1s:0x01 30s:0x02 1min:0x03 2min:0x04 5min:0x05 10min:0x06 30min:0x07 1hour:0x08
        // if (data[FRAME_DATA_INDEX] < 9 && data[FRAME_DATA_INDEX] >= 0)
        // {
        //     id(unmanned_time).publish_state(s_unmanned_time_str[data[FRAME_DATA_INDEX]]);
        // }
        // ESP_LOGD(TAG, "Report: get enter unmanned time %d", data[FRAME_DATA_INDEX]);
    }
    else if (data[FRAME_COMMAND_WORD_INDEX] == 0x8B)
    {
        // none:0x00  close_to:0x01  far_away:0x02
        if (data[FRAME_DATA_INDEX] < 3 && data[FRAME_DATA_INDEX] >= 0)
        {
            this->keep_away_text_sensor_->publish_state(s_keep_away_str[data[FRAME_DATA_INDEX]]);
        }
        ESP_LOGD(TAG, "Reply: get moving direction  %d", data[FRAME_DATA_INDEX]);
    }
    else
    {
        ESP_LOGD(TAG, "[%s] No found COMMAND_WORD(%02X) in Frame", __FUNCTION__, data[FRAME_COMMAND_WORD_INDEX]);
    }
}

// 发送数据帧
void mr24hpc1Component::send_query(uint8_t *query, size_t string_length)
{
    int i;
    for (i = 0; i < string_length; i++)
    {
        write(query[i]);
    }
    show_frame_data(query, i);
}

// 下发心跳包命令
void mr24hpc1Component::get_heartbeat_packet(void)
{
    uint8_t send_data_len = 10;
    uint8_t send_data[10] = {0x53, 0x59, 0x01, 0x01, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// 下发底层开放参数查询命令
void mr24hpc1Component::get_radar_output_information_switch(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x08, 0x80, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// 下发产品型号命令
void mr24hpc1Component::get_product_mode(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA1, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// 下发获得产品ID命令
void mr24hpc1Component::get_product_id(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA2, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// 下发硬件型号命令
void mr24hpc1Component::get_hardware_model(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA3, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

// 下发软件版本命令
void mr24hpc1Component::get_firmware_version(void)
{
    unsigned char send_data_len = 10;
    unsigned char send_data[10] = {0x53, 0x59, 0x02, 0xA4, 0x00, 0x01, 0x0F, 0x00, 0x54, 0x43};
    send_data[FRAME_DATA_INDEX + 1] = get_frame_crc_sum(send_data, send_data_len);
    this->send_query(send_data, send_data_len);
}

}  // namespace empty_text_sensor
}  // namespace esphome