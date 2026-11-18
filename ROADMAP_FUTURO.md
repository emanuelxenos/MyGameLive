# 🚀 Roadmap & Especificação de Recursos Futuros — LiveAvatar

Este documento reúne o planejamento técnico detalhado, arquitetura e passos de implementação das próximas funcionalidades essenciais do **LiveAvatar**, permitindo que qualquer desenvolvedor implemente os recursos com precisão e consistência.

---

## 📌 Sumário
1. [Módulo 1: Calibrador Visual Interativo da Boca (Mouth Calibrator UI)](#1-módulo-1-calibrador-visual-interativo-da-boca-mouth-calibrator-ui)
2. [Módulo 2: Sistema de Coreografias Sequenciais (Dance Routines Engine)](#2-módulo-2-sistema-de-coreografias-sequenciais-dance-routines-engine)
3. [Cronograma & Boas Práticas de Integração](#3-cronograma--boas-práticas-de-integração)

---

## 1. Módulo 1: Calibrador Visual Interativo da Boca (Mouth Calibrator UI)

### 🎯 Objetivo
Permitir que usuários finais personalizem a posição, tamanho e estilo da textura da boca diretamente pela tela do aplicativo (via mouse), sem necessitar de código-fonte, recompilação ou edição manual de arquivos.

### 🧩 Arquitetura Técnica
Atualmente, o retângulo da boca na textura é fixo via código:
```cpp
const Rectangle kRecBoca = { 61.0f, 611.0f, 128.0f, 128.0f };
```
O objetivo é transformar esse retângulo em um objeto dinâmico gerenciado por uma interface gráfica interativa sobre a textura UV do modelo.

### 🖼️ Interface de Usuário (Modo Calibração)
- **Atalho de Ativação**: Tecla `F2` (ou botão de engrenagem na tela).
- **Overlay Visual**:
  - Exibe um painel lateral/flutuante com a textura facial 2D do modelo (`1024x1024`).
  - Desenha um retângulo de seleção delimitador (*Bounding Box*) na posição atual da boca.
- **Interação com Mouse**:
  - **Arrastar (*Drag & Drop*)**: Clicar dentro do retângulo permite mover a boca nos eixos X e Y da textura.
  - **Redimensionar (*Resize Handles*)**: Alças nos 4 cantos permitem aumentar ou diminuir a largura e a altura do patch labial.
  - **Zoom / Pan**: Scroll do mouse permite aproximar da região facial para microajustes precisos.
- **Live Preview no Modelo 3D**:
  - Cada pixel movido no editor 2D atualiza a GPU instantaneamente via `UpdateTextureRec`, refletindo no rosto 3D do personagem em tempo real.
  - Um slider ou botão "Testar Fala" simula níveis de áudio (boca abrindo e fechando) para validação visual imediata.

### 🎨 Seletor de Estilos de Boca (Presets)
- Pasta dedicada: `assets/mouth_presets/`.
  - Subpastas: `realista/`, `anime/`, `cartoon/`, `custom/`.
  - Cada preset contém 3 arquivos: `boca_0.png` (fechada), `boca_1.png` (semi-aberta), `boca_2.png` (aberta).
- Menu dropdown ou botões de seta na UI para alternar o estilo visual em tempo real.

### 💾 Persistência de Dados (`config_boca.ini`)
Ao clicar em **"Salvar"**, as configurações são gravadas em arquivo:
```ini
[Boca]
posX=61.0
posY=611.0
largura=128.0
altura=128.0
preset=realista
espelhoHorizontal=false
espelhoVertical=false
sensibilidadeAudio=1.5
```

---

## 2. Módulo 2: Sistema de Coreografias Sequenciais (Dance Routines Engine)

### 🎯 Objetivo
Criar um motor de sequenciamento de animações que encadeia múltiplos passos de dança de forma harmônica, temporal e fluida, transformando o avatar em uma atração para lives e eventos.

### 🧩 Arquitetura Técnica
Atualmente, cada animação roda de forma isolada em loop infinito. O motor de **Dance Routines** gerencia um autômato de estados temporizado com suporte a transições suaves (*Blending/Crossfade*).

### ⚙️ Estrutura de Dados Proposta (C++)
```cpp
struct RoutineStep {
    std::string animationName;   // Ex: "dancar", "dancar2", "breakdance"
    float durationSeconds;       // Duração do passo (ex: 12.5f) ou -1 para ciclo completo
    int repeatCount;             // Quantas vezes o clip se repete
    float blendTime;             // Tempo de transição suave para o próximo passo (ex: 0.35s)
};

struct DanceRoutine {
    std::string name;
    std::vector<RoutineStep> steps;
    bool loopRoutine;            // Se repete a rotina inteira ou finaliza
    std::string exitAnimation;   // Animação ao terminar (geralmente "idle")
};
```

### 📜 Formato de Configuração (`dance_routines.ini`)
O usuário poderá criar rotinas infinitas de forma declarativa:
```ini
; ========================================================
; CONFIGURAÇÃO DE DANCE ROUTINES (LIVESTREAM)
; ========================================================

[Routine_HipHop]
loop=true
exit=idle
step1=dancar, duration=10.0, blend=0.4
step2=dancar2, duration=15.0, blend=0.5
step3=breakdance, duration=8.0, blend=0.3

[Routine_Funk]
loop=false
exit=idle
step1=samba, duration=12.0, blend=0.3
step2=dancar2, duration=10.0, blend=0.4

[Routine_Comemoracao]
loop=false
exit=idle
step1=dancar2, duration=6.0, blend=0.3
```

### 🔀 Suavização entre Danças (Bone Blending / Crossfade)
Para eliminar cortes bruscos entre animações com poses diferentes:
- Quando o passo atual estiver nos seus últimos `blendTime` segundos:
  - O Raylib calcula a interpolação esférica (SLERP) entre os quaternions do esqueleto da animação de saída e da animação de entrada.
  - A transição resulta em uma troca de passos contínua e natural, como em coreografias profissionais.

### 🎮 Gatilhos de Ativação
1. **Atalhos de Teclado**: Mapeamento de teclas de função ou Numpad no `config_teclas.ini` (ex: `NUMPAD1=routine:Routine_HipHop`).
2. **Integração UDP / Chat Bot**:
   - Envio de pacotes de rede pela porta `9999`:
     ```text
     ROUTINE Routine_HipHop
     ```
   - Permite que bots de TikTok, Twitch e YouTube disparem rotinas com base em presentes (*gifts*), doações ou comandos no chat.

---

## 3. Cronograma & Boas Práticas de Integração

| Fase | Recurso | Complexidade | Dependências |
|---|---|---|---|
| **Fase 1** | Motor de Dance Routines | Média | Nenhuma (usa o catálogo de GLBs atual) |
| **Fase 2** | Crossfade / Blending entre Danças | Alta | Requer cálculo de interpolação entre poses esqueléticas |
| **Fase 3** | Leitor de Presets de Boca via `.ini` | Baixa | Separar parâmetros hardcoded do `main.cpp` |
| **Fase 4** | Interface Visual com Mouse (Mouth Calibrator) | Média/Alta | Implementar desenho 2D e detecção de cliques com Raylib |

---
*Documento elaborado e registrado para a evolução contínua da arquitetura do LiveAvatar.*
