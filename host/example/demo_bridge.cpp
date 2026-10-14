#include "Stm32Bridge.hpp"

#include <iostream>
#include <thread>
#include <chrono>

using namespace std::chrono_literals;

int main() {
    std::cout << "====================================================\n";
    std::cout << "  STM32 Bridge - Exemplo de Handshake Bidirecional   \n";
    std::cout << "====================================================\n\n";

    std::cout << "Chamando conectar() sem parametros...\n";
    if (!bridge::conectar()) {
        std::cerr << "Falha ao conectar no STM32. Encerrando.\n";
        return 1;
    }

    std::cout << "\nMenu interativo:\n"
              << "  [1] Chamar enviar_objeto_encontrado()\n"
              << "  [0] Chamar enviar_sem_objeto()\n"
              << "  [a] Teste automatico (alterna 5x com intervalo)\n"
              << "  [q] Sair\n\n";

    std::string input;
    while (bridge::esta_conectado()) {
        std::cout << "\nbridge > " << std::flush;
        if (!std::getline(std::cin, input)) break;
        if (input.empty()) continue;

        char cmd = input[0];
        if (cmd == 'q') {
            break;
        } else if (cmd == '1') {
            std::cout << "--> Executando enviar_objeto_encontrado()...\n";
            bool ok = bridge::enviar_objeto_encontrado();
            std::cout << "Resultado: " << (ok ? "SUCESSO (Handshake OK)" : "FALHA") << "\n";
        } else if (cmd == '0') {
            std::cout << "--> Executando enviar_sem_objeto()...\n";
            bool ok = bridge::enviar_sem_objeto();
            std::cout << "Resultado: " << (ok ? "SUCESSO (Handshake OK)" : "FALHA") << "\n";
        } else if (cmd == 'a') {
            std::cout << "--> Iniciando ciclo automatico...\n";
            for (int i = 0; i < 5; ++i) {
                std::cout << "\n[Ciclo " << (i + 1) << "/5] Objeto Detectado:\n";
                bridge::enviar_objeto_encontrado();
                std::this_thread::sleep_for(500ms);

                std::cout << "\n[Ciclo " << (i + 1) << "/5] Sem Objeto:\n";
                bridge::enviar_sem_objeto();
                std::this_thread::sleep_for(500ms);
            }
        } else {
            std::cout << "Comando invalido. Use 1, 0, a ou q.\n";
        }
    }

    bridge::desconectar();
    std::cout << "Programa finalizado.\n";
    return 0;
}
