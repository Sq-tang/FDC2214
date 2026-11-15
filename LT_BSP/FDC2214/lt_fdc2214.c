#include "fdc2214_iic.h"
#include "fdc2214.h"
#include "sys.h"
#include "delay.h"
#include "stdlib.h"
#include "math.h"


uint8_t CHx_FIN_SEL[4];
double fREFx[4];
double FRE[4];
u32 ch0,ch1,ch2,ch3;

/* 保留最近 20 个有效原始样本；每次丢弃一个最大值和一个最小值。 */
#define FILTER_SAMPLES 20U
static uint32_t sample_window[FILTER_SAMPLES];
static uint8_t sample_count; /* 已填充的有效样本数，最多为 20。 */
static uint8_t sample_index; /* 环形缓冲区下次写入位置，覆盖最旧样本。 */

/*!
 *  @brief      IIC写FDC2214
 *  @param      Slve_Addr     器件地址
 *  @param      reg           寄存器
 *  @param      data          数据
 *  @since      v1.0
 *  Sample usage:       FDC2214_Write16(FDC2214_Addr, FDC2214_CONFIG, 0xFFFF);
 */
void FDC2214_Write16(uint8_t Slve_Addr, uint8_t reg, uint16_t data)
{
    u8 dat;
    FDC2214_IIC_Start();

    FDC2214_IIC_Send_Byte(Slve_Addr << 1);
    FDC2214_IIC_Wait_Ack();

    FDC2214_IIC_Send_Byte(reg);
    FDC2214_IIC_Wait_Ack();

    dat=(data >> 8);
    FDC2214_IIC_Send_Byte(dat);
    FDC2214_IIC_Wait_Ack();

    dat=data & 0xFF;
    FDC2214_IIC_Send_Byte(dat);
    FDC2214_IIC_Wait_Ack();

    FDC2214_IIC_Stop();
}

/*!
 *  @brief      IIC读FDC2214
 *  @param      Slve_Addr     器件地址
 *  @param      reg           寄存器
 *  @return     寄存器值
 *  @since      v1.0
 *  Sample usage:       FDC2214_Read16(FDC2214_Addr, FDC2214_CONFIG);
 */
uint16_t FDC2214_Read16(uint8_t Slve_Addr, uint8_t reg)
{
    uint16_t temp;

    FDC2214_IIC_Start();
    FDC2214_IIC_Send_Byte(Slve_Addr << 1);
    FDC2214_IIC_Wait_Ack();
    FDC2214_IIC_Send_Byte(reg);
    FDC2214_IIC_Wait_Ack();
    FDC2214_IIC_Start();
    FDC2214_IIC_Send_Byte((Slve_Addr << 1) | 0x01);
    FDC2214_IIC_Wait_Ack();
    temp = (uint16_t)(FDC2214_IIC_Read_Byte(1) << 8);
    temp |= FDC2214_IIC_Read_Byte(0);
    FDC2214_IIC_Stop();
    return temp;
}

/*!
 *  @brief      读取FDC2214一个通道的转换值
 *  @param      channel     通道
 *  @param      *data       读取的数据
 *  @return     0：失败；1：成功
 *  @since      v1.0
 *  Sample usage:       FDC2214_GetChannelData(FDC2214_Channel_0, &CH0_DATA);
 */
