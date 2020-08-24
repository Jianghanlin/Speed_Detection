// P1.1 is I/O interrupt input.Timer1_A is cintinuous mode,Measure mode is period.Timer0_A use for show period controlling
// When revolution is 3000RPM,period is 20ms
// if the accuracy is 0.1%,1000 pluses is need for 20ms
// so,counter pluse frequency is moer then 50KHz
//******************************************************************************

#include <msp430G2553.h>
#include "TCA6416A.h"
#include "HT1621.h"
#include "LCD_128.h"
#include "BCSplus_init.h"

#define fluses_turn_a_circle 1
unsigned int speed = 0;           //pluse脉冲数
unsigned int xiaoshu;             //存放显示小数
unsigned char show_flag = 0;      //显示标志
unsigned char overflow_times = 0; //溢出次数
unsigned char dis_buf[4] = {0, 0, 0, 0};

void GPIO_init();
void Timer1_init();
void Timer1_ISR();
void Timer1_stop();
void Timer0_init();
void Timer0_ISR();
void P1_IODect();
void P11_Onclick();
void Display();

int main(void)
{
    WDTCTL = WDTPW + WDTHOLD; // Stop WDT
    BCSplus_graceInit();
    GPIO_init();
    TCA6416A_Init(); // 初始化IO扩展口
    HT1621_init();   // 初始化lcd_128
    Timer0_init();
    _EINT();
    while (1) //显示转速
    {
        Display();
        LPM0;
    }
}


void Display()
{
    char i;
    char j;
    int xianshi = 0;
    if (show_flag)
    {
        _DINT();
        xianshi = speed;
        LCD_Clear();
        if(speed>999)//1000~9999（4位）
        {
            for (j = 0; j < 4; j++) //拆分整数数字
            {
                dis_buf[j] = xianshi % 10;
                xianshi /= 10;
            }
            for (i = 0; i < 4; i++)
            {
                LCD_DisplayDigit(dis_buf[i], 5 - i);
            }
        }
        else if(speed>99)//100~999（3位）
        {
            for (j = 0; j < 3; j++) //拆分整数数字
            {
                dis_buf[j] = xianshi % 10;
                xianshi /= 10;
            }
            for (i = 0; i < 3; i++)
            {
                LCD_DisplayDigit(dis_buf[i], 5 - i);
            }
        }
        else if(speed>9)//（2位）
        {
            for (j = 0; j < 2; j++) //拆分整数数字
            {
                dis_buf[j] = xianshi % 10;
                xianshi /= 10;
            }
            for (i = 0; i < 2; i++)
            {
                LCD_DisplayDigit(dis_buf[i], 5 - i);
            }
        }
        else
            LCD_DisplayDigit(xianshi,5);
        LCD_DisplayDigit(xiaoshu, 6); //显示小数
        LCD_DisplaySeg(60);           //显示小数点
        show_flag = 0;                //清显示标志
        HT1621_Reflash(LCD_Buffer);   //控制芯片RAM更新
        _EINT();
    }
}

void GPIO_init()
{
    //-----P1.1为脉冲信号输入，启用内部上拉电阻-----
    P1DIR &= ~BIT1; // P1.1设为输入(可省略)
    P1REN |= BIT1;  //启用P1.1内部上下拉电阻
    P1OUT |= BIT1;  //将电阻设置为上拉
    P1IES |= BIT1;  // P1.1设为下降沿中断
    P1IE |= BIT1;   // 允许P1.1中断
}

/******************************************************************************************************
 * 名       称：PORT1_ISR()
 * 功       能：响应P1口的外部中断服务
 * 入口参数：无
 * 出口参数：无
 * 说       明：P1.0~P1.8共用了PORT1中断，所以在PORT1_ISR()中必须查询标志位P1IFG才能知道
 *        具体是哪个IO引发了外部中断。P1IFG必须手动清除，否则将持续引发PORT1中断。
 * 范       例：无
 ******************************************************************************************************/
#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    //-----启用Port1事件检测函数-----
    P1_IODect();            //检测通过，则会调用事件处理函数
    P1IFG = 0;              //退出中断前必须手动清除IO口中断标志
    LPM0_EXIT;//修改堆栈中SR的值，返回后退出休眠
}

/******************************************************************************************************
 * 名       称：P1_IODect()
 * 功       能：判断具体引发中断的IO，并调用相应IO的中断事件处理函数
 * 入口参数：无
 * 出口参数：无
 * 说       明：该函数兼容所有8个IO的检测，请根据实际输入IO激活“检测代码”。
 *        本例中，仅有P1.1被用作输入IO，所以其他7个IO的“检测代码”没有被“激活”。
 * 范       例：无
 ******************************************************************************************************/
