/*
Website: www.iot-bits.com

Copyright (C) 2016, IoTBits (Author: Pratik Panda), all right reserved.
E-mail: hello@PratikPanda.com
Personal website: www.PratikPanda.com

The code referencing this license is open source software. Redistribution
and use of the code in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    Redistribution of original and modified source code must retain the 
    above copyright notice, this condition and the following disclaimer.

    This code (or modifications) in source or binary forms may NOT be used
    in a commercial application without obtaining permission from the Author.

This software is provided by the copyright holder and contributors "AS IS"
and any warranties related to this software are DISCLAIMED. The copyright 
owner or contributors are NOT LIABLE for any damages caused by use of this 
software.
*/

#include <string.h>
#include "sgtl5000.h"
#include "audiobit.h"
#include "soc/rtc.h"
#include "soc/soc.h"

static i2s_config_t audiobit_i2s_config = {
    .mode = I2S_MODE_MASTER | I2S_MODE_TX,                                  // Only TX
    .sample_rate = AUDIOBIT_SAMPLERATE,                                     // Default: 48kHz
    .bits_per_sample = AUDIOBIT_BITSPERSAMPLE,                              //16-bit per channel
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,                           //2-channels
    .communication_format = I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB,
    .dma_buf_count = 6,
    .dma_buf_len = 512,                                                      //
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1                                //Interrupt level 1
};

static i2s_pin_config_t audiobit_pin_config = {
    .bck_io_num = AUDIOBIT_BCK,
    .ws_io_num = AUDIOBIT_LRCLK,
    .data_out_num = AUDIOBIT_DOUT,
    .data_in_num = AUDIOBIT_DIN                                                       //Not used
};


// ##################################################################

/*
    Set the AudioBit according to sample rate, bit depth, and calculate MCLK
    
    arg_sample_rate: 48000, 8000, etc
    arg_bits_per_sample: 16, 24, etc
*/
esp_err_t audiobit_configure_stream (uint32_t arg_sample_rate, uint32_t arg_bits_per_sample)
{
    // NOTE: To save power, MCLK is always minimum possible, i.e. 256*Fs
    audiobit_i2s_config.sample_rate = arg_sample_rate;
    audiobit_i2s_config.bits_per_sample = arg_bits_per_sample;

    return i2s_driver_install (I2S_NUM, &audiobit_i2s_config, 0, NULL);//used to initialize and register the I2S driver with the hardware.
}
//Surround sound configuration allows the codec to process audio signals in a way that creates a spatial effect, making it sound like the audio is coming from all around you.
/*
    Set surround sound ON/OFF
    0: Surround OFF
    1-8: Surround effect level
*/
esp_err_t audiobit_set_surround_sound (uint8_t surround)
{
    if (surround == 0)      // No surround sound effect
        {
            if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_DAP_SGTL_SURROUND, 0x0000) == 0)//Returns the status of the register write
                return ESP_OK;
            else
                return ESP_FAIL;
        }

    if (surround >8) surround = 8;
    surround -= 1;          // Surround between 0-7 now

    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_DAP_SGTL_SURROUND, 0x3|(surround<<4)) == 0)
            return ESP_OK;
        else
            return ESP_FAIL;
 /*   First 2 bits → Enable surround mode.
Bits 4 to 7 → Control the surround effect level.
Bits 2 and 3 are unused for surround sound configuration.*/
}

/*
    Set microphone bias resistor
    0: hi-Z, 1: 2K, 2: 4K, 3: 8K
*/
//The bias resistor determines the load seen by the microphone, which affects signal quality and power consumption.
esp_err_t audiobit_set_mic_resistor (uint8_t bias)
{
    uint16_t readval;
    
    if (bias > 0x03)
        bias = 0;           // Invalid value, disable bias!

    audiobit_read_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_MIC_CTRL, &readval);
    readval &= 0xFCFF;//The bits for bias resistor setting are located at bits 8 and 9.&= 0xFCFF clears bits 8 and 9 to 00 without affecting other bits.
   
    readval |= (bias << 8);
    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_MIC_CTRL, readval) == 0)
        return ESP_OK;
    else
        return ESP_FAIL;
}