uint8_t FDC2214_GetChannelData(FDC2214_channel_t channel, uint32_t *data)
{
    /* 最多轮询 100 次状态寄存器；次数上限不等于 100 ms。 */
    int16_t timeout = 100;
    uint8_t dateReg_H;
    uint8_t dateReg_L;
    uint8_t bitUnreadConv;
    uint16_t status;
    uint32_t dataRead;
    switch(channel)
    {
    case FDC2214_Channel_0:
        dateReg_H = FDC2214_DATA_CH0;
        dateReg_L = FDC2214_DATA_LSB_CH0;
        bitUnreadConv = 0x0008;
        break;

    case FDC2214_Channel_1:
        dateReg_H = FDC2214_DATA_CH1;
        dateReg_L = FDC2214_DATA_LSB_CH1;
        bitUnreadConv = 0x0004;
        break;

    case FDC2214_Channel_2:
        dateReg_H = FDC2214_DATA_CH2;
        dateReg_L = FDC2214_DATA_LSB_CH2;
        bitUnreadConv = 0x0002;
        break;

    case FDC2214_Channel_3:
        dateReg_H = FDC2214_DATA_CH3;
        dateReg_L = FDC2214_DATA_LSB_CH3;
        bitUnreadConv = 0x0001;
        break;
    }
    status = FDC2214_Read16(FDC2214_Addr, FDC2214_STATUS);//读取状态寄存器的值
    while(timeout && !(status & bitUnreadConv))//检查是否有未读数据
    {
        status = FDC2214_Read16(FDC2214_Addr, FDC2214_STATUS);
        timeout--;
    }
    if(timeout)
    {
        dataRead = ((uint32_t)FDC2214_Read16(FDC2214_Addr, dateReg_H) << 16);
        dataRead |= FDC2214_Read16(FDC2214_Addr, dateReg_L);
        /* 高 4 位为转换错误标志，低 28 位才是数据；拒绝零值以避免除零。 */
        if ((dataRead & 0xF0000000UL) != 0 || dataRead == 0)
            return 0;
        *data = dataRead & 0x0FFFFFFFUL;
        return 1;
    }
    else
    {
        //超时
        return 0;
    }
}

/*!
 *  @brief      设置FDC2214一个通道的转换时间，转换时间(tCx)=(CHx_RCOUNT?16) / fREFx。
 *  @param      channel     通道
 *  @param      rcount      CHx_RCOUNT
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetRcount(FDC2214_Channel_0, 0x0FFF);
 */
void FDC2214_SetRcount(FDC2214_channel_t channel, uint16_t rcount)
{
    if(rcount <= 0x00FF) return;

    switch(channel)
    {
    case FDC2214_Channel_0:
        FDC2214_Write16(FDC2214_Addr, FDC2214_RCOUNT_CH0, rcount);
        break;

    case FDC2214_Channel_1:
        FDC2214_Write16(FDC2214_Addr, FDC2214_RCOUNT_CH1, rcount);
        break;

    case FDC2214_Channel_2:
        FDC2214_Write16(FDC2214_Addr, FDC2214_RCOUNT_CH2, rcount);
        break;

    case FDC2214_Channel_3:
        FDC2214_Write16(FDC2214_Addr, FDC2214_RCOUNT_CH3, rcount);
        break;
    }
}

/*!
 *  @brief      设置FDC2214一个通道的建立时间，使LC传感器可以在通道x开始转换之前稳定下来，建立时间(tSx) = (CHx_SETTLECOUNT?16) ÷ fREFx。
                CHx_SETTLECOUNT = 0x00, 0x01时(tSx) = 32 ÷ fREFx。
 *  @param      channel     通道
 *  @param      count       CHx_SETTLECOUNT
 *  @since      v1.0
 *  @note       CHx_SETTLECOUNT > Vpk × fREFx × C × π^2 / (32 × IDRIVEX)
 *  Sample usage:       FDC2214_SetSettleCount(FDC2214_Channel_0, 0x00FF);
 */
void FDC2214_SetSettleCount(FDC2214_channel_t channel, uint16_t count)
{
    switch(channel)
    {
    case FDC2214_Channel_0:
        FDC2214_Write16(FDC2214_Addr, FDC2214_SETTLECOUNT_CH0, count);
        break;

    case FDC2214_Channel_1:
        FDC2214_Write16(FDC2214_Addr, FDC2214_SETTLECOUNT_CH1, count);
        break;

    case FDC2214_Channel_2:
        FDC2214_Write16(FDC2214_Addr, FDC2214_SETTLECOUNT_CH2, count);
        break;

    case FDC2214_Channel_3:
        FDC2214_Write16(FDC2214_Addr, FDC2214_SETTLECOUNT_CH3, count);
        break;
    }
}

