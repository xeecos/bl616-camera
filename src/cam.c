#include "usb_uart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bflb_gpio.h"
#include "bflb_cam.h"
#include "bflb_clock.h"
#include "bflb_uart.h"
#include "bflb_mjpeg.h"
#include "bflb_pwm_v2.h"
#include "bf3003.h"
#include "config.h"
#include "uart.h"
#include "i2c.h"
#include "jpeg_head.h"
#include "cam.h"
#include "led.h"
#include "firmata.h"

static struct bflb_device_s *gpio;
static struct bflb_device_s *cam0;
static struct bflb_device_s *mjpeg;
volatile int status_cam = 0;
uint8_t *datajpeg_buf;
uint32_t datajpeg_len;
#define MJPEG_BUFFER_LENGTH (60 * 1024)
#define DVP_BUFFER_LENGTH (ROW_NUM * 2 * 640)

uint8_t jpg_head_buf[800] = { 0 };
uint32_t jpg_head_len;

uint8_t MJPEG_QUALITY = 50;
static TaskHandle_t cam_process_task_hd;
volatile uint32_t jpeg_len = 0;

static __attribute__((aligned(32))) ATTR_NOINIT_PSRAM_SECTION uint8_t dvp_buffer[DVP_BUFFER_LENGTH];
static __attribute__((aligned(32))) ATTR_NOINIT_PSRAM_SECTION uint8_t mjpeg_buffer[MJPEG_BUFFER_LENGTH];
void cam_isr(int irq, void *arg)
{
    bflb_cam_int_clear(cam0, CAM_INTCLR_NORMAL);
            // light_toggle();bflb_cam_pop_one_frame(cam0);
}
void mjpeg_isr(int irq, void *arg)
{
    uint8_t *pic;
    uint32_t intstatus = bflb_mjpeg_get_intstatus(mjpeg);
    // bflb_mjpeg_stop(mjpeg);
    // bflb_cam_stop(cam0);
    int frame = 0;
    if (intstatus & MJPEG_INTSTS_ONE_FRAME) {
        bflb_mjpeg_int_clear(mjpeg, MJPEG_INTCLR_ONE_FRAME); 
        bflb_mjpeg_int_clear(mjpeg, 1 << 10);
        
        if(cam_status()==0)
        {
            int len = bflb_mjpeg_get_frame_info(mjpeg, &pic);
            memcpy(datajpeg_buf, pic, len);
            datajpeg_len = len;
            cam_set_status(1);
            light_toggle();
        }
        frame = 1;
    }
    else if (intstatus & (1 << 6)) {
        bflb_mjpeg_int_clear(mjpeg, 1 << 10);
        frame = 1;
    }
    if(frame==1)
    {
        bflb_mjpeg_pop_one_frame(mjpeg);
        // BaseType_t pxHigherPriorityTaskWoken = pdFALSE;
        // vTaskNotifyGiveFromISR(cam_process_task_hd, &pxHigherPriorityTaskWoken);
        // if (pxHigherPriorityTaskWoken) {
        //     portYIELD_FROM_ISR(pxHigherPriorityTaskWoken);
        // }
    }
}
uint8_t cam_sensor_read(uint8_t address)
{
    struct bflb_i2c_msg_s msgs[2];
    uint8_t buffer[2];
    uint8_t readOut[1];
    
    msgs[0].addr = BF3003_ADDR;
    msgs[0].flags = I2C_M_NOSTOP;
    msgs[0].buffer = NULL;
    msgs[0].length = 1;

    msgs[1].addr = BF3003_ADDR;
    msgs[1].flags = I2C_M_READ;
    msgs[1].buffer = NULL;
    msgs[1].length = 1;
    
    buffer[0] = address & 0xff;
    msgs[0].buffer = buffer;
    msgs[1].buffer = readOut;
    i2c_transfer(msgs, 2);
    return readOut[0];
}

void cam_sensor_write(uint8_t address, uint8_t paramete)
{
    struct bflb_i2c_msg_s msgs[2];
    uint8_t buffer[2];
    
    msgs[0].addr = BF3003_ADDR;
    msgs[0].flags = I2C_M_NOSTOP;
    msgs[0].buffer = NULL;
    msgs[0].length = 1;

    msgs[1].addr = BF3003_ADDR;
    msgs[1].flags = 0;
    msgs[1].buffer = NULL;
    msgs[1].length = 1;
    
    uint8_t writeIn[1] = { paramete };
    buffer[0] = address & 0xff;
    msgs[0].buffer = buffer;
    msgs[1].buffer = writeIn;

    i2c_transfer(msgs, 2);
}
void cam_probe()
{
    int i = 0;
    while(1)
    {
		uint8_t ver = cam_sensor_read(BF3003_PID);
        if(ver == 0x30)
        {
            break;
        }
		printf("ver:%x\n",ver);
        light_toggle();
        bflb_mtimer_delay_ms(200);
    }
    i = 0;
    while (1)
    {
        cam_sensor_write(bf3003_init_list[i].address, bf3003_init_list[i].paramete);
        light_toggle();
        bflb_mtimer_delay_ms(20);
        i++;
        if(bf3003_init_list[i].address == 0 && bf3003_init_list[i].paramete == 0)
        {
            break;
        }
    }
}
int cam_status()
{
    return status_cam;
}
void cam_set_status(int status)
{
    status_cam = status;
}

uint32_t cam_data_length()
{
    return datajpeg_len;
}
uint8_t *cam_data()
{
    return datajpeg_buf;
}
void cam_task(void*param)
{
    while (1)
    {
        vTaskDelay(10);
    }
}

