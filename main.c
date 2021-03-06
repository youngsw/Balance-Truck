/**
  ******************************************************************************
  * 文件名程: main.c
  * 作    者: By Sw Young
  * 版    本: V1.0
  * 功    能:
  * 编写日期:
  ******************************************************************************
  * 说明：
  * 硬件平台：
  *   MCUc:TM4C123、2相四线步进电机、DRV8825电机驱动、WiFi
  * 软件设计说明：
  *   通过无线精确控制小车的前进、后退距离；左转右转角度。
  * Github：
  ******************************************************************************
**/
#include <stdint.h>
#include <stdbool.h>

#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_gpio.h"
#include "inc/hw_ints.h"
#include "driverlib/fpu.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/timer.h"
#include "driverlib/gpio.h"
#include "driverlib/adc.h"
#include "driverlib/pwm.h"
#include "Motor/motor.h"
#include "delay/delay.h"
#include "uart/uart.h"
#include "head.h"
#include "Timer/Timer.h"
#include "Hardware/I2C/I2C.h"
#include "Hardware/MPU6050/mpu6050.h"
#include "Hardware/MPU6050/mpu6050_firRefi.h"
#include "Hardware/DMP/inv_mpu.h"
#include "Hardware/DMP/inv_mpu_dmp_motion_driver.h"

extern uint32_t Counter;
extern uint8_t MotorOrderDirection;        //前：0  后：1  左：2  右： 3
extern uint8_t MotorOrderDisplacement;     //前后表示距离，左右表示转向角
uint32_t CountBan = 65335;                 //Counter最大值65535，计数一圈6400故，有效计数为10圈，即200cm
uint32_t SendCount = 0;                    //发送计数变量
uint8_t FlagSend = 0;                      //发送标志位
uint8_t Time;                              //时间变量
uint8_t CharTable[10];                     //回传数组
uint32_t CharNum;                          //临时数据
extern uint8_t Beep_Flag;                  //蜂鸣器标志位
uint8_t Flag_Stop = 0;                     //暂停标志
uint8_t Car_Mode = 0;                      //小车模式控制位，默认为遥控控制
uint8_t Direction_Set = 0;
extern bool followLineFlag;
/**
 * MPU6050相关
 * MPU6050的使用
 */
uint16_t tmp;                   //温度
short aacx,aacy,aacz;           //加速度传感器原始数据
short gyrox,gyroy,gyroz;        //陀螺仪原始数据
float pitch,roll,yaw,pitchStd;           //欧拉角

float parameter_Ang = 95.0;     //角度参数修正
float parameter_Dis = 340.0;    //距离参数修正
/**
  * 函 数 名:main.c
  * 函数功能: 主函数
  * 输入参数: 无
  * 返 回 值: 无
  * 说    明: 主函数
  *   By Sw Young
  *   2018.03.29
  */
int main(void)



