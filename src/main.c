#include <stm32f0xx.h>
#include <string.h>
#include <stdbool.h>
#include "clock_.h"
#include "fifo.h"
#define LOG(msg...) printf(msg);

// Select the Baudrate for the UART
#define BAUDRATE 115200 // Baud rate set to 115200 baud per second

#define MESSAGE_SIZE 64                      // Size of the message buffer
#define COL_FIELD 10                         // Number of columns in the battlefield
#define ROWS_FIELD 10                        // Number of rows in the battlefield
#define ACTIVE_BATTLEFIELD first_battlefield // Pointer to the active battlefield array
#define SUM_SHIP_PARTS 29                    // summ of all shipparts

const char USER_NAME[] = "Alexander"; // Name of the user, can be used for identification

volatile Fifo_t usart_rx_fifo;

const uint8_t USART2_RX_PIN = 3; // PA3 is used as USART2_RX
const uint8_t USART2_TX_PIN = 2; // PA2 is used as USART2_TX

// states for state machines
int state = 0;
int state_strategy = 0;
int counter_direction = 0;
int old_state_strategy = 0; // saves old state of state_strategy so we can return to the selected strategy

char message[MESSAGE_SIZE] = {0};     // Buffer to store the received message
int my_checksum[COL_FIELD] = {0};     // Initialize my_checksum array
int attacker_my_checksum[ROWS_FIELD]; // Buffer for chechsum host
int my_checksum_control = 0;          // summ to check if it's 30
int x_attack = 0;
int x_attack_old = 0;
int y_attack = 0;
int y_attack_old = 0;
int x_defense = 0;
int y_defense = 0;
bool next_time_surround = false;

int my_ship_parts = SUM_SHIP_PARTS;
int attacker_ship_parts = SUM_SHIP_PARTS;

