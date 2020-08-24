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
unsigned int speed = 0;           //pluse������
unsigned int xiaoshu;             //�����ʾС��
unsigned char show_flag = 0;      //��ʾ��־
unsigned char overflow_times = 0; //�������
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
    TCA6416A_Init(); // ��ʼ��IO��չ��
    HT1621_init();   // ��ʼ��lcd_128
    Timer0_init();
    _EINT();
    while (1) //��ʾת��
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
        if(speed>999)//1000~9999��4λ��
        {
            for (j = 0; j < 4; j++) //�����������
            {
                dis_buf[j] = xianshi % 10;
                xianshi /= 10;
            }
            for (i = 0; i < 4; i++)
            {
                LCD_DisplayDigit(dis_buf[i], 5 - i);
            }
        }
        else if(speed>99)//100~999��3λ��
        {
            for (j = 0; j < 3; j++) //�����������
            {
                dis_buf[j] = xianshi % 10;
                xianshi /= 10;
            }
            for (i = 0; i < 3; i++)
            {
                LCD_DisplayDigit(dis_buf[i], 5 - i);
            }
        }
        else if(speed>9)//��2λ��
        {
            for (j = 0; j < 2; j++) //�����������
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
        LCD_DisplayDigit(xiaoshu, 6); //��ʾС��
        LCD_DisplaySeg(60);           //��ʾС����
        show_flag = 0;                //����ʾ��־
        HT1621_Reflash(LCD_Buffer);   //����оƬRAM����
        _EINT();
    }
}

void GPIO_init()
{
    //-----P1.1Ϊ�����ź����룬�����ڲ���������-----
    P1DIR &= ~BIT1; // P1.1��Ϊ����(��ʡ��)
    P1REN |= BIT1;  //����P1.1�ڲ�����������
    P1OUT |= BIT1;  //����������Ϊ����
    P1IES |= BIT1;  // P1.1��Ϊ�½����ж�
    P1IE |= BIT1;   // ����P1.1�ж�
}

/******************************************************************************************************
 * ��       �ƣ�PORT1_ISR()
 * ��       �ܣ���ӦP1�ڵ��ⲿ�жϷ���
 * ��ڲ�������
 * ���ڲ�������
 * ˵       ����P1.0~P1.8������PORT1�жϣ�������PORT1_ISR()�б����ѯ��־λP1IFG����֪��
 *        �������ĸ�IO�������ⲿ�жϡ�P1IFG�����ֶ���������򽫳�������PORT1�жϡ�
 * ��       ������
 ******************************************************************************************************/
#pragma vector = PORT1_VECTOR
__interrupt void PORT1_ISR(void)
{
    //-----����Port1�¼���⺯��-----
    P1_IODect();            //���ͨ�����������¼�������
    P1IFG = 0;              //�˳��ж�ǰ�����ֶ����IO���жϱ�־
    LPM0_EXIT;//�޸Ķ�ջ��SR��ֵ�����غ��˳�����
}

/******************************************************************************************************
 * ��       �ƣ�P1_IODect()
 * ��       �ܣ��жϾ��������жϵ�IO����������ӦIO���ж��¼�������
 * ��ڲ�������
 * ���ڲ�������
 * ˵       �����ú�����������8��IO�ļ�⣬�����ʵ������IO��������롱��
 *        �����У�����P1.1����������IO����������7��IO�ġ������롱û�б��������
 * ��       ������
 ******************************************************************************************************/
void P1_IODect(void)
{
    P11_Onclick();
}

/******************************************************************************************************
 * ��       �ƣ�P11_Onclick()
 * ��       �ܣ�P1.1���ж��¼�������������P1.1�½��ص����
 * 1.��ȡʱ��ֵ;
 * 2.����timer1;
 * 3.������������=Tclk(�ֶ�ȡֵ+timer1�������*0ffffH)
 * ��ڲ�������
 * ���ڲ�������
 * ˵       ����ʹ���¼�����������ʽ��������ǿ�������ֲ�ԺͿɶ���
 * ��       ������
 ******************************************************************************************************/
void P11_Onclick()
{
    static unsigned char measure_flag = 0; //�����ڲ���һ��
    static float revolution = 0;           //ת��
    static float f = 32768;                //ʱ��Ƶ��
    measure_flag ^= BIT1;
    if (measure_flag)
    {
        Timer1_init(); //��������
    }
    else
    {
        speed = TA1R;
        revolution = ((65536 * overflow_times + speed) / f) * fluses_turn_a_circle;
        revolution = (1 / revolution) * 60;
        speed = (unsigned int)revolution; //speedΪ��ʾ����������ʾ�����е���
        revolution = revolution - speed;
        xiaoshu = 10 * revolution; //ȡ��һλ��ʾС��
        Timer1_stop();
    }
    overflow_times = 0; //���������
    TA1R = 0;
}

// Timer A1 ��ʼ��
void Timer1_init()
{

    TA1CTL = TASSEL_1 + MC_2 + TACLR + TAIE; // ACLK, Continous up,start,interrupt enable
}

// Timer A1 �ر�
void Timer1_stop()
{
    TA1CTL |= MC_0 + TACLR; // stop
}

// Timer A1��CCR1��CCR2�жϷ������
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
        Timer1_ISR(); //-----�����жϷ�����-----
        break;        // Vector 10:  TAIFG
    default:
        break;
    }
    LPM0_EXIT;
}

/******************************************************************************************************
 * ��       �ƣ�Timer1_ISR()
 * ��       �ܣ��������ж��¼�������
 * ��ڲ�������
 * ���ڲ������������
 * ˵       ����ʹ���¼�����������ʽ��������ǿ�������ֲ�ԺͿɶ���
 * ��       ������
 ******************************************************************************************************/
void Timer1_ISR()
{
    overflow_times++; //���������1
    TA0CTL &=~TAIFG;
}

/******************************************************************************************************
 * ��       �ƣ�Timer0_init()
 * ��       �ܣ�2s timer show
  ******************************************************************************************************/
void Timer0_init()
{
    TA0CCR0=32768;
    TA0CTL = TASSEL_1 + MC_1 + TACLR + TAIE; // ACLK, Continous up,start,interrupt enable

}

// Timer A1��CCR1��CCR2�жϷ������
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
        Timer0_ISR(); //-----�����жϷ�����-----
        break;        // Vector 10:  TAIFG
    default:
        break;
    }
    LPM0_EXIT;
}

/******************************************************************************************************
 * ��       �ƣ�Timer0_ISR()
 * ��       �ܣ�����ʾ��־
 * ��ڲ�������
  ******************************************************************************************************/
void Timer0_ISR()
{
    show_flag = 1; //����ʾ��־
}
