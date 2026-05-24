#include <stm32f10x.h>
#include "Led.h"

//操作LED0的函数实现
void LED0_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; // 使能 GPIOA 时钟
    // 初始化PA0
    GPIOA->CRL &= ~(GPIO_CRL_MODE0 | GPIO_CRL_CNF0); // 清除原配置
    GPIOA->CRL |= GPIO_CRL_MODE0; // 输出模式，最大速度 50MHz
    LED0_Off();
}

void LED0_On(void)
{
    GPIOA->ODR &= ~GPIO_ODR_ODR0; // 直接操作ODR寄存器，清零ODR0位
}

void LED0_Off(void)
{
    GPIOA->ODR |= GPIO_ODR_ODR0; // 直接操作ODR寄存器，置位ODR0位
}

void LED0_Toggle(void)
{
    if ((GPIOA->ODR & GPIO_ODR_ODR0) != 0)
    {
        GPIOA->ODR &= ~GPIO_ODR_ODR0; // 清零ODR0位
    }
    else
    {   
        GPIOA->ODR |= GPIO_ODR_ODR0; // 置位ODR0位
    }   
}

//操作LED1的函数实现
void LED1_Init(void)
{
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN; // 使能 GPIOA 时钟
    // 初始化PA1
    GPIOA->CRL &= ~(GPIO_CRL_MODE1 | GPIO_CRL_CNF1); // 清除原配置
    GPIOA->CRL |= GPIO_CRL_MODE1; // 输出模式，最大速度 50MHz

    LED1_Off();
}



void LED1_On(void)
{
    GPIOA->ODR &= ~GPIO_ODR_ODR1; // 直接操作ODR寄存器，清零ODR1位
}   

void LED1_Off(void)
{
    GPIOA->ODR |= GPIO_ODR_ODR1; // 直接操作ODR寄存器，置位ODR1位
}

void LED1_Toggle(void)
{
    if ((GPIOA->ODR & GPIO_ODR_ODR1) != 0)
    {
        GPIOA->ODR &= ~GPIO_ODR_ODR1; // 清零ODR1位
    }
    else
    {        
        GPIOA->ODR |= GPIO_ODR_ODR1; // 置位ODR1位
    }

}
