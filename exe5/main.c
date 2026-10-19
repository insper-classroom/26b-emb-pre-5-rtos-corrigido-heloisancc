/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "hardware/timer.h"
#include "hardware/irq.h"

const int BTN_PIN_R = 28;
const int BTN_PIN_Y = 21;

const int LED_PIN_R = 5;
const int LED_PIN_Y = 10;

QueueHandle_t xQueueBtn;
SemaphoreHandle_t xSemaphoreLedR;
SemaphoreHandle_t xSemaphoreLedY;

void btn_callback(uint gpio, uint32_t events) {
    if (events == 0x4) { // fall edge
        int pin = (int) gpio;
        xQueueSendFromISR(xQueueBtn, &pin, 0);
    }
}

void btn_task(void* p) {
    gpio_init(BTN_PIN_R);
    gpio_set_dir(BTN_PIN_R, GPIO_IN);
    gpio_pull_up(BTN_PIN_R);

    gpio_init(BTN_PIN_Y);
    gpio_set_dir(BTN_PIN_Y, GPIO_IN);
    gpio_pull_up(BTN_PIN_Y);

    gpio_set_irq_enabled_with_callback(BTN_PIN_R, GPIO_IRQ_EDGE_FALL, true,
                                       &btn_callback);
    gpio_set_irq_enabled(BTN_PIN_Y, GPIO_IRQ_EDGE_FALL, true);

    int pin = 0;
    while (true) {
        if (xQueueReceive(xQueueBtn, &pin, portMAX_DELAY) == pdTRUE) {
            if (pin == BTN_PIN_R) {
                xSemaphoreGive(xSemaphoreLedR);
            } else if (pin == BTN_PIN_Y) {
                xSemaphoreGive(xSemaphoreLedY);
            }
        }
    }
}

void led_r_task(void* p) {
    gpio_init(LED_PIN_R);
    gpio_set_dir(LED_PIN_R, GPIO_OUT);
    gpio_put(LED_PIN_R, 0);

    int piscando = 0;
    int led = 0;

    while (true) {
        if (!piscando) {
            // parado: dorme até o botão ser apertado
            if (xSemaphoreTake(xSemaphoreLedR, portMAX_DELAY) == pdTRUE) {
                piscando = 1;
            }
        } else {
            led = !led;
            gpio_put(LED_PIN_R, led);
            // espera 100 ms; se o botão for apertado nesse meio, para
            if (xSemaphoreTake(xSemaphoreLedR, pdMS_TO_TICKS(100)) == pdTRUE) {
                piscando = 0;
                led = 0;
                gpio_put(LED_PIN_R, 0);
            }
        }
    }
}

void led_y_task(void* p) {
    gpio_init(LED_PIN_Y);
    gpio_set_dir(LED_PIN_Y, GPIO_OUT);
    gpio_put(LED_PIN_Y, 0);

    int piscando = 0;
    int led = 0;

    while (true) {
        if (!piscando) {
            if (xSemaphoreTake(xSemaphoreLedY, portMAX_DELAY) == pdTRUE) {
                piscando = 1;
            }
        } else {
            led = !led;
            gpio_put(LED_PIN_Y, led);
            if (xSemaphoreTake(xSemaphoreLedY, pdMS_TO_TICKS(100)) == pdTRUE) {
                piscando = 0;
                led = 0;
                gpio_put(LED_PIN_Y, 0);
            }
        }
    }
}

int main() {
    stdio_init_all();

    xQueueBtn = xQueueCreate(32, sizeof(int));
    xSemaphoreLedR = xSemaphoreCreateBinary();
    xSemaphoreLedY = xSemaphoreCreateBinary();

    xTaskCreate(btn_task, "BTN_Task 1", 256, NULL, 1, NULL);
    xTaskCreate(led_r_task, "LED_Task R", 256, NULL, 1, NULL);
    xTaskCreate(led_y_task, "LED_Task Y", 256, NULL, 1, NULL);

    vTaskStartScheduler();

    while(1){}

    return 0;
}