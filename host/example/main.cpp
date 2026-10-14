/**
 * Exemplo: teste de LED (LD3) no STM32 via UART.
 *
 * Contrato com o firmware (Core/Src/main.c + Bsp.c):
 *   Host -> Edge : 1 byte  '1' (liga LD3) | '0' (desliga LD3)   (pacote fixo de 1 byte)
 *   Edge -> Host : "[led] ligado\r\n" | "[led] desligado\r\n"   (sem ACK)
 *
 * Uso: serial_example [porta]
 *   Sem argumento, procura o ST-Link (VID 0483 / PID 374b).
 */
#include "SerialClient.hpp"

#include <chrono>
#include <condition_variable>
#include <deque>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

using namespace serial;
using namespace std::chrono_literals;

namespace {

constexpr const char* kStLinkVid = "0483";
constexpr const char* kStLinkPid = "374b";
constexpr uint32_t    kBaudRate  = 115200;            // = huart3.Init.BaudRate
constexpr auto        kResponseTimeout = 500ms;

/**
 * Observer que remonta os chunks crus do SO em linhas terminadas em '\n'.
 * Necessário porque a resposta do STM32 pode chegar fragmentada em vários
 * onDataReceived (ou colada com a próxima).
 */
class LineObserver : public ISerialObserver {
public:
    LineObserver(SerialClient* client) : m_client(client) {}

    // Executa na thread do AsyncSerialReceiver
    void onDataReceived(const std::vector<uint8_t>& data) override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (uint8_t byte : data) {
                if (byte == '\r') continue;
                if (byte == '\n') {
                    if (!m_partial.empty()) {
                        /* INJEÇÃO DE ACK: 
                           Quando recebemos exatamente "[btn]", disparamos o ACK imediatamente 
                           na thread de RX, antes de notificar a aplicação, para não deixar 
                           a BSP_UART_Send_WaitACK travar por timeout. */
                        if (m_partial == "[btn]") {
                            std::vector<uint8_t> ack = {0x06};
                            if (m_client) {
                                m_client->send(ack);
                                // Imprime de forma assíncrona para avisar o usuário
                                std::cout << "\n[Background] Mensagem '[btn]' recebida do STM32. Enviando ACK (0x06) instantaneamente!\n> " << std::flush;
                            }
                            // DESCARTA a mensagem para não poluir a fila da thread principal
                            m_partial.clear();
                            continue;
                        }

                        m_lines.push_back(std::move(m_partial));
                        m_partial.clear();
                    }
                    continue;
                }
                if (m_partial.size() < kMaxLineLength) {
                    m_partial.push_back(static_cast<char>(byte));
                }
            }
        }
        m_cv.notify_all();
    }

    void onError(const std::string& errorMessage) override {
        std::cerr << "[serial] erro: " << errorMessage << std::endl;
    }

    void onConnectionClosed() override {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_closed = true;
        }
        m_cv.notify_all();
    }

    /** Bloqueia até uma linha completa chegar, a conexão cair ou o timeout expirar. */
    std::optional<std::string> waitLine(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_cv.wait_for(lock, timeout, [this] { return !m_lines.empty() || m_closed; });
        if (m_lines.empty()) {
            return std::nullopt;
        }
        std::string line = std::move(m_lines.front());
        m_lines.pop_front();
        return line;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lines.clear();
        m_partial.clear();
    }

    bool closed() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_closed;
    }

private:
    static constexpr size_t kMaxLineLength = 256;

    SerialClient*           m_client;
    mutable std::mutex      m_mutex;
    std::condition_variable m_cv;
    std::deque<std::string> m_lines;
    std::string             m_partial;
    bool                    m_closed = false;
};