/*
    Set microphone bias voltage (in 250mV steps)
    Range: 1250 - 3000 mV
*/
esp_err_t audiobit_set_mic_voltage (uint16_t voltage)
{
    uint16_t readval;
    
    if (voltage < 1250)
        voltage = 1250;
    else if (voltage > 3000)
        voltage = 3000;

    if (voltage > AUDIOBIT_VDDA - 200)
        return ESP_FAIL;

    voltage -= 1250;
    voltage /= 250;

    audiobit_read_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_MIC_CTRL, &readval);
    readval &= 0xFF8F;
    readval |= (voltage << 4);
    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_MIC_CTRL, readval) == 0)
        return ESP_OK;
    else
        return ESP_FAIL;
}

/*
    Set microphone gain in steps of 10dB each
    Range: 0 to 3
    0: 0dB, 1: +20dB... +40dB
*/
esp_err_t audiobit_set_mic_gain (uint8_t gain)
{
    uint16_t readval;

    if (gain > 3)
        gain = 3;

    audiobit_read_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_MIC_CTRL, &readval);
    readval &= 0xFFFC;
    readval |= gain;
    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_MIC_CTRL, readval) == 0)
        return ESP_OK;
    else
        return ESP_FAIL;
}

/*
    Mute the headphone output
*/
esp_err_t audiobit_mute_headphone (void)
{
    uint16_t readval;

    audiobit_read_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ANA_CTRL, &readval);
    readval |= 0x0010;
    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ANA_CTRL, readval) == 0)
    return ESP_OK;
else
    return ESP_FAIL;
}

/*
    Unmute the headphone output
*/
esp_err_t audiobit_unmute_headphone (void)
{
    uint16_t readval;

    audiobit_read_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ANA_CTRL, &readval);
    readval &= 0xFFEF;
    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ANA_CTRL, readval) == 0)
    return ESP_OK;
else
    return ESP_FAIL;
}

/*
    Set DAC and ADC reference voltage (VAG) in millivolts
    Range: 800 mV to 1575 mV
*/
esp_err_t audiobit_set_ref (uint16_t vag_voltage)
{
    uint16_t readval;

    if (vag_voltage < 800)
        vag_voltage = 800;
    else if (vag_voltage > 1575)
        vag_voltage = 1575;

    vag_voltage /= 25;
    vag_voltage -= 32;
    vag_voltage = (vag_voltage & 0x001F) << 4;

    audiobit_read_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_REF_CTRL, &readval);
    readval &= 0xFE0F;
    readval |= vag_voltage;
    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_REF_CTRL, readval) == 0)
    return ESP_OK;
else
    return ESP_FAIL;
}

/*
    Check if the module is connected and initialized (ready)
    Call this before any other operation, and after setting up
    I2C and I2S interfaces.

    Note: MCLK must be active for this check to return ESP_OK
*/
esp_err_t audiobit_check_module (void)
{
    uint16_t readval;

    audiobit_read_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ID, &readval);
    if ((readval & 0xFF00) == 0xAA00)
        return ESP_OK;
    else
        return ESP_FAIL;
}

/*
    Set digital volume for DAC output.
    Leave this value at 0dB if HP volume can be used to control output

    Volume: 0 dB to -90 dB
    Recommended (default): 0 dB
*/
esp_err_t audiobit_set_digital_volume (int8_t left_vol, int8_t right_vol)
{
    int8_t left, right;

    left = (-2*left_vol) + 0x3C;
    right = (-2*right_vol) + 0x3C;

    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_DAC_VOL, (right << 8)|left) == 0)
        return ESP_OK;
    else
        return ESP_FAIL;
}

/*
    Set volume for headphone output driver.
    Leave this value at 0dB for best audio quality, with DAC volume = 0 dB
    
    For 32 ohm headset in capless mode, -17dB is a good initial value
    Volume: +12 dB to -50 dB
    Recommended (default): 0 dB
*/
esp_err_t audiobit_set_headphone_volume (int8_t left_vol, int8_t right_vol)
{
    int8_t left, right;

    left = ((-2*left_vol) + 0x18) & 0x7F;
    right = ((-2*right_vol) + 0x18) & 0x7F;

    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ANA_HP_CTRL, (right << 8)|left) == 0)
        return ESP_OK;
    else
        return ESP_FAIL;
}