/*!
 *  @brief      设置FDC2214一个通道的的时钟频率,
                差分传感器配置：
                0x01 -- 传感器频率0.01MHz和8.75MHz
                0x02 -- 传感器频率5MHz和10MHz
                单端传感器配置：
                0x01 -- 传感器频率0.01MHz和10MHz
 *  @param      channel               通道
 *  @param      frequency_select      时钟选择
 *  @param      divider               分频器，fREFx = fCLK / CHx_FREF_DIVIDER，
 *  @note       fREFx is > 4 × fSENSOR(外部LC振荡器频率)
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetChannelClock(FDC2214_Channel_0, 0x01, 0xFF);
 */
void FDC2214_SetChannelClock(FDC2214_channel_t channel, uint8_t frequency_select, uint16_t divider)
{
    uint16_t temp = 0;

    temp = (uint16_t)(frequency_select << 12) | (divider & 0x03FF);

    CHx_FIN_SEL[channel] = frequency_select;//记录时钟选择
    fREFx[channel] = (double)ClockFrequency / divider;//记录时钟频率

    switch(channel)
    {
    case FDC2214_Channel_0:
        FDC2214_Write16(FDC2214_Addr, FDC2214_CLOCK_DIVIDERS_CH0, temp);
        break;

    case FDC2214_Channel_1:
        FDC2214_Write16(FDC2214_Addr, FDC2214_CLOCK_DIVIDERS_CH1, temp);
        break;

    case FDC2214_Channel_2:
        FDC2214_Write16(FDC2214_Addr, FDC2214_CLOCK_DIVIDERS_CH2, temp);
        break;

    case FDC2214_Channel_3:
        FDC2214_Write16(FDC2214_Addr, FDC2214_CLOCK_DIVIDERS_CH3, temp);
        break;
    }
}

/*!
 *  @brief      配置INTB引脚中断功能
 *  @param      mode      1：开中断；0：关中断
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetDriveCurrent(FDC2214_Channel_0, FDC2214_Drive_Current_0_025);
 */
void FDC2214_SetINTB(uint8_t mode)
{
    uint16_t temp = 0;

    temp = FDC2214_Read16(FDC2214_Addr, FDC2214_CONFIG);
    temp &= 0xFF7F;
    if(mode)
        temp |= 0x0080;
    FDC2214_Write16(FDC2214_Addr, FDC2214_CONFIG, temp);

    temp = FDC2214_Read16(FDC2214_Addr, FDC2214_ERROR_CONFIG);
    temp &= 0xFFFE;
    if(mode)
        temp |= 0x0001;
    FDC2214_Write16(FDC2214_Addr, FDC2214_ERROR_CONFIG, temp);
}

/*!
 *  @brief      激活FDC2214的一个通道，单通道模式
 *  @param      channel     通道
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetActiveChannel(FDC2214_Channel_0);
 */
void FDC2214_SetActiveChannel(FDC2214_channel_t channel)
{
    uint16_t temp = 0;
    temp = FDC2214_Read16(FDC2214_Addr, FDC2214_CONFIG);
    temp &= 0x3FFF;
    temp |= (uint16_t)(channel << 14);

    FDC2214_Write16(FDC2214_Addr, FDC2214_CONFIG, temp);
}

/*!
 *  @brief      设置FDC2214的工作模式：正常、睡眠。
 *  @param      mode     0：正常工作；1：睡激活模式选择。眠
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetSleepMode(0);
 */
void FDC2214_SetSleepMode(uint8_t mode)
{
    uint16_t temp = 0;
    temp = FDC2214_Read16(FDC2214_Addr, FDC2214_CONFIG);
    temp &= 0xDFFF;
    if(mode)temp |= 0x2000;

    FDC2214_Write16(FDC2214_Addr, FDC2214_CONFIG, temp);
}

