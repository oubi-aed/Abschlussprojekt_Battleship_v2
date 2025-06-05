#include <stm32f0xx.h>
#include <string.h>
#include "clock_.h"
#include "fifo.h"
#define LOG( msg... ) printf( msg );


// Select the Baudrate for the UART
#define BAUDRATE 115200 // Baud rate set to 115200 baud per second

#define MESSAGE_SIZE 64 // Size of the message buffer

const char USER_NAME[] = "Alexander"; // Name of the user, can be used for identification

volatile Fifo_t usart_rx_fifo;

const uint8_t USART2_RX_PIN = 3; // PA3 is used as USART2_RX
const uint8_t USART2_TX_PIN = 2; // PA2 is used as USART2_TX

int first_battlefield[10*10] = {
  2, 2, 0, 3, 3, 3, 0, 0, 0, 0, //Row 0
  0, 0, 0, 0, 0, 0, 0, 0, 0, 3,
  0, 2, 2, 0, 2, 2, 0, 0, 0, 3,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 3,
  0, 5, 0, 0, 4, 4, 4, 4, 0, 0,
  0, 5, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 5, 0, 4, 4, 4, 4, 0, 0, 0,
  0, 5, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 5, 0, 2, 2, 0, 3, 3, 3, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

//overrides _write so we can use printf
// The printf function is supported through the custom _write() function,
// which redirects output to UART (USART2)
int _write(int handle, char *data, int size)
{
    int count = size;
    while (count--)
    {
        while (!(USART2->ISR & USART_ISR_TXE))
        {
        };
        USART2->TDR = *data++;
    }
    return size;
}

void response(char question[])
{
  if(strncmp (question, "HD_STA", 6) ==0) //Start
  {
    LOG("DH_START_%s\n", USER_NAME); // Respond with the user's name
  }





}



void USART2_IRQHandler(void)
{
  static int ret; // You can do some error checking
  if (USART2->ISR & USART_ISR_RXNE)
  {                                              // Check if RXNE flag is set (data received)
    uint8_t c = USART2->RDR;                     // Read received byte from RDR (this automatically clears the RXNE flag)
    ret = fifo_put((Fifo_t *)&usart_rx_fifo, c); // Put incoming Data into the FIFO Buffer for later handling
  }

  USART2->ICR = 0xffffffff;
}


int main(void)
{
  SystemClock_Config(); // Configure the system clock to 48 MHz

  RCC->AHBENR |= RCC_AHBENR_GPIOAEN;    // Enable GPIOA clock
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN; // Enable USART2 clock

  // Configure PA2 and PA3 for USART2 (TX and RX)
  GPIOA->MODER |= 0b10 << (USART2_TX_PIN * 2);    // Set PA2 to Alternate Function mode
  GPIOA->AFR[0] |= 0b0001 << (4 * USART2_TX_PIN); // Set AF for PA2 (USART2_TX)
  GPIOA->MODER |= 0b10 << (USART2_RX_PIN * 2);    // Set PA3 to Alternate Function mode
  GPIOA->AFR[0] |= 0b0001 << (4 * USART2_RX_PIN); // Set AF for PA3 (USART2_RX)

  // Configure USART2
  USART2->BRR = (APB_FREQ / BAUDRATE); // Set baud rate (requires APB_FREQ to be defined)
  USART2->CR1 |= 0b1 << 2;             // Enable receiver (RE bit)
  USART2->CR1 |= 0b1 << 3;             // Enable transmitter (TE bit)
  USART2->CR1 |= 0b1 << 0;             // Enable USART (UE bit)
  USART2->CR1 |= 0b1 << 5;             // Enable RXNE interrupt (RXNEIE bit)

  // Configure NVIC for USART2 interrupt
  NVIC_SetPriorityGrouping(0);                               // Use 4 bits for priority, 0 bits for subpriority
  uint32_t uart_pri_encoding = NVIC_EncodePriority(0, 1, 0); // Encode priority: group 1, subpriority 0
  NVIC_SetPriority(USART2_IRQn, uart_pri_encoding);          // Set USART2 interrupt priority
  NVIC_EnableIRQ(USART2_IRQn);                               // Enable USART2 interrupt


  fifo_init((Fifo_t *)&usart_rx_fifo);                       // Init the FIFO

  uint32_t bytes_recv = 0;
  char message[MESSAGE_SIZE]; // Buffer to store the received message

  for (;;)
  { // Infinite loop

    
    uint8_t byte;
    if (fifo_get((Fifo_t *)&usart_rx_fifo, &byte) == 0)
    {
      if (bytes_recv < MESSAGE_SIZE){

        message[bytes_recv] = byte; // Store the received byte in the message buffer
      
        if(message[bytes_recv] == 'H') // Check for end of message
        {
          response(message); // Call the response function with the received message

        }        
      }
      bytes_recv++; // count the Bytes Received by getting Data from the FIFO
    }
  }
}


