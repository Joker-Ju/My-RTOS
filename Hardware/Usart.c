#include "stm32f10x.h"
#include <stdio.h>

uint8_t buffer[100] = {0};
uint8_t size = 0;
uint8_t Usart1Over = 0; //接收完成标志位

void Usart_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; //使能GPIOA时钟
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN; //使能USART1时钟

    //初始化GPIOA的PA9为复用推挽输出（USART1_TX）
    GPIOA->CRH |=GPIO_CRH_CNF9_1; //复用推挽输出
    GPIOA->CRH &= ~GPIO_CRH_CNF9_0; //复用推挽输出
    GPIOA->CRH |= GPIO_CRH_MODE9; //输出模式，最大速度50MHz
    //初始化GPIOA的PA10为浮空输入（USART1_RX）
    GPIOA->CRH &= ~GPIO_CRH_CNF10_1; //浮空输入
    GPIOA->CRH |= GPIO_CRH_CNF10_0; //浮空输入
    GPIOA->CRH &= ~GPIO_CRH_MODE10; //输入模式

    //初始化USART1
    //设置波特率
    USART1->BRR = 72000000 / 115200; //设置波特率为115200
    //使能USART1，发送和接收
    USART1->CR1 |= (USART_CR1_TE | USART_CR1_UE|USART_CR1_RE); //使能发送、接收和USART1
    
    //其他配置，数据帧格式
    USART1->CR1 &= ~USART_CR1_M; //设置为8数据位
    USART1->CR1 &= ~USART_CR1_PCE; //禁用奇偶校验
    USART1->CR2 &= ~USART_CR2_STOP; //设置为1停止位


   //开启使能中断
    USART1->CR1 |= USART_CR1_RXNEIE; //使能接收中断
    /*中断接收时由于开了发送中断导致程序出现问题*/
    // USART1->CR1 |= USART_CR1_TXEIE; //使能发送中断
    USART1->CR1 |= USART_CR1_IDLEIE; //使能空闲线检测中断

   //配置NVIC
   NVIC_SetPriorityGrouping(3); //设置优先级分组为3
   NVIC_SetPriority(USART1_IRQn, 3); //设置USART1中断优先级
   NVIC_EnableIRQ(USART1_IRQn); //使能USART1中断

    

}

void Usart_TransmitChar(uint8_t ch)
{
    while (!(USART1->SR & USART_SR_TXE)){}; //等待发送缓冲区空
    USART1->DR = ch; //发送数据
}

//uint8_t Usart_ReceiveChar(void)
//{
//    while (!(USART1->SR & USART_SR_RXNE)) {}//等待接收缓冲区非空
//    return USART1->DR; //读取接收到的数据
//}



void Usart_TransmitString(uint8_t *str, uint8_t size)
{
    for (uint8_t i = 0; i < size; i++)
    {
        Usart_TransmitChar(str[i]); //发送字符串中的每个字符
    }
}

//  void Usart_ReceiveString(uint8_t buffer[], uint8_t *size)
//  {
//      uint16_t i = 0;
//      while(1)
//      {
//          while (!(USART1->SR & USART_SR_RXNE)) //检查接收缓冲区是否有数据
//          {
//              if ((USART1->SR & USART_SR_IDLE)) //检查空闲线检测标志位，表示接收完成
//              {
//                  *size = i; //返回接收到的字符串长度
//                  //读DR寄存器以清除空闲线检测标志位
//                  USART1->DR;
//                  return; //退出函数
//              }
//          }
//          buffer[i] = USART1->DR; //存储接收到的字符
//          i++;       
//      }
//  }

//printf函数重定向
int fputc(int ch, FILE *f)
{
    Usart_TransmitChar((uint8_t)ch); //通过USART发送字符
    return ch; //返回发送的字符
}

//void USART1_IRQHandler(void)
//{
//    if (USART1->SR & USART_SR_RXNE) //检查接收中断标志位
//    {
//        uint8_t byte = USART1->DR; //读取接收到的数据并存储在buffer中，size为接收到的字符串长度
//    }
//    else if (USART1->SR & USART_SR_IDLE) //检查空闲线检测标志位
//    {
//        //读DR寄存器以清除空闲线检测标志位
//        USART1->DR;
//		//发回电脑显示
//		Usart_TransmitString(buffer, size); //发送接收到的字符串
// 	   //置标志位到主函数去处理
//		Usart1Over = 1;
//         // size = 0;
//    }
//}