/*!
 *  @brief      FDC2214激活模式选择。
 *  @param      mode     0：全电流激活模式；1：低功率激活模式
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetCurrentMode(1);
 */
void FDC2214_SetCurrentMode(uint8_t mode)
{
    uint16_t temp = 0;
    temp = FDC2214_Read16(FDC2214_Addr, FDC2214_CONFIG);
    temp &= 0xF7FF;
    if(mode)
        temp |= 0x800;

    FDC2214_Write16(FDC2214_Addr, FDC2214_CONFIG, temp);
}

/*!
 *  @brief      设置FDC2214时钟源
 *  @param      src     0：内部时钟源；1：外部时钟源
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetClockSource(1);
 */
void FDC2214_SetClockSource(uint8_t src)
{
    uint16_t temp = 0;
    temp = FDC2214_Read16(FDC2214_Addr, FDC2214_CONFIG);
    temp &= 0xFDFF;
    if(src)
        temp |= 0x200;

    FDC2214_Write16(FDC2214_Addr, FDC2214_CONFIG, temp);
}

/*!
 *  @brief      高电流传感器驱动，只适用于单通道模式。
 *  @param      mode     0：正常（最大1.5mA）；1：高电流传感器驱动（>1.5mA）
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetHighCurrentMode(0);
 */
void FDC2214_SetHighCurrentMode(uint8_t mode)
{
    uint16_t temp = 0;
    temp = FDC2214_Read16(FDC2214_Addr, FDC2214_CONFIG);
    temp &= 0xFFBF;
    if(mode)
        temp |= 0x40;

    FDC2214_Write16(FDC2214_Addr, FDC2214_CONFIG, temp);
}

/*!
 *  @brief      设置FDC2214的MUX CONFIG寄存器
 *  @param      autoscan      自动扫描模式 0：关闭，1：打开
 *  @param      channels      自动扫描通道
 *  @param      bandwidth     数字滤波器带宽，大于外部LC振荡器频率
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetMUX_CONFIG(1, FDC2214_Channel_Sequence_0_1_2, FDC2214_Bandwidth_10M);
 */
void FDC2214_SetMUX_CONFIG(uint8_t autoscan, FDC2214_channel_sequence_t channels, FDC2214_filter_bandwidth_t bandwidth)
{
    uint16_t temp = 0;

    if(autoscan)
    {
        temp = (uint16_t)(autoscan << 15) | (channels << 13) | 0x0208 | bandwidth;
    }
    else
    {
        temp = 0x0208 | bandwidth;
    }

    FDC2214_Write16(FDC2214_Addr, FDC2214_MUX_CONFIG, temp);
}

/*!
 *  @brief      传感器复位
 *  @since      v1.0
 *  Sample usage:       FDC2214_Reset();
 */
void FDC2214_Reset(void)
{
    FDC2214_Write16(FDC2214_Addr, FDC2214_RESET_DEV, 0x8000);
}

/*!
 *  @brief      设置FDC2214的一个通道传感器驱动电流，确保振荡幅度在1.2V和1.8V之间。
 *  @param      channel      通道
 *  @param      current      电流大小
 *  @since      v1.0
 *  Sample usage:       FDC2214_SetDriveCurrent(FDC2214_Channel_0, FDC2214_Drive_Current_0_025);
 */
void FDC2214_SetDriveCurrent(FDC2214_channel_t channel, FDC2214_drive_current_t current)
{
    uint16_t temp = 0;
    temp = (uint16_t)(current << 11);

    switch(channel)
    {
    case FDC2214_Channel_0:
        FDC2214_Write16(FDC2214_Addr, FDC2214_DRIVE_CURRENT_CH0, temp);
        break;

    case FDC2214_Channel_1:
        FDC2214_Write16(FDC2214_Addr, FDC2214_DRIVE_CURRENT_CH1, temp);
        break;

    case FDC2214_Channel_2:
        FDC2214_Write16(FDC2214_Addr, FDC2214_DRIVE_CURRENT_CH2, temp);
        break;

    case FDC2214_Channel_3:
        FDC2214_Write16(FDC2214_Addr, FDC2214_DRIVE_CURRENT_CH3, temp);
        break;
    }
}

