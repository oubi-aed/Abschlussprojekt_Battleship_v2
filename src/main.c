#include <stm32f0xx.h>
#include <string.h>
#include <stdbool.h>
#include "clock_.h"
#include "fifo.h"
#define LOG( msg... ) printf( msg );


// Select the Baudrate for the UART
#define BAUDRATE 115200 // Baud rate set to 115200 baud per second

#define MESSAGE_SIZE 20 // Size of the message buffer
#define COL_FIELD 10 // Number of columns in the battlefield
#define ROWS_FIELD 10 // Number of rows in the battlefield
#define active_battlefield first_battlefield // Pointer to the active battlefield array

const char USER_NAME[] = "Alexander"; // Name of the user, can be used for identification

volatile Fifo_t usart_rx_fifo;

const uint8_t USART2_RX_PIN = 3; // PA3 is used as USART2_RX
const uint8_t USART2_TX_PIN = 2; // PA2 is used as USART2_TX

int first_battlefield[COL_FIELD*ROWS_FIELD] = {
  2, 2, 0, 3, 3, 3, 0, 0, 0, 0, //Row 0 //
  0, 0, 0, 0, 0, 0, 0, 0, 0, 3,
  0, 2, 2, 0, 2, 2, 0, 0, 0, 3,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 3,
  0, 5, 0, 0, 4, 4, 4, 4, 0, 0,
  0, 5, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 5, 0, 4, 4, 4, 4, 0, 0, 0,
  0, 5, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 5, 0, 2, 2, 0, 3, 3, 3, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0
}; //controlsum is: 5, 1, 5, 1, 5, 1, 5, 1, 6, 0

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

void fieldController(int *x, int *y, int field[]){

  for (int row = 0; row < x; row++) { // Loop through each row
    for (int col = 0; col < y; col++) // Loop through each collumn
      LOG("%d\t", field[row * COL_FIELD + col]);
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

void config_hardware(void){
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
}

int main(void)
{

  config_hardware(); // Function to configure the hardware

  fifo_init((Fifo_t *)&usart_rx_fifo);                       // Init the FIFO



  uint32_t bytes_recv = 0;
  char message[MESSAGE_SIZE]; // Buffer to store the received message
  bool able_to_save = false; // Flag to indicate if the message is ready to be sent

  for (;;)
  { // Infinite loop

    
    uint8_t byte;
    if (fifo_get((Fifo_t *)&usart_rx_fifo, &byte) == 0){
      if (bytes_recv < MESSAGE_SIZE){
        if(byte == 'H'){
          able_to_save = true;
        }
        if(byte == '\n' || byte == '\r'){
          able_to_save = false;
        }
        if(able_to_save == true){
          message[bytes_recv] = byte;
          bytes_recv++; 
        }


        
        if(strncmp (message, "HD_START", 8) ==0){
          LOG("DH_START_%s\n", USER_NAME); // Respond with the user's name
          memset(message, 0, sizeof(message)); // Reset the message buffer
          bytes_recv = 0;
          able_to_save = false;
        }
        
        if(strncmp (message, "HD_CS", 5) ==0){
        LOG("DH_CS_");
        int checksum [COL_FIELD] = {0}; // Initialize checksum array
        for (int row = 0; row < ROWS_FIELD; row++){ // splitting the Rows from the Columns{
          for (int col = 0; col < COL_FIELD; col++){ //summ for each Row
            if (active_battlefield[row * COL_FIELD + col] != 0){
              checksum[row]++;
            }
          }
          LOG("%d", checksum[row]);
        }
        LOG("\n");
        memset(message, 0, sizeof(message)); // Reset the message buffer
        bytes_recv = 0;
        able_to_save = false;
        }

        if(strncmp (message, "HD_BOOM", 7) ==0){
          LOG("This is boom:%s\n",message); // Respond with the user's name
          memset(message, 0, sizeof(message)); // Reset the message buffer
          bytes_recv = 0;
          able_to_save = false;
        }
          
      } else if (bytes_recv >= MESSAGE_SIZE){
        LOG("error_overrun\n");
        memset(message, 0, sizeof(message)); // Reset the message buffer
        bytes_recv = 0;
      }
      
      
    }
  }
}
