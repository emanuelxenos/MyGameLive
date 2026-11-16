# LiveAvatar 🎭

Sistema leve e otimizado em **C++ e Raylib 5.5** para renderização de avatar 3D em tempo real com suporte a Chroma Key para transmissões ao vivo (OBS Studio, Streamlabs, etc.).

O software permite acionar animações corporais dinamicamente através de **teclas de atalho globais** (mesmo com o jogo ou outra janela em foco) ou via **comandos de rede UDP**, com retorno automático para a pose de descanso (*idle*).

---

## 🚀 Funcionalidades

- **Chroma Key Integrado:** Fundo verde puro (`#00FF00`) pronto para captura de janela no OBS com filtro de remoção de fundo sem sobrecarregar a GPU.
- **Suporte a Modelos Mixamo:** Compatível com modelos e esqueletos humanoides no formato standard `.glb` / `.gltf`.
- **Animações Dinâmicas:** Pasta dedicada `animacoes/` onde basta soltar arquivos `.glb` com o nome da ação desejada.
- **Atalhos de Teclado Globais:** Dispare animações usando teclas configuráveis via arquivo `config_teclas.ini` (F1-F12, teclado numérico, letras), funcionando mesmo em segundo plano.
- **Controle Remoto via UDP:** Porta `9999` para integração com bots de chat (Twitch, YouTube, Kick), Stream Deck, Channel Points ou scripts externos (Python, Node.js).
- **Recarregamento a Quente (*Hot Reload*):** Adicione novas animações ou altere atalhos e pressione `F9` (ou envie o comando `reload`) sem precisar fechar o aplicativo.
- **Otimizado para GPUs Integradas:** Skinning estável a 30 FPS na CPU/GPU, consumindo poucos recursos durante a gameplay.

---

## 📁 Estrutura de Arquivos

```text
MyGameLive/
├── LiveAvatar.exe         # Executável principal
├── compilar.bat           # Script de compilação rápida via MinGW/G++
├── main.cpp               # Código-fonte em C++
├── config_teclas.ini      # Mapeamento de teclas de atalho globais
├── personagem.glb         # Modelo 3D principal (com animação idle embutida)
├── animacoes/             # Pasta onde ficam as animações adicionais (.glb)
│   ├── dancar.glb
│   └── tchau.glb
├── teste_comandos.py      # Script em Python para envio de comandos de teste via UDP
├── liveavatar_log.txt     # Log em tempo real para diagnóstico
└── *.dll                  # Dependências de execução (Raylib, GLFW, MinGW)
```

---

## 🎮 Como Usar

### 1. Executar o Programa
Dê dois cliques diretamente no arquivo `LiveAvatar.exe` (ou execute `compilar.bat`).

### 2. Configurar no OBS Studio
1. Abra o OBS Studio e crie uma nova fonte de **Captura de Janela** (*Window Capture*).
2. Selecione a janela `LiveAvatar`.
3. Clique com o botão direito na fonte no OBS > **Filtros** > **Filtros de Efeito (+)**.
4. Adicione o filtro **Chroma Key** (o verde padrão já removerá o fundo perfeitamente).
5. Posicione e redimensione seu avatar sobre a sua transmissão.

---

## ⌨️ Atalhos de Teclado (`config_teclas.ini`)

O mapeamento de teclas pode ser editado no arquivo `config_teclas.ini`:

```ini
F5=idle
F6=dancar
F7=tchau
F8=fundo
F9=reload

# Outros exemplos suportados:
# NUMPAD1=dancar
# NUMPAD2=pular
# SPACE=pular
```

- **`F8`:** Alterna a cor do fundo (Verde Chroma Key / Cinza Neutro / Magenta).
- **`F9`:** Recarrega todas as animações e atalhos sem reiniciar.

---

## 🌐 Comandos via Rede (UDP)

O sistema escuta comandos de texto simples via socket UDP não-bloqueante na porta **`9999`**.

### Exemplo via Terminal / Script Python:
Execute o arquivo `teste_comandos.py`:
```bash
python teste_comandos.py
```
E digite o nome de qualquer animação presente na pasta `animacoes/` (ex: `dancar`, `tchau`, `idle`) ou envie `reload`.

---

## 📦 Adicionando Novas Animações (Mixamo)

1. Baixe a animação desejada no site do [Mixamo](https://www.mixamo.com) no formato **FBX** (selecione a opção *"In Place"* quando disponível).
2. Salve o arquivo na pasta `animacoes/` com extensão `.glb` e com o nome do comando (ex: `animacoes/dancar.glb`).
3. Pressione `F9` na tela do `LiveAvatar` para carregar imediatamente a nova ação.

---

## 🛠️ Compilação Manual

Requisitos:
- MinGW-w64 (GCC com suporte a C++17)
- Raylib 5.5

Comando de compilação:
```powershell
g++ -O2 -std=c++17 main.cpp -o LiveAvatar.exe -lraylib -lopengl32 -lgdi32 -lwinmm -lws2_32
```
Ou simplesmente execute o arquivo `compilar.bat`.