{
    //
    // Enable lazy stacking for interrupt handlers.  This allows floating-point
    // instructions to be used within interrupt handlers, but at the expense of
    // extra stack usage.
    //
    FPUEnable();        //开启浮点运算
    FPULazyStackingEnable();
    SysCtlClockSet(SYSCTL_SYSDIV_2_5|SYSCTL_USE_PLL|SYSCTL_XTAL_16MHZ|SYSCTL_OSC_MAIN);
    //
    // Enable the GPIO port that is used for the on-board LED.
    //
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);

    //
    // Enable the GPIO pins for the LED (PF2).
    //
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3);

    //GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, GPIO_PIN_2);

    Uart0Iint();        //串口0初始化
    Uart1Iint();        //串口1初始化
    UART2Iint();        //串口1初始化
    MotorInit();        //电机控制引脚初始化
    MotorContolTimer(); //电机控制定时器初始化
    MotorSet(3,0,0);    //设置电机初始为制动
    followLineInit();   //巡线IO口初始化
    Beep_Configure();   //蜂鸣器初始化
    TimerEnable(TIMER1_BASE, TIMER_A);//关闭使能定时器



    /**
     * MPU6050初始化
     */
    MPU_Init();
    while(mpu_dmp_init())               //已关闭dmp的自检
    {//进入while，检测不到MPU6050
        GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, ~GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));
        delay_ms(1000);
        //放置mpu错误报警
    }//放置正常报警
    UARTprintf("mpu is ok!");
    delay_ms(1000);delay_ms(1000);delay_ms(1000);delay_ms(1000);delay_ms(1000);delay_ms(1000);delay_ms(1000);delay_ms(1000);//初始化8s
    if(mpu_dmp_get_data(&pitch,&roll,&yaw)==0)
    {
        //temp=MPU_Get_Temperature();   //得到温度值
        MPU_Get_Accelerometer(&aacx,&aacy,&aacz);   //得到加速度传感器数据
        MPU_Get_Gyroscope(&gyrox,&gyroy,&gyroz);    //得到陀螺仪数据
    }
    pitchStd = pitch;
    //UARTprintf("TEST");   //UARTprint函数输出重定向至UART1

    while(1)    //Loop
    {
        while(follo wLineFlag==true)
        {
            followLine();
        }


        if(MotorOrderDirection==0)  //B
        {
           // Counter = 0; //计数清零
            MotorSet(2,0,1);
        }
        if(MotorOrderDirection==1)  //F
        {
           // Counter = 0; //计数清零
            MotorSet(2,1,0);
        }
        if(MotorOrderDirection==2&&Direction_Set)  //L
        {
           // Counter = 0; //计数清零
            MotorSet(2,1,1);
            //Direction_Set = 0;
        }
        if(MotorOrderDirection==3&&Direction_Set)  //R
        {
           // Counter = 0; //计数清零
            MotorSet(2,0,0);
            //MotorSet(1,0,0);
            //Direction_Set = 0;
        }
        //计算公式：200步一转，一转20cm,32细分；Distance = 20*Counter/(200*32)；
        if(MotorOrderDirection==0||MotorOrderDirection==1)
        {
            CountBan = MotorOrderDisplacement*parameter_Dis;
            //UARTprintf("Dis%d",(Counter*20)/6400);
        }
        //计算公式；轮距16cm，1/4弧长为25.1327cm，对应20cm为400步可求得1/4弧长为251步可知每十弧度27.888889步
        else if (MotorOrderDirection==2||MotorOrderDirection==3)
        {
//          CountBan = (int)MotorOrderDisplacement*2.788889*32;  //理论值89.3
            CountBan = (int)MotorOrderDisplacement*parameter_Ang;         //修正到90.2
            //UARTprintf("Ang%d",(int)(Counter/89.3));
        }

        if(Counter>CountBan)    //停止
        {
            if(Car_Mode==1||Direction_Set==0)
            {
                Counter = 0; //计数清零
                FlagSend = 0;
                MotorSet(3,0,0);//制动
                TimerDisable(TIMER0_BASE, TIMER_A);
                //TimerDisable(TIMER1_BASE, TIMER_A);
                Beep_Flag=0;
            }
            else
                ;
        }
        if(Flag_Stop)           //暂停
        {
            GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, ~GPIOPinRead(GPIO_PORTF_BASE, GPIO_PIN_1));
            delay_ms(200);
//            UARTprintf("Counter%d",Counter);
//            UARTprintf("CountBan%d",CountBan);
        }
        SendCount++;
        if(SendCount>1000)  //控制发送密度
        {
            SendCount = 0;
            if((MotorOrderDirection==0||MotorOrderDirection==1)&&FlagSend)
            {
//                UARTprintf("D%d",(Counter*20)/6400);
//                UARTprintf("T%d",Counter/10);
/*                //发送位移
                CharNum = (Counter*20)/6400+1;   //做修改
                CharTable[0] = 'D';
                CharTable[1] = CharNum/100+48;
                CharTable[2] = CharNum/10%10+48;
                CharTable[3] = CharNum%10+48;
                CharTable[4] = '0';
                CharTable[5] = '0';
                UART1Send(CharTable,10);

                //发送时间
                CharNum = Counter/10;
                CharTable[0] = 'T';
                CharTable[1] = CharNum/10000+48;
                CharTable[2] = CharNum/1000%10+48;
                CharTable[3] = CharNum/100%10+48;
                CharTable[4] = CharNum/10%10+48;
                CharTable[5] = CharNum%10+48;
                UART1Send(CharTable,10);
*/
//                OLED_ShowNum(28,3,(Counter*20)/6400,3,16);
//                OLED_ShowNum(98,3,(int)(Counter/10),3,16);
            }
            else if ((MotorOrderDirection==2||MotorOrderDirection==3)&&FlagSend)
            {
//                UARTprintf("A%d",(int)(Counter/89.3));
//                UARTprintf("T%d",Counter/10);
                //发送位移
                CharNum = (Counter/(int)parameter_Ang); //做修正
                CharTable[0] = 'A';
                CharTable[1] = CharNum/100+48;
                CharTable[2] = CharNum/10%10+48;
                CharTable[3] = CharNum%10+48;
                CharTable[4] = '0';
                CharTable[5] = '0';
                UART1Send(CharTable,10);

                //发送时间
                CharNum = Counter/10;
                CharTable[0] = 'T';
                CharTable[1] = CharNum/10000+48;
                CharTable[2] = CharNum/1000%10+48;
                CharTable[3] = CharNum/100%10+48;
                CharTable[4] = CharNum/10%10+48;
                CharTable[5] = CharNum%10+48;
                UART1Send(CharTable,10);

//                OLED_ShowNum(28,6,(int)(Counter/89.3),3,16);
//                OLED_ShowNum(98,3,(int)(Counter/10),3,16);
            }
        }
        if(mpu_dmp_get_data(&pitch,&roll,&yaw)==0)
        {
            //temp=MPU_Get_Temperature();   //得到温度值
            MPU_Get_Accelerometer(&aacx,&aacy,&aacz);   //得到加速度传感器数据
            MPU_Get_Gyroscope(&gyrox,&gyroy,&gyroz);    //得到陀螺仪数据
//            if(1){UARTprintf("Pitch  %d  ", (int)pitch);}
//            if(1){UARTprintf("Roll  %d  ", (int)roll);}
//            if(1){UARTprintf("Yaw  %d\n", (int)yaw);}
        }//end if
    }

  //  return 0;
}