/*!
 *  @brief      计算频率
 *  @param      channel      通道
 *  @param      datax        读取的转换值
 *  @return     频率，单位Hz
 *  @since      v1.0
 *  Sample usage:       FDC2214_CalculateFrequency(FDC2214_Channel_0, 0xFF);
 */
double FDC2214_CalculateFrequency(FDC2214_channel_t channel, uint32_t datax)
{
    double frequency = 0.0;

    frequency = (double)(CHx_FIN_SEL[channel] * fREFx[channel] * datax) / 268435456.0;//2^28 0.298*data

    return frequency;
}

/*!
 *  @brief      计算外部电容大小
 *  @param      frequency      频率大小，单位Hz
 *  @param      inductance     板载电感大小，单位uH
 *  @param      capacitance    板载电容大小，单位pF
 *  @return     外部电容大小，单位pF
 *  @since      v1.0
 *  Sample usage:       FDC2214_CalculateCapacitance(FDC2214_Channel_0, 6000000, 18, 33);
 */
double FDC2214_CalculateCapacitance(double frequency, float inductance, float capacitance)
{
    double cap = 0.0;

    cap = (1000000000000000000/ (inductance * (2 * PI * frequency) * (2 * PI * frequency)))-capacitance;
    return cap;
}

/*!
 *  @brief      FDC2214初始化函数
 *  @return     1：成功；0：失败
 *  @since      v1.0
 *  Sample usage:       FDC2214_Init();
 */
/* 单通道初始化：成功返回 0，设备 ID 不匹配返回 1。
 * 仅启用物理 CH0；保留原有转换时间及激励参数，避免单纯缩短转换时间。
 */
uint8_t FDC2214_Init(void)
{
    if (FDC2214_Read16(FDC2214_Addr, FDC2214_DEVICE_ID) != FDC2214_ID)
        return 1;
    FDC2214_Reset();
    /* 重新初始化时丢弃旧窗口，必须重新收满样本才输出。 */
    sample_count = 0;
    sample_index = 0;
    FDC2214_SetRcount(FDC2214_Channel_0, 10000); /* 10000 * 16 / 40 MHz = 4 ms */
    /* 建立时间参数：200 * 16 / 40 MHz = 80 us。 */
    FDC2214_SetSettleCount(FDC2214_Channel_0, 200);
    /* 输入频率选择为 2，参考时钟分频为 1；频率换算使用同一组参数。 */
    FDC2214_SetChannelClock(FDC2214_Channel_0, 1, 2);
    FDC2214_SetINTB(0);
    FDC2214_SetActiveChannel(FDC2214_Channel_0);
    FDC2214_SetCurrentMode(0);
    FDC2214_SetClockSource(1); /* 使用外部 40 MHz 参考时钟，需与实物一致。 */
    FDC2214_SetHighCurrentMode(1); /* Enable high-current drive for CH0-only operation. */
    /* 关闭自动扫描后，序列参数不生效；由 ActiveChannel 选择 CH0。 */
    FDC2214_SetMUX_CONFIG(0, FDC2214_Channel_Sequence_0_1, FDC2214_Bandwidth_10M);
    FDC2214_SetDriveCurrent(FDC2214_Channel_0, FDC2214_Drive_Current_1_571);
    FDC2214_SetSleepMode(0); /* 退出休眠，开始连续转换。 */
    delay_ms(100); /* 仅初始化阶段等待，不进入正常采样循环。 */
    return 0;
}

/**
 * 读取 CH0 的一次新转换，更新 20 点滑动去极值平均。
 * @param capacitance 输出电容，单位 pF；已扣除 33 pF，未扣主程序的 9 pF。
 * @return 1：已写入有效电容；0：空指针、读取失败或窗口尚未填满。
 * 返回 0 时不修改输出变量，调用者不得将旧值作为本次测量发送。
 * 首次收满 20 点后，每个新样本都产生一个结果；相邻结果共享 19 点，
 * 因此更高的输出频率不代表独立测量带宽同步提高。
 * 读取失败会清空窗口计数，恢复后重新预热，避免混入故障前的旧数据。
 */