//Drive strength determines how much current is used to drive the signal on the pins — higher strength increases signal integrity but also increases noise and power consumption.
/*→ Drive strength setting for both I2C and I2S pins.
0 → Disable
1 → Low
2 → Medium
3 → High */
/* Drive Strength Levels:
0b00 = Disable
0b01 = Low Drive Strength
0b10 = Medium Drive Strength
0b11 = High Drive Strength*/

esp_err_t audiobit_pin_drive_strength (uint8_t i2c_strength, uint8_t i2s_strength)
{
    uint16_t strength;

    i2c_strength &= 0x03;
    i2s_strength &= 0x03;
    strength = i2c_strength|(i2c_strength << 2);//strength = 0b11 | 0b1100 = 0b1111
    strength |= (i2s_strength << 4)|(i2s_strength << 6)|(i2s_strength << 8);// When you set I2S strength, you're only changing bits 4–9 — that's why medium (0b10) works even when I2C is high (0b11)

    if (audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ANA_HP_CTRL, strength) == 0)
        return ESP_OK;
    else
        return ESP_FAIL;
}

/* Basic initialization, DAC enable:

• Start condition
• Device address with the R/W bit cleared to indicate write 
• Send two bytes for the 16 bit register address (most significant byte first)
• Stop Condition followed by start condition (or a single restart condition)
• Device address with the R/W bit set to indicate read 
• Read two bytes from the addressed register (most significant byte first)
• Stop condition
*/
// It configures the clock, I2S interface, power settings, gain, and drive strength.
esp_err_t audiobit_playback_init (void)
{
    uint8_t retval=0;
    uint16_t readval;

    // Read chip ID//If the ID is read correctly, it confirms the codec is responding over I2C.
    retval = audiobit_read_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ID, &readval);
    printf("AudioBit chip ID: %d, return code: %d\n", readval, retval);

    // Digital power control
    /*Bit 0 = Power up the ADC (record path).
Bit 5 = Power up the DAC (playback path).
This powers up the DAC and enables the I2S interface.*/
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_DIG_POWER, 0x0021);
    printf("Power up I2S and DAC, err_code: %d\n", retval);

    // 48kHz sample rate, with CLKM = 256*Fs = 12.288000 MHz//CLKM is a high-frequency master clock for the codec
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_CLK_CTRL, 0x0008);
    printf("Power up I2S and DAC, err_code: %d\n", retval);

    // I2S mode = , LRALIGN = 0, LRPOL = 0
    //Value written: 0x0130 → 0000 0001 0011 0000

