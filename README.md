# 📟 Smart PDV

O **Smart PDV** é um sistema de ponto de venda embarcado de baixo custo. Ele transforma uma Raspberry Pi Pico W em um terminal de vendas completo com interface física, processamento de pagamentos simulados e integração total com a nuvem via protocolo MQTT.

## ❗ Problema Resolvido
Sistemas de caixa tradicionais costumam ser caros, dependentes de PCs e pouco flexíveis. Este projeto oferece:
*   **Baixo Custo:** Hardware acessível e eficiente.
*   **Portabilidade:** Sistema embarcado autônomo.
*   **Conectividade IoT:** Monitoramento de vendas em tempo real via **ThingsBoard**.

---

## 🚀 Funcionalidades
- ✅ **Menu Interativo:** Navegação fluida em display OLED SSD1306.
- ✅ **Controle Físico:** Navegação via Joystick e entrada de valores por Teclado Matricial 4x4.
- ✅ **Pagamentos:** Cálculo automático de troco e geração de **QR Code PIX** (padrão BR Code).
- ✅ **Feedback Multimodal:** Avisos sonoros (Buzzer) e visuais (LEDs RGB).
- ✅ **Nuvem:** Conexão Wi-Fi estável e telemetria via MQTT.

---

## 🕹️ Como Operar o Sistema
Após ligar o dispositivo e estabelecer a conexão Wi-Fi, o fluxo de operação segue estes passos:

1.  **Seleção de Produtos:** Utilize o **Joystick** para navegar entre os itens listados no display OLED. Pressione o botão B para adicionar itens ao carrinho e o botão A para tirar itens do carrinho.
2.  **Fechamento do Pedido:** Após selecionar os produtos, avance para a tela de pagamento navegando até o final do menu dos itens e clicando em ">>Prosseguir<<". Na tela de pagamento o sistema calculará o valor total automaticamente.
3.  **Escolha do Pagamento:**
    *   **Dinheiro:** Selecione a opção 'Dinheiro' com o botão A e insira o valor recebido utilizando o **Teclado Matricial**. O sistema exibirá o troco no display.
    *   **PIX:** Selecione a opção 'Pix' com o botão B e o sistema gerará um **QR Code dinâmico** no visor OLED para simulação de pagamento.
4.  **Finalização e IoT:** Ao confirmar a venda, os dados da transação (valor, itens, valor total) serão enviados automaticamente para o **ThingsBoard** via MQTT.

### 📊 Monitoramento
As vendas são enviadas em tempo real e podem ser visualizadas no Dashboard oficial:
🔗 [Acesse o Dashboard no ThingsBoard](https://thingsboard.cloud/dashboard/42760740-35ff-11f1-9c01-37a7c07792f2?publicId=4fd1fb40-3624-11f1-9c01-37a7c07792f2)


---

## 🧰 Hardware e Software

### Componentes Utilizados
> **Nota:** O projeto foi desenvolvido utilizando a placa **BitDogLab**. Caso utilize componentes avulsos, siga a lista abaixo:
*   Raspberry Pi Pico W
*   Display OLED SSD1306 (I2C)
*   Teclado Matricial 4x4
*   Joystick Analógico e Botões (Push buttons)
*   Buzzer e LEDs
*   Protoboard e Jumpers

### Requisitos de Software
*   [Pico SDK](https://github.com) configurado.
*   GCC ARM Toolchain & CMake.
*   **Extensões VS Code sugeridas:**
    *   *Raspberry Pi Pico Extension* (Oficial)
    *   *C/C++ (Microsoft)*
    *   *CMake (twxs)*
    *   *CMake Tools (Microsoft)*

---

## 🛠️ Instalação e Configuração

1.  **Clonar o repositório:**
    ```bash
    git clone https://github.com/Tobias-Costa/final_project_Embarcatech.git
    ```
2.  **Configurar Credenciais (Obrigatório):**
    No código fonte, altere as definições para os dados da sua rede:
    ```c
    #define WIFI_SSID "NOME-DA-REDE"
    #define WIFI_PASSWORD "SENHA-DA-REDE"
    ```
3.  **Compilar e Carregar:**
    *   Abra a pasta do projeto no VS Code.
    *   Utilize a extensão **Raspberry Pi Pico** para realizar o *Import*, *Compile* e *Flash* para a placa.
    *   > **Nota:** Importe na versão 1.5 do SDK

---

## 🤝 Créditos e Licença

* Este projeto utiliza a biblioteca pico-ssd1306, desenvolvida por David Schramm.
Repositório original: [Clique aqui](https://github.com/daschr/pico-ssd1306)
  *  Licença: [MIT](https://github.com/daschr/pico-ssd1306/blob/main/LICENSE)

* Este projeto utiliza a biblioteca QRCode, desenvolvida por Richard Moore.
Repositório original: [Clique aqui](https://github.com/ricmoo/qrcode/)
  * Licença: [MIT](https://github.com/ricmoo/QRCode/blob/master/LICENSE.txt)

* Este projeto está sob a licença [MIT](LICENSE).