uint8_t FDC2214_ReadCapacitance(double *capacitance)
{
    uint32_t raw, minimum, maximum;
    volatile uint64_t sum = 0; /* 20 个 28 位数累加可能超过 32 位范围。 */
    unsigned int i;
    if (capacitance == 0)
        return 0;
    if (!FDC2214_GetChannelData(FDC2214_Channel_0, &raw))
    {
        sample_count = 0;
        sample_index = 0;
        return 0;
    }
    /* 环形覆盖最旧值；窗口未满时只收集样本，不计算结果。 */
    sample_window[sample_index] = raw;
    sample_index = (sample_index + 1U) % FILTER_SAMPLES;
    if (sample_count < FILTER_SAMPLES)
        ++sample_count;
    if (sample_count < FILTER_SAMPLES)
        return 0;
    /* 对当前窗口求和并找极值，去掉两个极值后取剩余 18 点平均。 */
    minimum = maximum = sample_window[0];
    for (i = 0; i < FILTER_SAMPLES; ++i)
    {
        raw = sample_window[i];
        sum += raw;
        if (raw < minimum) minimum = raw;
        if (raw > maximum) maximum = raw;
    }
    ch0 = (uint32_t)((sum - minimum - maximum) / (FILTER_SAMPLES - 2U));
    /* 先换算频率，再按 18 uH 电感和 33 pF 固定电容求待测电容。 */
    FRE[0] = FDC2214_CalculateFrequency(FDC2214_Channel_0, ch0);
    *capacitance = FDC2214_CalculateCapacitance(FRE[0], 18, 33);
    return 1;
}

/* 旧批量滤波接口：一次收集 20 点，失败持续重试；单路主程序不再调用。 */
long filterread(FDC2214_channel_t channel, uint32_t *data)
{
    u16 i=0;
    long result=0;
    long Data[20],max=0,min=0;
    uint64_t Sum=0;
    for(i=0; i<=19; i++)
    {
        if(FDC2214_GetChannelData(channel,data)==1)
        {
            Data[i]=*data;
        }
        else
        {
            i--;
        }
        delay_us(100);
    }
    Sum=Data[0];
    max=Data[0];
    min=Data[0];
    for(i=1; i<=19; i++)
    {
        if(max<Data[i])max=Data[i];
        if(min>Data[i])min=Data[i];
        Sum=Data[i]+Sum;//累加
    }
    result=(Sum-max-min)/18;
    return result;
}
double getcap(u8 channel,double dg,double dr)
{
    double Cap=0.0;
    switch(channel)
    {
    case 0:
        ch0=filterread(FDC2214_Channel_0,&ch0);
        FRE[0]=FDC2214_CalculateFrequency(FDC2214_Channel_0,ch0);
        Cap=FDC2214_CalculateCapacitance(FRE[0], dg, dr);
        break;
    case 1:
        ch1=filterread(FDC2214_Channel_1,&ch1);
        FRE[1]=FDC2214_CalculateFrequency(FDC2214_Channel_1,ch1);
        Cap=FDC2214_CalculateCapacitance(FRE[1], dg, dr);
        break;
    case 2:
        ch2=filterread(FDC2214_Channel_2,&ch2);
        FRE[2]=FDC2214_CalculateFrequency(FDC2214_Channel_2,ch2);
        Cap=FDC2214_CalculateCapacitance(FRE[2], dg, dr);
        break;
    case 3:
        ch3=filterread(FDC2214_Channel_3,&ch3);
        FRE[3]=FDC2214_CalculateFrequency(FDC2214_Channel_3,ch3);
        Cap=FDC2214_CalculateCapacitance(FRE[3], dg, dr);
        break;
    }
    return Cap;
}