int first_battlefield[COL_FIELD * ROWS_FIELD] = {
    2, 2, 0, 3, 3, 3, 0, 0, 0, 0, // Row 0 //
    0, 0, 0, 0, 0, 0, 0, 0, 0, 3,
    0, 2, 2, 0, 2, 2, 0, 0, 0, 3,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 3,
    0, 5, 0, 0, 4, 4, 4, 4, 0, 0,
    0, 5, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 5, 0, 4, 4, 4, 4, 0, 0, 0,
    0, 5, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 5, 0, 2, 2, 0, 3, 3, 3, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // controlsum is: 5, 1, 5, 1, 5, 1, 5, 1, 6, 0

// best strategy: https://www.youtube.com/watch?v=q4aWxlGOV4w
int attack_best_strategy[COL_FIELD * ROWS_FIELD] = {
    0, 0, 0, 1, 0, 0, 0, 1, 0, 0,
    0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1,
    1, 0, 0, 0, 1, 0, 0, 0, 1, 0,
    0, 0, 0, 1, 0, 0, 0, 1, 0, 0,
    0, 0, 1, 0, 0, 0, 1, 0, 0, 0,
    0, 1, 0, 0, 0, 1, 0, 0, 0, 1,
    1, 0, 0, 0, 1, 0, 0, 0, 1, 0,
    0, 0, 0, 1, 0, 0, 0, 1, 0, 0,
    0, 0, 1, 0, 0, 0, 1, 0, 0, 0};

int attack_each[COL_FIELD * ROWS_FIELD] = {
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1};

// overrides _write so we can use printf
//  The printf function is supported through the custom _write() function,
//  which redirects output to UART (USART2)
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

int fieldController(int x, int y, int field[])
{
  int pos_number = 0;
  pos_number = field[x * COL_FIELD + y];

  return pos_number;
}

void attack_strategy(int *x, int *y, int field[])
{
  for (int row = 0; row < ROWS_FIELD; row++)
  { // Loop through each row
    for (int col = 0; col < COL_FIELD; col++)
    { // Loop through each collumn
      if (field[row * COL_FIELD + col] > 0)
      {
        *y = col;
        *x = row;
        field[row * COL_FIELD + col] = 0;
        return;
      }
    }
  }
}

void attack_surround(void)
{
  // attack surroundings
  switch (counter_direction)
  {
  case 0:                                // attacks right

    if (y_attack+1 < COL_FIELD)
    {   
      LOG("DH_BOOM_%d_%d\n", x_attack, y_attack+1);
      counter_direction = 1;
      break;
    }
    else
    {
      
      counter_direction = 1;
    }
    
  case 1:                                // attacks down
    if (x_attack+1 < COL_FIELD)
    {
      LOG("DH_BOOM_%d_%d\n", x_attack+1, y_attack);
      counter_direction = 2;
      break;
    }
    else
    { 
      counter_direction = 2;
    }
    
  case 2:                                // attacks left
    if (y_attack-1 >= 0)
    {
      LOG("DH_BOOM_%d_%d\n", x_attack, y_attack-1);
      counter_direction = 3;
      break;
    }
    else
    {
      
      counter_direction = 3;
    }
    
  case 3:                                // attacks up
    if (x_attack-1 >= 0)
    {
      LOG("DH_BOOM_%d_%d\n", x_attack-1, y_attack);
      state_strategy = old_state_strategy;
      x_attack_old = x_attack;
      y_attack_old = y_attack;
      counter_direction = 0;
    
    }
    else
    {

      LOG("DH_BOOM_0_0\n");    
      state_strategy = old_state_strategy;
      x_attack_old = x_attack;
      y_attack_old = y_attack;
      counter_direction = 0;
    }
    break;
    
  }
}

void field_plot(int field[])
{
  for (int row = 0; row < ROWS_FIELD; row++)
  {
    LOG("DH_SF%dD", row); // Loop through each row
    for (int col = 0; col < COL_FIELD; col++)
    { // Loop through each collumn
      LOG("%d", field[row * COL_FIELD + col]);
    }
    LOG("\n");
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

void config_hardware(void)
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
}

void reset_all(void)
{

  memset(my_checksum, 0, sizeof(my_checksum));
  memset(attacker_my_checksum, 0, sizeof(attacker_my_checksum));

  my_checksum_control = 0; // summ to check if it's 30
  x_attack = 0;
  y_attack = 0;
  x_defense = 0;
  y_defense = 0;
  my_ship_parts = SUM_SHIP_PARTS;
  attacker_ship_parts = SUM_SHIP_PARTS;
}

int main(void)
{

  config_hardware(); // Function to configure the hardware

  fifo_init((Fifo_t *)&usart_rx_fifo); // Init the FIFO

  for (;;)
  { // Infinite loop

    uint8_t byte = 0;

    int msg_pos = 0;
    do
    {
      if (fifo_get((Fifo_t *)&usart_rx_fifo, &byte) == 0)
      {
        if (byte == '\n')
          break;
        message[msg_pos] = (char)byte;
        msg_pos++;
      }

      if (msg_pos >= MESSAGE_SIZE)
      {
        // buffer overflow
        msg_pos = 0;
      }
    } while (1);

    switch (state)
    {
    case 0: // Start

      reset_all();

      if (strncmp(message, "HD_START", 8) == 0)
      {

        LOG("DH_START_%s\n", USER_NAME);     // Respond with the user's name
        memset(message, 0, sizeof(message)); // Reset the message buffer
        msg_pos = 0;
        state = 1;
      }

      break;
    case 1: // HD_Start

      if (strncmp(message, "HD_CS_", 6) == 0)
      {
        // store my_checksum from enemy
        // for (int i = 0; i < ROWS_FIELD; i++){
        //   attacker_my_checksum[i] = message[7+i];
        // controlling my_checksum
        // my_checksum_control += attacker_my_checksum[i];
        //}
        // if (my_checksum_control != SUM_SHIP_PARTS){
        // printf("Ceating! my_checksum must be %d", SUM_SHIP_PARTS);
        //}

        // send my_checksum
        LOG("DH_CS_");
        for (int row = 0; row < ROWS_FIELD; row++)
        { // splitting the Rows from the Columns{
          for (int col = 0; col < COL_FIELD; col++)
          { // summ for each Row
            if (ACTIVE_BATTLEFIELD[row * COL_FIELD + col] > 0)
            {
              my_checksum[row]++;
            }
          }
          LOG("%d", my_checksum[row]);
        }
        LOG("\n");
        memset(message, 0, sizeof(message)); // Reset the message buffer
        state = 2;
      }
      break;

    case 2: // HD_BOOM

      if (strncmp(message, "HD_BOOM_H", 9) == 0)
      {
        // attacker_my_checksum[y_attack] = attacker_my_checksum[y_attack] - 1;
        attacker_ship_parts--;
        if (x_attack_old != x_attack || y_attack_old != y_attack)
        {
          state_strategy = 9;
        }else{
          state_strategy = old_state_strategy;
        }

        memset(message, 0, sizeof(message)); // Reset the message buffer
      }

      if (strncmp(message, "HD_BOOM_M", 9) == 0)
      {
        memset(message, 0, sizeof(message)); // Reset the message buffer
      }

      if (strncmp(message, "HD_BOOM_", 8) == 0)
      {

        // damage control
        // umwandeln der werte
        x_defense = message[8] - '0';
        y_defense = message[10] - '0';
        int field = 0;
        field = fieldController(x_defense, y_defense, ACTIVE_BATTLEFIELD);

        if (field == 0)
        {
          LOG("DH_BOOM_M\n");
        }
        else
        {
          LOG("DH_BOOM_H\n");
          // my_checksum[y_defense] = my_checksum[y_defense] - 1;
          my_ship_parts--;
        }


        // attack
        switch (state_strategy)
        {
        case 0: // first attack strategy

          old_state_strategy = state_strategy;
          attack_strategy(&x_attack, &y_attack, attack_best_strategy);
          LOG("DH_BOOM_%d_%d\n", x_attack, y_attack);

          if (x_attack * COL_FIELD + y_attack > 95)
          {
            state_strategy = 1;
          }
          if ((my_ship_parts == 0) || (attacker_ship_parts == 0))
          {
            memset(message, 0, sizeof(message)); // Reset the message buffer
            state = 3;
          }
          break;

        case 1: // changing battlefield
          old_state_strategy = state_strategy;
          attack_strategy(&x_attack, &y_attack, attack_each);
          LOG("DH_BOOM_%d_%d\n", x_attack, y_attack);

          if (x_attack * COL_FIELD + y_attack > 95)
          {
          }
          if ((my_ship_parts == 0) || (attacker_ship_parts == 0))
          {
            memset(message, 0, sizeof(message)); // Reset the message buffer
            state = 3;
          }

          break;
        case 9:
          attack_surround();
          break;
        }

      case 3: // plot battlefield after end

        if ((my_ship_parts == 0) || (attacker_ship_parts == 0))
        {
          field_plot(ACTIVE_BATTLEFIELD);

          memset(message, 0, sizeof(message)); // Reset the message buffer
          state = 4;
        }
        break;

      case 4: // reset

        if (strncmp(message, "HD_START", 8) == 0)
        {
          reset_all();

          state = 0;
          state_strategy = 0;
        }
        break;
      }
    }
  }
}
