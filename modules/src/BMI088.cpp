#include "BMI088.hpp"
#include "tx_api.h"
#include "bsp_pwm.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include "spi.h"

namespace BMI088
{
    /**
     * @brief BMI088 acc gyro 标定
     * @note 标定后的陀螺仪零偏存储在 Gyro_offset 中。
     * @attention 不管工作模式是blocking还是IT,标定时都是blocking模式,所以不用担心中断关闭后无法标定(RobotInit关闭了全局中断)
     * @attention 标定精度和等待时间有关。
     * @todo 将标定次数(等待时间)变为参数供设定
     * @section 整体流程为1.累加加速度数据计算gNrom()
     *                   2.累加陀螺仪数据计算零飘
     *                   3. 如果标定过程运动幅度过大,重新标定
     *                   4.保存标定参数
     */
     void cBMI088::Calibrate()
    {
        const int calib_samples = 4000; // 采样次数
        float gyro_sum[3] = {0.0f, 0.0f, 0.0f};
        gyro_data_t temp_gyro;

        // 1. 清除旧的 Offset，防止叠加
        Gyro_offset[0] = 0.0f;
        Gyro_offset[1] = 0.0f;
        Gyro_offset[2] = 0.0f;

        // 2. 循环采样
        for (int i = 0; i < calib_samples; i++)
        {
            ReadGyroData(&temp_gyro); 
            if (self_test.GYRO_DATA_ERR == true){
                self_test.CALIBRATE_ERR = true;
            // 恢复默认值或保留上次值
                Gyro_offset[0] = BMI088_GYRO_PRE_CALI_OFFSET_X; 
                Gyro_offset[1] = BMI088_GYRO_PRE_CALI_OFFSET_Y;
                Gyro_offset[2] = BMI088_GYRO_PRE_CALI_OFFSET_Z;
                return;
            }

                // 这里读取的是原始值（因为Offset已清零）
            gyro_sum[0] += temp_gyro.x;
            gyro_sum[1] += temp_gyro.y;
            gyro_sum[2] += temp_gyro.z;
            
            tx_thread_sleep(1); // 等待一个 ThreadX tick；实际耗时由 tick 频率决定。
        }

        // 3. 计算平均值作为零偏
        Gyro_offset[0] = gyro_sum[0] / calib_samples;
        Gyro_offset[1] = gyro_sum[1] / calib_samples;
        Gyro_offset[2] = gyro_sum[2] / calib_samples;
        
        // 如果零偏过大（例如超过 0.1 rad/s），可能是在运动中标定的，应报错或丢弃
        if (fabs(Gyro_offset[0]) > 0.1f || fabs(Gyro_offset[1]) > 0.1f || fabs(Gyro_offset[2]) > 0.1f)
        {
            self_test.CALIBRATE_ERR = true;
            // 恢复默认值或保留上次值
            Gyro_offset[0] = BMI088_GYRO_PRE_CALI_OFFSET_X; 
            Gyro_offset[1] = BMI088_GYRO_PRE_CALI_OFFSET_Y;
            Gyro_offset[2] = BMI088_GYRO_PRE_CALI_OFFSET_Z;
        }
        else
        {
            self_test.CALIBRATE_ERR = false;
        }
    }

    void cBMI088::TemperatureControl(float target_temp)
    {
        // TODO: 温度读取完成后，用 TempPid 计算加热占空比并限制到 [0, 1]。
        // 未完成前保持加热关闭。
        
            // 示例软件限制，不是 BMI088 的额定极限。
            // 最终数值应按板级加热设计确定。
            constexpr float maxTarget = 55.0f;
            constexpr float cutoffTemp = 60.0f;

            float currentTemp = InvalidValue();
            ReadAccTemperature(&currentTemp);
            acc_data.temperature = currentTemp;

            bool invalid =
                self_test.INIT_ERR ||
                self_test.ACC_CHIP_ID_ERR ||
                !std::isfinite(target_temp) ||
                !std::isfinite(currentTemp) ||
                target_temp < 0.0f ||
                target_temp > maxTarget ||
                currentTemp < -40.0f ||
                currentTemp >= cutoffTemp;

            if (invalid)
            {
                TempPid.Clear();

                PWM_SetDutyRatio(
                    &HEATING_RESISTANCE_TIM, 0.0f,
                    HEATING_RESISTANCE_CHANNEL);

                self_test.TEMP_CTRL_ERR = true;
                return;
            }

            float duty = 0.0f;

            if (currentTemp < target_temp)
            {
                TempPid.ref = target_temp;
                TempPid.fdb = currentTemp;
                TempPid.UpdateResult();

                duty = TempPid.result;

                if (!std::isfinite(duty))
                {
                    TempPid.Clear();

                    PWM_SetDutyRatio(
                        &HEATING_RESISTANCE_TIM, 0.0f,
                        HEATING_RESISTANCE_CHANNEL);

                    self_test.TEMP_CTRL_ERR = true;
                    return;
                }

                if (duty < 0.0f) duty = 0.0f;
                if (duty > 1.0f) duty = 1.0f;
            }
            else
            {
                // 加热器只能加热；达到目标后关闭并清积分。
                TempPid.Clear();
            }
        PWM_SetDutyRatio(&HEATING_RESISTANCE_TIM, duty, HEATING_RESISTANCE_CHANNEL);

        self_test.TEMP_CTRL_ERR = false;
    }