/*
Bits[3:0] = 0000 → I2S data format → Standard left-justified
Bit[4] = 1 → Bit depth → 16-bit samples
Bit[5] = 1 → Mode → Slave mode (ESP32 as master)
Bit[8] = 1 → Clock rate → SCLK rate = 32 × Fs = 1.536 MHz (for Fs = 48 kHz) //SCLK is the clock rate for transferring audio data over I2S.
*/
    // 32*Fs is SCLK rate, 16 bits/sample, I2S is slave, no PLL used
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_I2S_CTRL, 0x0130);
    printf("I2S configured, err_code: %d\n", retval);

    // I2S in -> DAC output, rest left at default//register to route the I2S input to the DAC output:
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_SSS_CTRL, 0x0010);//Bit 4 = 1 → Routes I2S input to DAC.
    printf("Attach I2S in to DAC, err_code: %d\n", retval);

    // Unmute DAC, no volume ramp enabled
    //DAC converts digital audio to analog for output, and it's initially muted to prevent noise; unmuting it without volume ramping ensures immediate audio output at the set volume without gradual increase.
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ADCDAC_CTRL, 0x0000);//0x0002 → Bit[1] = 1 → Mutes the DAC.
    printf("Unmute DAC, err_code: %d\n", retval);

    // DAC volume is 0dB for both channels
    /* 
    Volume in dB=(register value−0x3F)×0.5
    0x3C = 60 (in decimal)
    (60−63)×0.5=(−3)×0.5=−1.5dB
    */
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_DAC_VOL, 0x3C3C);
    printf("DAC volume -0.5dB, err_code: %d\n", retval);

    // Moderate drive strength (4mA) for all pads
    /*//all these defined in the hard ware data sheet.
    00 → 2 mA (Weak)
01 → 2.5 mA (Slightly stronger)
10 → 4 mA (Moderate)
11 → 8 mA (Strong)
*/
    /*
    10 → 4 mA → Moderate drive strength
The bits are grouped as follows (2 bits per pad):
00 00 10 10 10 10 10 10
10 → 4 mA for each pad
So this sets moderate drive strength for all the pads (SCLK, LRCLK, DOUT, DIN, CTRL_OUT, etc.).
*/
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_PAD_STRENGTH, 0x02AA);
    printf("Moderate drive strength for pads, err_code: %d\n", retval);

    // Headphone output volume is -17dB each//Volume in dB=(58−63)×0.5=−5×0.5=−2.5dB
    //Bits [7:0] control the left headphone volume, and bits [15:8] control the right headphone volume.
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ANA_HP_CTRL, 0x3A3A);//0x3A3A sets both left and right headphone volumes to 0x3A = -17 dB each.
    printf("HP out volume is -17dB, err_code: %d\n", retval);
    
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Unmute HP, ZCD disabled, rest mute
    /*Bit 0 = 1 → Unmute headphone output.
    //Bit 8 = 1 → Ensures stable operation or reserved setting (but not enabling ZCD for the headphone).
    //Disabling ZCD for headphones ensures quick and direct audio output without waiting for a zero-crossing event! 

    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ANA_CTRL, 0x0101);
    printf("Unmute HP, err_code: %d\n", retval);

    // VAG_VAL = 0.8V + 100mV = 0.9V
    /*
    Bits [6:4] → Control the VAG level setting 
000 → 0.8V
001 → 0.825V
010 → 0.85V
011 → 0.875V
100 → 0.9V
101 → 0.925V
110 → 0.95V
111 → 0.975V
 0x0040 sets bits [6:4] to 100 → This corresponds to 0.9V for VAG.
 // to prevent distortion and improve audio quality.
    */
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_REF_CTRL, 0x0040);
    printf("Check VAG = 0.9V!, err_code: %d\n", retval);

    // Capless HP and DAC on
    // Stereo DAC with external VDDD source
    //0x40FC ensures that all analog sections (including DAC, ADC, headphone, line out, mic bias, and PLL) are fully powered up together for proper operation.
    retval = audiobit_write_reg (AUDIOBIT_I2C_NUM, SGTL5000_CHIP_ANA_POWER, 0x40FC);
    printf("Power up all analog sections, err_code: %d\n", retval);

    if (retval == 0)
        return ESP_OK;
    else
        return ESP_FAIL;
}

/* Write operation:

• Start condition 
• Device address with the R/W bit cleared to indicate write 
• Send two bytes for the 16 bit register address (most significant byte first)
• Send two bytes for the 16 bits of data to be written to the register (most significant byte first)
• Stop condition
*/

esp_err_t audiobit_write_reg (i2c_port_t i2c_num, uint16_t reg_addr, uint16_t reg_val)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    uint8_t dwr[4];

    // Start condition
    i2c_master_start(cmd);
    // Address + Write bit
    i2c_master_write_byte(cmd, (AUDIOBIT_I2C_ADDR<<1)|WRITE_BIT, ACK_CHECK_EN);//I2C hardware expects an 8-bit address (7 bits for the slave address + 1 bit for R/W).
    // MSB for reg address
    //i2c_master_write_byte(cmd, (reg_addr>>8)&0xFF, ACK_CHECK_EN);
    dwr[0] = (reg_addr>>8)&0xFF;
    // LSB for reg address
    //i2c_master_write_byte(cmd, (reg_addr&0xFF), ACK_CHECK_EN);
    dwr[1] = (reg_addr&0xFF);
    // MSB for reg data
    //i2c_master_write_byte(cmd, (reg_val>>8)&0xFF, ACK_CHECK_EN);
    dwr[2] = (reg_val>>8)&0xFF;
    // LSB for reg data
    //i2c_master_write_byte(cmd, (reg_val&0xFF), ACK_CHECK_EN);
    dwr[3] = (reg_val&0xFF);
    i2c_master_write(cmd, dwr, 4, ACK_CHECK_EN);
    i2c_master_stop(cmd);

    // Execute and return status
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);//i2c_master_cmd_begin() expects the timeout value in ticks, not milliseconds.
To convert milliseconds to ticks:1000 / portTICK_RATE_MS
    //portTICK_RATE_MS → Converts ms to RTOS ticks
    i2c_cmd_link_delete(cmd);
    return ret;
}


