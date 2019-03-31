/*
 * File:   main.c
 * Author: eagle
 *
 * Created on 2019/03/30, 2:33
 */


#include <xc.h>
#define _XTAL_FREQ 8000000    //  delay用に必要(クロック8MHzを指定)
#define HIGH 1
#define LOW 0
#define LED PORTAbits.RA4

// コンフィギュレーション１の設定
#pragma config FOSC     = INTOSC   // 内部ｸﾛｯｸ使用する(INTOSC)
#pragma config WDTE     = OFF      // ｳｵｯﾁﾄﾞｯｸﾞﾀｲﾏｰ無し(OFF)
#pragma config PWRTE    = ON       // 電源ONから64ms後にﾌﾟﾛｸﾞﾗﾑを開始する(ON)
#pragma config MCLRE    = OFF      // 外部ﾘｾｯﾄ信号は使用せずにﾃﾞｼﾞﾀﾙ入力(RA3)ﾋﾟﾝとする(OFF)
#pragma config CP       = OFF      // ﾌﾟﾛｸﾞﾗﾑﾒﾓﾘｰを保護しない(OFF)
#pragma config BOREN    = ON       // 電源電圧降下常時監視機能ON(ON)
#pragma config CLKOUTEN = OFF      // CLKOUTﾋﾟﾝをRA4ﾋﾟﾝで使用する(OFF)
#pragma config IESO     = OFF      // 外部・内部ｸﾛｯｸの切替えでの起動はなし(OFF)
#pragma config FCMEN    = OFF      // 外部ｸﾛｯｸ監視しない(OFF)
// コンフィギュレーション２の設定
#pragma config WRT      = OFF      // Flashﾒﾓﾘｰを保護しない(OFF)
#pragma config PPS1WAY  = OFF      // ロック解除シーケンスを実行すれば何度でもPPSLOCKをセット/クリアできる
#pragma config ZCDDIS   = ON       // ゼロクロス検出回路は無効とする(ON)
#pragma config PLLEN    = OFF      // 動作クロックを32MHzでは動作させない(OFF)
#pragma config STVREN   = ON       // スタックがオーバフローやアンダーフローしたらリセットをする(ON)
#pragma config BORV     = HI       // 電源電圧降下常時監視電圧(2.5V)設定(HI)
#pragma config LPBOR    = OFF      // 低消費電力ブラウンアウトリセット(LPBOR_OFF)
#pragma config LVP      = OFF      // 低電圧プログラミング機能使用しない(OFF)

int threshold = 0;
int DASHCNT = 150;
int NOSIGNALCNT = 300;
int JOYCNT = 100;

void Blink(int n)
{
    for(int i = 0; i < n; i++)
    {
        LED = HIGH;
        __delay_ms(150);
        LED = LOW;
        __delay_ms(150);
    }
}

void SendChar(char c)
{
    while(TXIF == 0);
    TXREG = c;
}

void interrupt isr()
{
    if(PIR1bits.RCIF)
    {
        Blink(1);
        char rx_data = RCREG;
        switch(rx_data)
        {
            case 'd' : DASHCNT+=10; break;   //increase dash counter
            case 's' : DASHCNT=DASHCNT > 10 ? DASHCNT-10 : DASHCNT; break;   //decrease dash counter
            case 'n' : NOSIGNALCNT+=10; break;   //increase noSignal counter
            case 'b' : NOSIGNALCNT=NOSIGNALCNT > 10 ? NOSIGNALCNT-10 : NOSIGNALCNT; break;   //decrease noSignal counter
            case 'j' : JOYCNT+=10; break;   //increase JOYCNT counter
            case 'h' : JOYCNT=JOYCNT > 10 ? JOYCNT-10 : JOYCNT; break;   //decrease JOYCNT counter
            default : break;
        }  
        PIR1bits.RCIF = 0;
        SendChar('r');
    }
}

