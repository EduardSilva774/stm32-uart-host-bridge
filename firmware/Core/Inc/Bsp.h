/**
 * @file    Bsp.h
 * @author  iona50
 * @date    10 de out. de 2026
 * @brief   Cabeçalho do Board Support Package para comunicação UART.
 *
 * Define os tipos, estruturas de estado e protótipos de funções necessários
 * para a comunicação serial UART, suportando modos bloqueantes (Polling)
 * e assíncronos (Interrupção), além de controle de fluxo via ACK.
 */

#ifndef INC_BSP_H_
#define INC_BSP_H_

#include "main.h"

/**
 * @brief Referência externa para a estrutura de controle da UART3.
 *
 * Variável instanciada pelo STM32CubeMX que contém a configuração de
 * hardware do periférico USART3.
 */
extern UART_HandleTypeDef huart3;

/**
 * @brief Tipo de ponteiro de função para o callback de recepção assíncrona.
 *
 * @param size Quantidade de bytes efetivamente recebidos e prontos no buffer.
 */
typedef void (* BSP_UART_Callback_t)(uint16_t size);

/**
 * @brief Enumeração dos estados operacionais da interface UART no BSP.
 *
 * Utilizada pela máquina de estados interna para gerenciar o fluxo de
 * dados e evitar colisões entre acessos imperativos e assíncronos ao hardware.
 */
typedef enum {
    BSP_UART_STATE_IDLE,                 /**< Nenhuma operação de recepção ativa no momento. */
    BSP_UART_STATE_RECEIVING_IT,         /**< Recepção assíncrona (background) por interrupção em andamento. */
    BSP_UART_STATE_RECEIVING_POLLING,    /**< Recepção síncrona bloqueante ativa, CPU dedicada à UART. */
    BSP_UART_STATE_WAITING_ACK           /**< Aguardando um byte específico de Acknowledge (ACK). */
} BSP_UART_State_t;

/* --- Protótipos das Funções de Recepção --- */
HAL_StatusTypeDef BSP_UART_StartRecieve_IT(uint8_t *pData, uint16_t Size, BSP_UART_Callback_t callback);
HAL_StatusTypeDef BSP_UART_Receive(uint8_t *pData, uint16_t Size, uint32_t Timeout);
void BSP_UART_StopReceive_IT(void);

/* --- Protótipos das Funções de Envio --- */
HAL_StatusTypeDef BSP_UART_Send(uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef BSP_UART_Send_WaitACK(uint8_t *pData, uint16_t Size, uint8_t ExpectedAck, uint32_t Timeout);

#endif /* INC_BSP_H_ */