    void cBMI088::VerifyAccChipID()
    {
        uint8_t id = 0;

        bool ok = ReadReg(
            BMI088_CS_ACC, ACC_CHIP_ID_ADDR, &id, 1);

        self_test.ACC_CHIP_ID_ERR =
            !ok || id != ACC_CHIP_ID_VAL;

        if (self_test.ACC_CHIP_ID_ERR)
            self_test.INIT_ERR = true;
    }

        void cBMI088::VerifyGyroChipID()
    {
        uint8_t id = 0;

        bool ok = ReadReg(BMI088_CS_GYRO, GYRO_CHIP_ID_ADDR, &id, 1);
        //< 如果chip id不等于预设值,则陀螺仪ID错误,初始化错误
        self_test.GYRO_CHIP_ID_ERR =!ok || id != GYRO_CHIP_ID_VAL;

        if (self_test.GYRO_CHIP_ID_ERR)
            self_test.INIT_ERR = true;  
    }

    void cBMI088::VerifyAccData()
    {
        // TODO: 检查加速度数据是否有效，并更新 ACC_DATA_ERR。
        ReadAccData(&acc_data);

        if (self_test.ACC_DATA_ERR)
            return;

        // 当前为 ±6g，给换算误差留一点余量。
        constexpr float limit = 60.0f;  // m/s²

        self_test.ACC_DATA_ERR =
            !std::isfinite(acc_data.x) ||
            !std::isfinite(acc_data.y) ||
            !std::isfinite(acc_data.z) ||
            std::fabs(acc_data.x) > limit ||
            std::fabs(acc_data.y) > limit ||
            std::fabs(acc_data.z) > limit;
    }

    void cBMI088::VerifyGyroData()
    {
        ReadGyroData(&gyro_data);

        if (self_test.GYRO_DATA_ERR)
            return;

        // ±2000 °/s 约为 ±34.91 rad/s。
        // 检查减去零偏之前的值，避免零偏影响量程判断。
        constexpr float limit = 35.0f;

        self_test.GYRO_DATA_ERR =
            !std::isfinite(gyro_data.x) ||
            !std::isfinite(gyro_data.y) ||
            !std::isfinite(gyro_data.z) ||
            std::fabs(gyro_data.x + Gyro_offset[0]) > limit ||
            std::fabs(gyro_data.y + Gyro_offset[1]) > limit ||
            std::fabs(gyro_data.z + Gyro_offset[2]) > limit;
        // TODO: 检查角速度数据是否有效，并更新 GYRO_DATA_ERR。
    }

    bool cBMI088::WriteReg(enum BMI088_SENSOR cs, uint8_t addr,
         uint8_t *data, uint8_t len)
    {
        // TODO: 根据 cs 拉低对应 GPIO 片选；按 SPI 写协议发送地址和数据，
        // 最后释放片选，并处理通信失败与必要的延时。

        if (data == nullptr || len != 1)
            return false;

        if (cs != BMI088_CS_ACC && cs != BMI088_CS_GYRO)
            return false;

        GPIO_TypeDef* port = (cs == BMI088_CS_ACC) ? Accel_GPIO_Port : Gyro_GPIO_Port;

        uint16_t pin = (cs == BMI088_CS_ACC) ? Accel_Pin : Gyro_Pin;

        uint8_t tx[2] = {
            static_cast<uint8_t>(addr & BMI088_SPI_WRITE_CODE),
            data[0]
        };

        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);

        HAL_StatusTypeDef status =
            HAL_SPI_Transmit(&hspi1, tx, sizeof(tx), 100);