int adc(char ch)
{
    switch(ch)
    {
        case 'x' : ADCON0 = 0b0010101;   break;   //RC1(AN5) X-axis joy and Click
        case 'y' : ADCON0 = 0b0011001;   break;   //RC2(AN6) Y-axis joy
        case 'f' : ADCON0 = 0b0011101;   break;   //RC3(AN7) force sensor
    }
    __delay_ms(1);
    
    GO_nDONE = 1;
    while(GO_nDONE);
    
    int value = ADRESH;
    value = (value << 8) + ADRESL;
    return value;
}

void Initialize()
{
    //ピンの各種設定
    OSCCON = 0b01110010;
    ANSELA = 0x00;  //AN0-3 as digital I/O
    ANSELC = 0x0e;  //RC1,2,3 as ADC.
    TRISA = 0x04;   //RA2 is Input
    TRISC = 0x0e;   // RC1,2,3 as input. others are output.
    PORTA = 0x00;   //initial out is LOW
    PORTC = 0x00;
    
    //Analog
    ADCON1 = 0b11010011;    //use FVR
    ADCON2 = 0b00000000;
    FVRCON = 0b11110011;    //4xVfvr
    
    //USART TX:RC0 RX : RA2
    RC0PPS = 0b10100;   //RC0 as TX
    RXPPS = 0b00010;    //RA2 as RX
    TXSTA = 0b00100100; //非同期, no-parity, 8bit
    RCSTA = 0b10010000; //受信情報設定
    SPBRG = 51;  //baudrate = 9600
    
    RCIF = 0; //interrupt by serial RX
    RCIE = 1; //enable the interrupt by Serial RX
    PEIE = 1; //周辺機器割り込みを許可
    GIE = 1; //全割り込みを許可
    
    LED = LOW;
    
    //calculation threshold
    for(int i = 0; i < 20; i++)
    {
        int v = adc('f');
        threshold += v;
    }
    threshold = threshold/20.0*0.95;
}


void main(void) {
    int signal = 0;
    int signalCount = 0;
    int noSignalCount = 0;
    char noSignalFlag = 1;
    
    int xCounter = 0;
    int yCounter = 0;
    Initialize();
    Blink(2);
    
    while(1)
    {
        int value = adc('f');
        signal = value < threshold ? HIGH : LOW;
        LED = signal;
        __delay_ms(2);
        
        signalCount += signal == HIGH ? 1 : 0;
        noSignalCount += signal == LOW ? 1 : 0;
        
        //end of signal
        if(noSignalCount > NOSIGNALCNT && noSignalFlag == 0)
        {
            SendChar(10);   // \n
            noSignalFlag = 1;   //大量の改行防止flag
            noSignalCount = 0;
            signalCount = 0;
        }
        
        //get signal
        if(signal == LOW && signalCount >= DASHCNT) // dash signal
        {
            noSignalFlag = 0;
            SendChar('-');
            signalCount = 0;
            noSignalCount = 0;
        }
        else if(signal == LOW && signalCount < DASHCNT && 0 < signalCount) // dot signal
        {
            noSignalFlag = 0;
            SendChar('.');
            signalCount = 0;
            noSignalCount = 0;
        }
        
        int x = adc('x');
        int y = adc('y');
        if(x > 1000 && xCounter == 0)
        {
            SendChar('p');
            xCounter = JOYCNT;
            Blink(1);
        }
        else if(x < 500 && xCounter == 0)
        {
            SendChar('-');  SendChar('x');
            xCounter = JOYCNT;
            Blink(1);
        }
        else if(x > 800 && xCounter == 0)
        {
            SendChar('+');  SendChar('x');
            xCounter = JOYCNT;
            Blink(1);
        }
        else if(y < 500 && yCounter == 0)
        {
            SendChar('-');  SendChar('y');
            yCounter = JOYCNT;
            Blink(1);
        }
        else if(y > 800 && yCounter == 0)
        {
            SendChar('+');  SendChar('y');
            yCounter = JOYCNT;
            Blink(1);
        }
        
        xCounter = xCounter > 0 ? xCounter-1 : xCounter;
        yCounter = yCounter > 0 ? yCounter-1 : yCounter;
    }
    return;
}