/* Read operation:

• Start condition
• Device address with the R/W bit cleared to indicate write 
• Send two bytes for the 16 bit register address (most significant byte first)
• Stop Condition followed by start condition (or a single restart condition)
• Device address with the R/W bit set to indicate read 
• Read two bytes from the addressed register (most significant byte first)
• Stop condition
*/
esp_err_t audiobit_read_reg (i2c_port_t i2c_num, uint16_t reg_addr, uint16_t *reg_val)
{
    uint8_t *byte_val = reg_val;		// This will cause warning, please ignore
    esp_err_t ret;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    // Start condition
    i2c_master_start(cmd);
    // Address + Write bit
    i2c_master_write_byte(cmd, (AUDIOBIT_I2C_ADDR<<1)|WRITE_BIT, ACK_CHECK_EN);
    // MSB for reg address
    i2c_master_write_byte(cmd, (reg_addr>>8)&0xFF, ACK_CHECK_EN);
    // LSB for reg address
    i2c_master_write_byte(cmd, (reg_addr&0xFF), ACK_CHECK_EN);

    // Restart (stop + start)
    i2c_master_start(cmd);

    // Address + read
    i2c_master_write_byte(cmd, (AUDIOBIT_I2C_ADDR<<1)|READ_BIT, ACK_CHECK_EN);
    
    // MSB for reg data
    i2c_master_read(cmd, byte_val + 1, 1, ACK_VAL);
    // LSB for reg data
    i2c_master_read_byte(cmd, byte_val, NACK_VAL);
    
    i2c_master_stop(cmd);
    // Execute and return status, should return 0
    ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_RATE_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

/**
 * @brief i2c master initialization
 */
void audiobit_i2c_init()
{
    int i2c_master_port = AUDIOBIT_I2C_NUM;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = AUDIOBIT_I2C_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = AUDIOBIT_I2C_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = AUDIOBIT_I2C_FREQ_HZ;
    i2c_param_config(i2c_master_port, &conf);//above all configure here.
    i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);//necessary because it allocates memory and resources for I2C communication and attaches the driver to the specified I2C port.
    /*
        In master mode, the ESP32 doesn't need RX or TX buffers because:
It sends commands directly to the I2C bus.
The data received comes directly into memory without needing extra buffering.
        */
}

void audiobit_i2s_init ()
{
    i2s_driver_install(I2S_NUM, &audiobit_i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM, &audiobit_pin_config);

    // Enable MCLK output//The codec uses MCLK to generate internal clocks for audio processing, such as the sampling rate and bit clock.
    WRITE_PERI_REG(PIN_CTRL, READ_PERI_REG(PIN_CTRL)&0xFFFFFFF0);//It reads the PIN_CTRL register value, clears the lowest 4 bits to reset MCLK settings, and writes the modified value back to enable MCLK output.
    PIN_FUNC_SELECT (PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0_CLK_OUT1);
    /*
        GPIO pins on the ESP32 can have multiple functions (like input, output, clock, etc.).
The PIN_FUNC_SELECT() function sets the function of a specific GPIO pin.
PERIPHS_IO_MUX_GPIO0_U refers to the GPIO0 multiplexer register, which controls what GPIO0 will do.
FUNC_GPIO0_CLK_OUT1 tells the multiplexer to configure GPIO0 as a clock output (MCLK).
So, this line sets GPIO0 to act as the master clock (MCLK) output.
*/
}