void P1_IODect(void)
{
    P11_Onclick();
}

/******************************************************************************************************
 * 名       称：P11_Onclick()
 * 功       能：P1.1的中断事件处理函数，即当P1.1下降沿到达后：
 * 1.读取时间值;
 * 2.启动timer1;
 * 3.计算脉冲周期=Tclk(现读取值+timer1溢出次数*0ffffH)
 * 入口参数：无
 * 出口参数：无
 * 说       明：使用事件处理函数的形式，可以增强代码的移植性和可读性
 * 范       例：无
 ******************************************************************************************************/
void P11_Onclick()
{
    static unsigned char measure_flag = 0; //隔周期测量一次
    static float revolution = 0;           //转速
    static float f = 32768;                //时钟频率
    measure_flag ^= BIT1;
    if (measure_flag)
    {
        Timer1_init(); //启动测量
    }
    else
    {
        speed = TA1R;
        revolution = ((65536 * overflow_times + speed) / f) * fluses_turn_a_circle;
        revolution = (1 / revolution) * 60;
        speed = (unsigned int)revolution; //speed为显示整数，在显示程序中调用
        revolution = revolution - speed;
        xiaoshu = 10 * revolution; //取出一位显示小数
        Timer1_stop();
    }
    overflow_times = 0; //清溢出次数
    TA1R = 0;
}

// Timer A1 初始化
void Timer1_init()
{

    TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE; // ACLK, Continous up,start,interrupt enable
}

// Timer A1 关闭
void Timer1_stop()
{
    TA1CTL |= MC_0 + TACLR; // stop
}

// Timer A1、CCR1、CCR2中断服务程序
#pragma vector = TIMER1_A1_VECTOR
__interrupt void TIMER1_A1(void)
{
    switch (TA1IV)
    {
    case TA1IV_NONE:
        break; // Vector  0:  No interrupt
    case TA1IV_TACCR1:
        break; // Vector  2:  TACCR1 CCIFG
    case TA1IV_TACCR2:
        break; // Vector  4:  TACCR2 CCIFG
    case TA1IV_6:
        break; // Vector  6:  Reserved CCIFG
    case TA1IV_8:
        break;        // Vector  8:  Reserved CCIFG
    case TA1IV_TAIFG: // Vector 10:  TAIFG
        Timer1_ISR(); //-----启用中断服务函数-----
        break;        // Vector 10:  TAIFG
    default:
        break;
    }
    LPM0_EXIT;
}

/******************************************************************************************************
 * 名       称：Timer1_ISR()
 * 功       能：脉冲测宽中断事件处理函数
 * 入口参数：无
 * 出口参数：溢出次数
 * 说       明：使用事件处理函数的形式，可以增强代码的移植性和可读性
 * 范       例：无
 ******************************************************************************************************/
void Timer1_ISR()
{
    overflow_times++; //溢出次数加1
    TA0CTL &=~TAIFG;
}

/******************************************************************************************************
 * 名       称：Timer0_init()
 * 功       能：2s timer show
  ******************************************************************************************************/
void Timer0_init()
{
    TA0CCR0=32768;
    TA0CTL = TASSEL_1 + MC_1 + TACLR + TAIE; // ACLK, Continous up,start,interrupt enable

}

// Timer A1、CCR1、CCR2中断服务程序
#pragma vector = TIMER0_A1_VECTOR
__interrupt void TIMER0_A0(void)
{
    switch (TA0IV)
    {
    case TA0IV_NONE:
        break; // Vector  0:  No interrupt
    case TA0IV_TACCR1:
        break; // Vector  2:  TACCR1 CCIFG
    case TA0IV_TACCR2:
        break; // Vector  4:  TACCR2 CCIFG
    case TA0IV_6:
        break; // Vector  6:  Reserved CCIFG
    case TA0IV_8:
        break;        // Vector  8:  Reserved CCIFG
    case TA0IV_TAIFG: // Vector 10:  TAIFG
        Timer0_ISR(); //-----启用中断服务函数-----
        break;        // Vector 10:  TAIFG
    default:
        break;
    }
    LPM0_EXIT;
}

/******************************************************************************************************
 * 名       称：Timer0_ISR()
 * 功       能：置显示标志
 * 入口参数：无
  ******************************************************************************************************/
void Timer0_ISR()
{
    show_flag = 1; //置显示标志
}