void cam_init()
{
    
    datajpeg_buf = (uint8_t *)malloc(MJPEG_BUFFER_LENGTH);
    datajpeg_len = 0;
    // board_uartx_gpio_init();
    // uart0 = bflb_device_get_by_name("uart0");
    // struct bflb_uart_config_s cfg;
    // cfg.baudrate = 2000000;
    // cfg.data_bits = UART_DATA_BITS_8;
    // cfg.stop_bits = UART_STOP_BITS_1;
    // cfg.parity = UART_PARITY_NONE;
    // cfg.flow_ctrl = 0;
    // cfg.tx_fifo_threshold = 7;
    // cfg.rx_fifo_threshold = 7;
    // cfg.bit_order = UART_LSB_FIRST;
    // bflb_uart_init(uart0, &cfg);

    gpio = bflb_device_get_by_name("gpio");
    uart_init(gpio);
    firmata_init();
    while(1)
    {
        firmata_start();
        firmata_write(0);
        firmata_write(0);
        firmata_end();
        uart_write(firmata_get(),firmata_length());
        bflb_mtimer_delay_ms(200);
        firmata_start();
        firmata_write(0);
        firmata_write(1);
        firmata_end();
        uart_write(firmata_get(),firmata_length());
        bflb_mtimer_delay_ms(200);
        light_toggle();
    }
    i2c_init(gpio, PIN_SCL0, PIN_SDA0);
    cam_probe();

    bflb_gpio_init(gpio, PIN_CAM_XCLK, GPIO_FUNC_CLKOUT | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_D0, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_D1, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_D2, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_D3, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_D4, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_D5, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_D6, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_D7, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_HREF, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_VSYNC, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);
    bflb_gpio_init(gpio, PIN_CAM_PIXCLK, GPIO_FUNC_CAM | GPIO_ALTERNATE | GPIO_PULLUP | GPIO_SMT_EN | GPIO_DRV_1);

    // bflb_gpio_init(gpio, PIN_CAM_XCLK, GPIO_FUNC_PWM0 | GPIO_ALTERNATE | GPIO_SMT_EN | GPIO_DRV_1);
    
    // /* period = .XCLK / .clk_div / .period = 40MHz / 2 / 4 = 5000KHz */
    // struct bflb_pwm_v2_config_s cfg_pwm = {
    //     .clk_source = BFLB_SYSTEM_XCLK,
    //     .clk_div = 2,
    //     .period = 4,
    // };

    // struct bflb_device_s *pwm = bflb_device_get_by_name("pwm_v2_0");
    // bflb_pwm_v2_init(pwm, &cfg_pwm);
    // bflb_pwm_v2_channel_set_threshold(pwm, PWM_CH3, 1, 3); 
    // bflb_pwm_v2_channel_positive_start(pwm, PWM_CH3);
    // bflb_pwm_v2_start(pwm);
    
    cam0 = bflb_device_get_by_name("cam0");
    struct bflb_cam_config_s cam_config;
    struct image_sensor_config_s *sensor_config = &bf3003_config;


    bflb_cam_int_mask(cam0, CAM_INTMASK_NORMAL, false);
    bflb_irq_attach(cam0->irq_num, cam_isr, NULL);
    bflb_irq_enable(cam0->irq_num);

    memcpy(&cam_config, sensor_config, IMAGE_SENSOR_INFO_COPY_SIZE);
    cam_config.with_mjpeg = true;
    cam_config.input_source = CAM_INPUT_SOURCE_DVP;
    cam_config.output_format = CAM_OUTPUT_FORMAT_AUTO;
    cam_config.output_bufaddr = (uint32_t) dvp_buffer;
    cam_config.output_bufsize = DVP_BUFFER_LENGTH;

    bflb_cam_init(cam0, &cam_config);
    // uint32_t regval = getreg32(cam0->reg_base);
    // regval |= (0b000 << 0x8);
    // putreg32(regval, cam0->reg_base);
    bflb_cam_feature_control(cam0, CAM_CMD_COUNT_TRIGGER_NORMAL_INT, 1);
    // bflb_cam_feature_control(cam0, CAM_CMD_WRAP_MODE, 1);
    
    mjpeg = bflb_device_get_by_name("mjpeg");

    struct bflb_mjpeg_config_s config;
    config.format = MJPEG_FORMAT_YUV422_YUYV;
    config.quality = MJPEG_QUALITY;
    config.rows = ROW_NUM;
    config.resolution_x = cam_config.resolution_x;
    config.resolution_y = cam_config.resolution_y;
    config.input_bufaddr0 = (uint32_t) dvp_buffer;
    config.input_bufaddr1 = 0;
    config.output_bufaddr = (uint32_t)mjpeg_buffer;
    config.output_bufsize = MJPEG_BUFFER_LENGTH;
    config.input_yy_table = NULL; /* use default table */
    config.input_uv_table = NULL; /* use default table */

    bflb_mjpeg_init(mjpeg, &config);

    jpg_head_len = JpegHeadCreate(YUV_MODE_422, MJPEG_QUALITY, cam_config.resolution_x, cam_config.resolution_y, jpg_head_buf);
    bflb_mjpeg_fill_jpeg_header_tail(mjpeg, jpg_head_buf, jpg_head_len);

    bflb_mjpeg_tcint_mask(mjpeg, false);
    bflb_irq_attach(mjpeg->irq_num, mjpeg_isr, NULL);
    bflb_irq_enable(mjpeg->irq_num);

    
    bflb_cam_start(cam0);
    bflb_mjpeg_start(mjpeg);
    xTaskCreate(cam_task, (char *)"cam_task", 1024, NULL, 14, &cam_process_task_hd);
}