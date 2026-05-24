#ifndef __USART_H
#define __USART_H

#include <stdio.h>
void Usart_Init(void);
void Usart_TransmitChar(uint8_t ch);
//uint8_t Usart_ReceiveChar(void);
void Usart_TransmitString(uint8_t *str, uint8_t size);
//void Usart_ReceiveString(uint8_t buffer[], uint8_t* size);

#endif