/** Envia '1' ou '0' e valida a resposta esperada do firmware. */
bool sendLedCommand(SerialClient& client, LineObserver& lines, char cmd) {
    const std::string expected = (cmd == '1') ? "[led] ligado" : "[led] desligado";

    if (!client.send(std::string(1, cmd))) {
        std::cerr << "  falha ao enviar '" << cmd << "'" << std::endl;
        return false;
    }

    const auto deadline = std::chrono::steady_clock::now() + kResponseTimeout;
    while (true) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        if (remaining <= 0ms) break;

        auto line = lines.waitLine(remaining);
        if (!line) break;

        if (*line == expected) {
            std::cout << "  STM32: " << *line << "  [OK]" << std::endl;
            return true;
        }
        std::cout << "  STM32 (inesperado): " << *line << std::endl;
    }

    std::cout << "  timeout: sem resposta \"" << expected << "\" em "
              << kResponseTimeout.count() << " ms" << std::endl;
    return false;
}

std::optional<std::string> selectPort(int argc, char* argv[]) {
    if (argc > 1) {
        return std::string(argv[1]);
    }

    if (auto st = PortEnumerator::findByVidPid(kStLinkVid, kStLinkPid)) {
        std::cout << "ST-Link encontrado: " << st->portPath
                  << " (" << st->description << ")" << std::endl;
        return st->portPath;
    }

    std::cout << "ST-Link (" << kStLinkVid << ":" << kStLinkPid << ") nao encontrado.\n"
              << "Portas disponiveis:" << std::endl;
    for (const auto& p : PortEnumerator::listPorts()) {
        std::cout << "  " << p.portPath << "  VID:" << p.vendorId << " PID:" << p.productId
                  << "  " << p.description << std::endl;
    }
    std::cout << "Informe a porta manualmente: serial_example /dev/ttyACMx" << std::endl;
    return std::nullopt;
}

void printHelp() {
    std::cout << "\nComandos:\n"
                 "  1  -> liga LD3\n"
                 "  0  -> desliga LD3\n"
                 "  b  -> pisca LD3 5x\n"
                 "  q  -> sair\n" << std::endl;
}

} // namespace

int main(int argc, char* argv[]) {
    auto port = selectPort(argc, argv);
    if (!port) {
        return 1;
    }

    SerialClient client;
    // O observer agora recebe ponteiro para o client para enviar o ACK instantaneamente
    LineObserver lines(&client);

    std::cout << "Conectando em " << *port << " @ " << kBaudRate << " 8N1..." << std::endl;
    if (!client.connect(*port, kBaudRate)) {
        std::cerr << "Falha ao abrir " << *port << std::endl;
        return 1;
    }

    // addObserver só tem efeito APÓS connect() (o receiver é criado no connect).
    client.addObserver(&lines);
    std::this_thread::sleep_for(50ms);
    lines.clear();                    // descarta lixo residual da porta
    (void)client.readBufferedMessages();

    printHelp();

    std::string input;
    while (!lines.closed()) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input)) break;   // EOF
        if (input.empty()) continue;

        const char cmd = input[0];
        if (cmd == 'q') {
            break;
        } else if (cmd == '1' || cmd == '0') {
            sendLedCommand(client, lines, cmd);
        } else if (cmd == 'b') {
            int ok = 0;
            for (int i = 0; i < 5; ++i) {
                ok += sendLedCommand(client, lines, '1');
                std::this_thread::sleep_for(250ms);
                ok += sendLedCommand(client, lines, '0');
                std::this_thread::sleep_for(250ms);
            }
            std::cout << "  pisca: " << ok << "/10 respostas validas" << std::endl;
        } else {
            printHelp();
        }

        // O MessageBufferObserver interno acumula todos os chunks sem limite;
        // como usamos o LineObserver, apenas descartamos.
        (void)client.readBufferedMessages();
    }

    if (lines.closed()) {
        std::cerr << "Conexao encerrada pelo dispositivo." << std::endl;
    }

    client.disconnect();
    std::cout << "Desconectado." << std::endl;
    return 0;
}
