#include "main.h"
#include "tm1637.h"

volatile uint32_t tickCount;
uint32_t last_display_update;
uint16_t counter;
char lastKey;
uint32_t lastScanTime;

// задаем настройки замка
#define ACCESS_CODE 1234U // пароль 
#define CODE_LENGTH 4U    //цифр в коде

#define GREEN_LED_PIN 9U  // режим: доступ разрешен
#define RED_LED_PIN 15U   // режим: доступ запрещён

#define RESULT_HOLD_MS 1500U

// состояние замка
static uint32_t enteredCode; // то, что ввел мой пользователь
static uint8_t  digitCount; // сколько цифр всего введено

// вызывает функцию каждую миллисекунду => счетчик времени
void osSystickHandler(void) {
  tickCount++;
}


void initGPIO() {
  // Включаем тактирование GPIOA и GPIOB (General Purpose Input/Outpu)
  // «прочитать текущее значение, добавить к нему наши биты, записать обратно»
  RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_GPIOBEN;

  // Настраиваем PA5 как выход
  GPIOA->MODER = (GPIOA->MODER & ~(3 << 10)) | (1 << 10);
  GPIOA->OTYPER &= ~(1 << 5);
  GPIOA->OSPEEDR |= (1 << 10);

  // PA9 (зелёный)
  GPIOA->MODER &= ~(3U << (GREEN_LED_PIN * 2));
  GPIOA->MODER |=  (1U << (GREEN_LED_PIN * 2));
  GPIOA->OTYPER &= ~(1U << GREEN_LED_PIN);

  // PA15 (красный)
  GPIOA->MODER &= ~(3U << (RED_LED_PIN * 2));
  GPIOA->MODER |=  (1U << (RED_LED_PIN * 2));
  GPIOA->OTYPER &= ~(1U << RED_LED_PIN);

  // чтобы при старте они не гореди - нужно сбросить все в 0
  GPIOA->BRR = (1U << GREEN_LED_PIN) | (1U << RED_LED_PIN);
}

void initUSART2() {
  // Включаем тактирование USART2
  RCC->APB1ENR |= RCC_APB1ENR_USART2EN;

  // Настраиваем PA2 и PA3 в альтернативный режим
  GPIOA->MODER = (GPIOA->MODER & ~(0xF << 4)) | (0xA << 4);
  GPIOA->AFR[0] = (GPIOA->AFR[0] & ~(0xFF << 8)) | (1 << 8) | (1 << 12);

  // Настраиваем USART2
  USART2->BRR = 417; // 48MHz/115200
  USART2->CR1 = USART_CR1_TE | USART_CR1_UE;
}

void initSysTick() {
  // сколько тактов считать до прерывания
  SysTick->LOAD = 47999; // 1ms при 48MHz
  SysTick->VAL = 0;
  SysTick->CTRL = (1 << 2) | (1 << 1) | (1 << 0);
}

int _write(int file, uint8_t *ptr, int len) {
  for (int i = 0; i < len; i++) {
    while (!(USART2->ISR & USART_ISR_TXE));
    USART2->TDR = ptr[i];
  }
  return len;
}

// включить/выключить зеленый светодиод
static void led_green(int on) {
  if (on) {
    GPIOA->BSRR = (1U << GREEN_LED_PIN);
  } else {
    // если запишем 1 в 25-й бит (16+9) - сбрасываем РА9 в 0 (выкл светодиод)
    GPIOA->BSRR = (1U << (GREEN_LED_PIN + 16));
  }
}

// включить/выключить красный светодиод
static void led_red(int on) {
  if (on) {
    GPIOA->BSRR = (1U << RED_LED_PIN);
  } else {
    GPIOA->BSRR = (1U << (RED_LED_PIN + 16));
  }
}

// сбрасываем все для замка: накопитель кода, счетчик цифр, 0 на дисплее, погасить светодиоды
static void reset_input(void) {
  enteredCode = 0;
  digitCount  = 0;
  tm1637_display_number(0);
  led_green(0);
  led_red(0);
}

static void process_key(char key) {
  if (key >= '0' && key <= '9') {
    if (digitCount < CODE_LENGTH) {
      enteredCode = enteredCode * 10U + (uint32_t)(key - '0');
      digitCount ++;
      tm1637_display_number((int)enteredCode);
      printf("Digit: %c | Code: %lu | Count: %u\n",
             key, (unsigned long)enteredCode, digitCount);
    } else {
      printf("Code if ready, press # or *\n");
    }
    return; 
  }

  if (key == '*') {
    printf("Reset input\n");
    reset_input();
    return;
  }

  if (key == '#') {
    if (digitCount < CODE_LENGTH) {
      printf("Code incomplete: entered %u digits, need %u\n",
         digitCount, CODE_LENGTH);
      return; 
    }
    if (enteredCode == ACCESS_CODE) {
      printf("ACCESS!\n");
      led_red(0);
      led_green(1);
    } else {
      printf("FALSE CODE!\n");
      led_green(0);
      led_red(1);
    }

    uint32_t start = tickCount;
    while ((tickCount - start) < RESULT_HOLD_MS) { }

    reset_input();
    return;
  }
}


int main(void) {
  initGPIO();
  initUSART2();
  initSysTick();
  initKeyboard();
  tm1637_init();

  printf("Hello in my Project!", "Wokwi Simulation");

  reset_input();

  while (1) {
    char key = scanKeyboard();
    if (key != '\0') {
      printf("Key pressed: %c\n", key);
      process_key(key);
    }
  }
  return 0;
}