        // 无论成功失败，都结束本次片选。
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);

        // 教学版：统一留出写入间隔。
        // 不代替软复位后的等待，也不要在中断里调用。
        HAL_Delay(2);

        return status == HAL_OK;
    }

    bool cBMI088::ReadReg(enum BMI088_SENSOR cs, uint8_t addr,
        uint8_t* data, uint8_t len)
    {
        if (data == nullptr || len == 0 || len > 6)
            return false;

        if (cs != BMI088_CS_ACC && cs != BMI088_CS_GYRO)
            return false;

        GPIO_TypeDef* port =
            (cs == BMI088_CS_ACC) ? Accel_GPIO_Port : Gyro_GPIO_Port;

        uint16_t pin =
            (cs == BMI088_CS_ACC) ? Accel_Pin : Gyro_Pin;

        uint8_t tx[8] = {};
        uint8_t rx[8] = {};

        tx[0] = addr | BMI088_SPI_READ_CODE;

        // 地址传输时收到的字节无效。
        // 加速度计还额外返回一个 dummy。
        uint8_t offset = (cs == BMI088_CS_ACC) ? 2 : 1;

        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);

        HAL_StatusTypeDef status = HAL_SPI_TransmitReceive(
            &hspi1, tx, rx, offset + len, 100);

        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);

        if (status != HAL_OK)
            return false;

        for (uint8_t i = 0; i < len; ++i)
            data[i] = rx[offset + i];

        return true;
            // TODO: 根据 cs 拉低对应 GPIO 片选；按 SPI 读协议发送地址，
            // 丢弃加速度计返回的首个无效字节，读取 len 字节并释放片选。
            // 未实现前清零缓冲区，避免芯片 ID 校验读取未初始化数据。
    }

    void cBMI088::Config()
    {
        self_test.INIT_ERR = true;
        tx_thread_sleep(10); //< 等待系统稳定

        /*-------------------------------------加速度计初始化-------------------------------------*/
        
        //< 先软重启，清空所有寄存器
        uint8_t pTxData;
        pTxData = ACC_SOFTRESET_VAL;
        WriteReg(BMI088_CS_ACC, ACC_SOFTRESET_ADDR, &pTxData, 1);
        tx_thread_sleep(100); //< 延时100ms,重启需要时间

        //< 打开加速度计电源
        pTxData = ACC_PWR_CTRL_ON;
        WriteReg(BMI088_CS_ACC, ACC_PWR_CTRL_ADDR, &pTxData, 1);
        tx_thread_sleep(150);

        //< 加速度计变成正常模式
        pTxData = ACC_PWR_CONF_ACT;
        WriteReg(BMI088_CS_ACC, ACC_PWR_CONF_ADDR, &pTxData, 1);
        tx_thread_sleep(10); //

        //< 测量范围
        pTxData = ACC_RANGE_6G;
        WriteReg(BMI088_CS_ACC, ACC_RANGE_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        pTxData = 0xAB;
        WriteReg(BMI088_CS_ACC, ACC_CONF_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        pTxData = 0x08;
        WriteReg(BMI088_CS_ACC, INT1_IO_CTRL_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        pTxData = 0x04;
        WriteReg(BMI088_CS_ACC, INT_MAP_DATA_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        VerifyAccChipID();

        /*-------------------------------------陀螺仪初始化-------------------------------------*/
        //< 先软重启，清空所有寄存器
        pTxData = GYRO_SOFTRESET_VAL;
        WriteReg(BMI088_CS_GYRO, GYRO_SOFTRESET_ADDR, &pTxData, 1);
        tx_thread_sleep(100); //< 延时100ms,重启需要时间

        pTxData = GYRO_RANGE_2000_DEG_S;
        WriteReg(BMI088_CS_GYRO, GYRO_RANGE_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        pTxData = GYRO_ODR_2000Hz_BANDWIDTH_230Hz | GYRO_LPM1_SUS;
        WriteReg(BMI088_CS_GYRO, GYRO_BANDWIDTH_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        pTxData = GYRO_LPM1_NOR;
        WriteReg(BMI088_CS_GYRO, GYRO_LPM1_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        pTxData = GYRO_DRDY_ON;
        WriteReg(BMI088_CS_GYRO, GYRO_INT_CTRL_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        pTxData = 0x00;
        WriteReg(BMI088_CS_GYRO, GYRO_INT3_INT4_IO_CONF_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        pTxData = 0x01;
        WriteReg(BMI088_CS_GYRO, GYRO_INT3_INT4_IO_MAP_ADDR, &pTxData, 1);
        tx_thread_sleep(5); //< 延时5ms

        VerifyGyroChipID();


        if (self_test.ACC_CHIP_ID_ERR ||
        self_test.GYRO_CHIP_ID_ERR)
        return;

        // 只有前面的全部初始化步骤都成功，才执行这里。
        self_test.INIT_ERR = false;
    }


    void cBMI088::ReadAccData(acc_data_t *data)
    {
            if (data == nullptr)
        {
            self_test.ACC_DATA_ERR = true;
            return;
        }

        const float invalid = InvalidValue();
        *data = {invalid, invalid, invalid, invalid};

        uint8_t buffer[ACC_XYZ_LEN] = {};

        if (!ReadReg(BMI088_CS_ACC, ACC_X_LSB_ADDR,
                    buffer, ACC_XYZ_LEN))
        {
            self_test.ACC_DATA_ERR = true;
            return;
        }

        data->x = DecodeSigned16(buffer[0], buffer[1])
                * IMU_ACCEL_6G_SEN;

        data->y = DecodeSigned16(buffer[2], buffer[3])
                * IMU_ACCEL_6G_SEN;

        data->z = DecodeSigned16(buffer[4], buffer[5])
                * IMU_ACCEL_6G_SEN;

        ReadAccTemperature(&data->temperature);

        // 本例把 ACC_DATA_ERR 定义为三轴加速度是否有效。
        // 温度异常由温控函数单独处理。
        self_test.ACC_DATA_ERR = false;

        // TODO: 按 BMI088 加速度计 SPI 协议丢弃首个无效字节，拼接三轴有符号原始值，
        // 再按配置的量程换算为 m/s²，写入 data。
    }

    void cBMI088::ReadGyroData(gyro_data_t *data)
    {
            if (data == nullptr)
        {
            self_test.GYRO_DATA_ERR = true;
            return;
        }

        const float invalid = InvalidValue();
        *data = {invalid, invalid, invalid};

        uint8_t buffer[GYRO_XYZ_LEN] = {};

        if (!ReadReg(BMI088_CS_GYRO, GYRO_RATE_X_LSB_ADDR,
                    buffer, GYRO_XYZ_LEN))
        {
            self_test.GYRO_DATA_ERR = true;
            return;
        }

        data->x = DecodeSigned16(buffer[0], buffer[1])
                * IMU_GYRO_2000_SEN - Gyro_offset[0];

        data->y = DecodeSigned16(buffer[2], buffer[3])
                * IMU_GYRO_2000_SEN - Gyro_offset[1];

        data->z = DecodeSigned16(buffer[4], buffer[5])
                * IMU_GYRO_2000_SEN - Gyro_offset[2];

        self_test.GYRO_DATA_ERR =
            !std::isfinite(data->x) ||
            !std::isfinite(data->y) ||
            !std::isfinite(data->z);
        // TODO: 读取并拼接陀螺仪三轴原始值，按配置量程换算为 rad/s，
        // 减去 Gyro_offset 后写入 data。
    }

    void cBMI088::ReadAccTemperature(float *temp)
    {
        if (temp == nullptr)
        return;

        *temp = InvalidValue();

        uint8_t buffer[TEMP_LEN] = {};

        if (!ReadReg(BMI088_CS_ACC, TEMP_MSB_ADDR,
                    buffer, TEMP_LEN))
            return;

        int32_t raw =
            (static_cast<int32_t>(buffer[0]) << 3) |
            (buffer[1] >> 5);

        // 0x400 对应手册中的无效温度编码。
        if (raw == 0x400)
            return;

        // 11 位补码的符号扩展。
        if (raw >= 1024)
            raw -= 2048;

        *temp = raw * TEMP_UNIT + TEMP_BIAS;

        // TODO: 丢弃加速度计读取时的首个无效字节，解析 11 位有符号温度并换算为摄氏度。

    }

        // 用 NaN 表示本次没有有效测量，避免把通信失败当成 0。
    float cBMI088::InvalidValue()
    {
        return std::numeric_limits<float>::quiet_NaN();
    }

    // 两个字节拼成有符号的 16 位整数：低字节在前。
    int16_t cBMI088::DecodeSigned16(uint8_t low, uint8_t high)
    {
        uint16_t bits =
            static_cast<uint16_t>(low) |
            (static_cast<uint16_t>(high) << 8);

        int32_t value = bits;

        if (bits & 0x8000U)
            value -= 65536;

        return static_cast<int16_t>(value);
    }

}

