#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

extern "C" __declspec(dllimport) SHORT __stdcall GetAsyncKeyState(int vKey);

#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <algorithm>
#include <cstdio>

#include "audio_detector.h"

namespace fs = std::filesystem;

// === LOG EM ARQUIVO ===
FILE* gLog = nullptr;
void Log(const char* fmt, ...) {
    if (!gLog) return;
    va_list args;
    va_start(args, fmt);
    vfprintf(gLog, fmt, args);
    va_end(args);
    fprintf(gLog, "\n");
    fflush(gLog);
}

bool TeclaGlobalPressionada(int vKey) {
    return (GetAsyncKeyState(vKey) & 0x0001) != 0;
}

struct AnimacaoClip {
    std::string nome;
    ModelAnimation anim;
    bool carregada;
};

struct AtalhoTeclado {
    int vKey;
    std::string nomeTecla;
    std::string comando;
};

int ObterVKeyPorNome(std::string tecla) {
    std::transform(tecla.begin(), tecla.end(), tecla.begin(), ::toupper);
    if (tecla == "F1")  return 0x70; if (tecla == "F2")  return 0x71;
    if (tecla == "F3")  return 0x72; if (tecla == "F4")  return 0x73;
    if (tecla == "F5")  return 0x74; if (tecla == "F6")  return 0x75;
    if (tecla == "F7")  return 0x76; if (tecla == "F8")  return 0x77;
    if (tecla == "F9")  return 0x78; if (tecla == "F10") return 0x79;
    if (tecla == "F11") return 0x7A; if (tecla == "F12") return 0x7B;
    if (tecla == "NUMPAD0") return 0x60; if (tecla == "NUMPAD1") return 0x61;
    if (tecla == "NUMPAD2") return 0x62; if (tecla == "NUMPAD3") return 0x63;
    if (tecla == "NUMPAD4") return 0x64; if (tecla == "NUMPAD5") return 0x65;
    if (tecla == "NUMPAD6") return 0x66; if (tecla == "NUMPAD7") return 0x67;
    if (tecla == "NUMPAD8") return 0x68; if (tecla == "NUMPAD9") return 0x69;
    if (tecla == "SPACE" || tecla == "ESPACO") return 0x20;
    if (tecla == "ENTER") return 0x0D;
    if (tecla.length() == 1) {
        char c = tecla[0];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z')) return (int)c;
    }
    return 0;
}

// Deltas pré-calculados dos morph targets de abertura da boca e queixo
struct MorphDelta {
    int vertexIndex;
    float dx, dy, dz;
};

// Prim 0: Cabeça/Queixo (Boy01_Body_MAT1)
const MorphDelta gChinDeltas[] = {
    {4307, 0.000000f, -0.027813f, -0.007947f},
    {4320, 0.000000f, -0.017257f, -0.004931f},
    {4390, 0.000000f, -0.010764f, -0.003075f},
    {4391, 0.000000f, -0.017959f, -0.005131f},
    {4420, 0.000000f, -0.011866f, -0.003390f},
    {4421, 0.000000f, -0.002543f, -0.000726f},
    {4509, 0.000000f, -0.011240f, -0.003211f},
    {4510, 0.000000f, -0.018533f, -0.005295f},
    {4648, 0.000000f, -0.017257f, -0.004931f},
    {4649, 0.000000f, -0.011240f, -0.003211f},
    {4726, 0.000000f, -0.011246f, -0.003213f},
    {4783, 0.000000f, -0.002543f, -0.000726f}
};
const int gChinDeltasCount = sizeof(gChinDeltas) / sizeof(gChinDeltas[0]);

// Prim 2: Boca (Boy01_Mouth_MAT2)
const MorphDelta gMouthDeltas[] = {
    {6, 0.000000f, -0.027906f, -0.007973f},
    {8, 0.000000f, -0.017384f, -0.004967f},
    {10, 0.000000f, -0.010517f, -0.003005f},
    {11, 0.000000f, -0.019026f, -0.005436f},
    {12, 0.000000f, -0.011320f, -0.003234f},
    {13, 0.000000f, -0.018471f, -0.005277f},
    {26, 0.000000f, -0.017384f, -0.004967f},
    {27, 0.000000f, -0.011320f, -0.003234f},
    {28, 0.000000f, -0.010952f, -0.003129f},
    {39, 0.000000f, -0.002410f, -0.000688f},
    {45, 0.000000f, -0.002410f, -0.000688f}
};
const int gMouthDeltasCount = sizeof(gMouthDeltas) / sizeof(gMouthDeltas[0]);

int main() {
    gLog = fopen("liveavatar_log.txt", "w");
    Log("=== LiveAvatar Iniciando (Audio Lipsync Ativo) ===");

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(800, 800, "LiveAvatar");
    SetTargetFPS(60);

    Log("Janela criada 800x800");

    // Inicia captura de áudio WASAPI Loopback (detecta voz/sons do Windows)
    if (AudioDetector::Init()) {
        Log("AudioDetector iniciado com sucesso via WASAPI!");
    } else {
        Log("AVISO: Falha ao iniciar AudioDetector");
    }

    // === UDP ===
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    SOCKET udpSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    u_long mode = 1;
    ioctlsocket(udpSocket, FIONBIO, &mode);
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9999);
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    bind(udpSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));
    Log("UDP socket aberto na porta 9999");

    // === MODELO ===
    Model model = { 0 };
    bool modeloCarregado = false;

    // Buffers para sincronização labial híbrida (3D vertex morph + UV atlas)
    std::vector<float> baseMouthUVs;
    std::vector<float> baseChinVertices;
    std::vector<float> baseMouthVertices;

    if (FileExists("personagem.glb")) {
        Log("Carregando personagem.glb...");
        model = LoadModel("personagem.glb");
        modeloCarregado = true;
        Log("Modelo carregado. Meshes=%d, Materiais=%d", model.meshCount, model.materialCount);

        // Copia UVs base da boca (mesh 2) para fallback/ampliação de textura
        if (model.meshCount > 2 && model.meshes[2].texcoords != nullptr) {
            int vCount = model.meshes[2].vertexCount;
            baseMouthUVs.resize(vCount * 2);
            memcpy(baseMouthUVs.data(), model.meshes[2].texcoords, vCount * 2 * sizeof(float));
            Log("Base UVs salvas para a boca: %d vertices", vCount);
        }

        // Salva vértices base de posição
        if (model.meshCount > 0 && model.meshes[0].vertices != nullptr) {
            int vCount = model.meshes[0].vertexCount;
            baseChinVertices.resize(vCount * 3);
            memcpy(baseChinVertices.data(), model.meshes[0].vertices, vCount * 3 * sizeof(float));
        }
        if (model.meshCount > 2 && model.meshes[2].vertices != nullptr) {
            int vCount = model.meshes[2].vertexCount;
            baseMouthVertices.resize(vCount * 3);
            memcpy(baseMouthVertices.data(), model.meshes[2].vertices, vCount * 3 * sizeof(float));
        }
    } else {
        Log("ERRO: personagem.glb nao encontrado!");
    }

    // === ANIMAÇÕES ===
    std::unordered_map<std::string, AnimacaoClip> catalogoAnimacoes;

    auto AnimValida = [](const ModelAnimation& a) -> bool {
        return a.frameCount > 0 && a.framePoses != nullptr;
    };

    auto CarregarTodasAnimacoes = [&]() {
        for (auto& par : catalogoAnimacoes) {
            if (AnimValida(par.second.anim))
                UnloadModelAnimation(par.second.anim);
        }
        catalogoAnimacoes.clear();

        // Idle embutido no personagem.glb
        if (modeloCarregado) {
            int qtd = 0;
            ModelAnimation* anims = LoadModelAnimations("personagem.glb", &qtd);
            Log("personagem.glb tem %d animacoes embutidas", qtd);
            if (anims && qtd > 0) {
                if (AnimValida(anims[0])) {
                    catalogoAnimacoes["idle"] = { "idle", anims[0], true };
                    Log("idle carregado: %d frames", anims[0].frameCount);
                }
                for (int i = 1; i < qtd; i++) UnloadModelAnimation(anims[i]);
                RL_FREE(anims);
            }
        }

        // Animações externas na pasta animacoes/
        if (fs::exists("animacoes")) {
            for (const auto& entry : fs::directory_iterator("animacoes")) {
                if (!entry.is_regular_file()) continue;
                std::string ext = entry.path().extension().string();
                if (ext != ".glb" && ext != ".gltf") continue;
                std::string nomeAcao = entry.path().stem().string();
                std::string path = entry.path().string();
                int count = 0;
                ModelAnimation* carregadas = LoadModelAnimations(path.c_str(), &count);
                Log("animacoes/%s.glb: %d animacoes", nomeAcao.c_str(), count);
                if (carregadas && count > 0 && AnimValida(carregadas[0])) {
                    if (catalogoAnimacoes.count(nomeAcao) && AnimValida(catalogoAnimacoes[nomeAcao].anim))
                        UnloadModelAnimation(catalogoAnimacoes[nomeAcao].anim);
                    catalogoAnimacoes[nomeAcao] = { nomeAcao, carregadas[0], true };
                    Log("  Carregado '%s': %d frames", nomeAcao.c_str(), carregadas[0].frameCount);
                    for (int i = 1; i < count; i++) UnloadModelAnimation(carregadas[i]);
                    RL_FREE(carregadas);
                }
            }
        }

        Log("Total de animacoes no catalogo: %d", (int)catalogoAnimacoes.size());
        for (auto& par : catalogoAnimacoes)
            Log("  '%s': %d frames", par.first.c_str(), par.second.anim.frameCount);
    };

    CarregarTodasAnimacoes();

    // === CONFIG TECLAS ===
    std::vector<AtalhoTeclado> atalhos;
    auto CarregarConfigTeclas = [&]() {
        atalhos.clear();
        std::ifstream arquivo("config_teclas.ini");
        if (!arquivo.is_open()) {
            std::ofstream padrao("config_teclas.ini");
            padrao << "F5=idle\nF6=dancar\nF7=dancar2\nF8=fundo\nF9=reload\n";
            padrao.close();
            arquivo.open("config_teclas.ini");
        }
        std::string linha;
        while (std::getline(arquivo, linha)) {
            if (linha.empty() || linha[0] == '#' || linha[0] == ';') continue;
            size_t sep = linha.find('=');
            if (sep == std::string::npos) continue;
            std::string teclaStr = linha.substr(0, sep);
            std::string cmdStr = linha.substr(sep + 1);
            teclaStr.erase(0, teclaStr.find_first_not_of(" \t\r\n"));
            teclaStr.erase(teclaStr.find_last_not_of(" \t\r\n") + 1);
            cmdStr.erase(0, cmdStr.find_first_not_of(" \t\r\n"));
            cmdStr.erase(cmdStr.find_last_not_of(" \t\r\n") + 1);
            int vk = ObterVKeyPorNome(teclaStr);
            if (vk != 0) atalhos.push_back({ vk, teclaStr, cmdStr });
        }
        Log("Teclas carregadas: %d atalhos", (int)atalhos.size());
    };
    CarregarConfigTeclas();

    // === ESTADO DA ANIMAÇÃO ===
    std::string nomeAnimAtual = catalogoAnimacoes.count("idle") ? "idle" : "";
    if (!catalogoAnimacoes.empty() && nomeAnimAtual.empty())
        nomeAnimAtual = catalogoAnimacoes.begin()->first;

    int frameAtual = 0;
    float animTimer = 0.0f;
    const float ANIM_FPS = 60.0f;
    Log("Animacao inicial: '%s'", nomeAnimAtual.c_str());

    // Câmera frontal de corpo inteiro perfeitamente centralizada
    Camera3D camera = { 0 };
    camera.position   = { 0.0f, -0.90f, 2.5f };
    camera.target     = { 0.0f, -0.90f, 0.0f };
    camera.up         = { 0.0f, 1.0f, 0.0f };
    camera.fovy       = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int modoFundo = 0;
    Color coresFundo[3] = {
        { 0, 255, 0, 255 },    // Verde Chroma Key
        { 35, 35, 35, 255 },   // Cinza escuro (preview)
        { 255, 0, 255, 255 }   // Magenta
    };

    auto DispararAcao = [&](const std::string& cmd) {
        if (catalogoAnimacoes.count(cmd)) {
            Log("Acao: '%s'", cmd.c_str());
            nomeAnimAtual = cmd;
            frameAtual = 0;
            animTimer = 0.0f;
        } else {
            Log("Acao desconhecida: '%s'", cmd.c_str());
        }
    };

    auto ProcessarComando = [&](const std::string& cmd) {
        if (cmd == "fundo") {
            modoFundo = (modoFundo + 1) % 3;
            Log("Fundo mudado para %d", modoFundo);
        } else if (cmd == "reload") {
            Log("Reload solicitado");
            CarregarTodasAnimacoes();
            CarregarConfigTeclas();
            if (!nomeAnimAtual.empty() && !catalogoAnimacoes.count(nomeAnimAtual))
                nomeAnimAtual = catalogoAnimacoes.count("idle") ? "idle" : "";
        } else {
            DispararAcao(cmd);
        }
    };

    char buffer[512];
    int frameLoop = 0;

    // Estado do Lipsync e detecção de fala
    float mouthWeight = 0.0f;
    float audioSilenceTimer = 0.0f;
    bool isAudioTalking = false;

    Log("Entrando no loop principal...");

    while (!WindowShouldClose()) {

        // 1. UDP
        int bytes = 0;
        while ((bytes = recv(udpSocket, buffer, sizeof(buffer) - 1, 0)) > 0) {
            buffer[bytes] = '\0';
            ProcessarComando(std::string(buffer));
        }

        // 2. Teclas globais
        for (const auto& a : atalhos) {
            if (TeclaGlobalPressionada(a.vKey))
                ProcessarComando(a.comando);
        }

        // 3. Detecção de Áudio (WASAPI Loopback)
        float currentAudio = AudioDetector::GetLevel();
        float dt = GetFrameTime();

        // Limiar de detecção de fala
        const float AUDIO_TALK_THRESHOLD = 0.025f;

        if (currentAudio > AUDIO_TALK_THRESHOLD) {
            audioSilenceTimer = 0.0f;
            if (!isAudioTalking) {
                isAudioTalking = true;
                // Se estiver no idle, inicia animação de falar / gesticular
                if (nomeAnimAtual == "idle" && catalogoAnimacoes.count("falar")) {
                    nomeAnimAtual = "falar";
                    frameAtual = 0;
                    animTimer = 0.0f;
                }
            }
        } else {
            audioSilenceTimer += dt;
            // 0.35s de silêncio contínuo antes de voltar para idle
            if (audioSilenceTimer > 0.35f && isAudioTalking) {
                isAudioTalking = false;
                if (nomeAnimAtual == "falar" && catalogoAnimacoes.count("idle")) {
                    nomeAnimAtual = "idle";
                    frameAtual = 0;
                    animTimer = 0.0f;
                }
            }
        }

        // Suavização do peso de abertura da boca
        float targetWeight = isAudioTalking ? std::clamp(currentAudio * 1.5f, 0.0f, 1.0f) : 0.0f;
        mouthWeight += (targetWeight - mouthWeight) * std::min(dt * 20.0f, 1.0f);

        // 4. Atualização da Animação do Esqueleto (60 FPS)
        bool temAnim = modeloCarregado
            && !nomeAnimAtual.empty()
            && catalogoAnimacoes.count(nomeAnimAtual)
            && AnimValida(catalogoAnimacoes[nomeAnimAtual].anim);

        if (temAnim) {
            ModelAnimation& animObj = catalogoAnimacoes[nomeAnimAtual].anim;
            animTimer += dt;

            if (animTimer >= (1.0f / ANIM_FPS)) {
                animTimer -= (1.0f / ANIM_FPS);
                frameAtual++;

                if (frameAtual >= animObj.frameCount) {
                    frameAtual = 0;
                    // Se estava falando e o som parou, retorna para idle
                    if (nomeAnimAtual == "falar" && !isAudioTalking && catalogoAnimacoes.count("idle")) {
                        nomeAnimAtual = "idle";
                    }
                    // Se for outra animação disparada pontualmente (ex: dança) e terminar, volta para idle
                    else if (nomeAnimAtual != "idle" && nomeAnimAtual != "falar" && catalogoAnimacoes.count("idle")) {
                        nomeAnimAtual = "idle";
                    }
                }

                UpdateModelAnimation(model, animObj, frameAtual);
            }
        }

        // 5. Atualização da Boca (Lipsync Híbrido: 3D Vertex Morph + UV Texture Atlas)
        if (modeloCarregado) {
            // Método A: Deformação 3D da malha labial e queixo
            if (model.meshCount > 2 && model.meshes[2].animVertices != nullptr && !baseMouthVertices.empty()) {
                // Restaura e aplica delta nos animVertices da boca (mesh 2)
                for (int i = 0; i < gMouthDeltasCount; i++) {
                    int vi = gMouthDeltas[i].vertexIndex;
                    int idx = vi * 3;
                    model.meshes[2].animVertices[idx + 0] = baseMouthVertices[idx + 0] + gMouthDeltas[i].dx * mouthWeight;
                    model.meshes[2].animVertices[idx + 1] = baseMouthVertices[idx + 1] + gMouthDeltas[i].dy * mouthWeight;
                    model.meshes[2].animVertices[idx + 2] = baseMouthVertices[idx + 2] + gMouthDeltas[i].dz * mouthWeight;
                }
                UpdateMeshBuffer(model.meshes[2], 0, model.meshes[2].animVertices, model.meshes[2].vertexCount * 3 * sizeof(float), 0);
            }

            if (model.meshCount > 0 && model.meshes[0].animVertices != nullptr && !baseChinVertices.empty()) {
                // Aplica delta nos animVertices do queixo/lábio inferior (mesh 0)
                for (int i = 0; i < gChinDeltasCount; i++) {
                    int vi = gChinDeltas[i].vertexIndex;
                    int idx = vi * 3;
                    model.meshes[0].animVertices[idx + 0] = baseChinVertices[idx + 0] + gChinDeltas[i].dx * mouthWeight;
                    model.meshes[0].animVertices[idx + 1] = baseChinVertices[idx + 1] + gChinDeltas[i].dy * mouthWeight;
                    model.meshes[0].animVertices[idx + 2] = baseChinVertices[idx + 2] + gChinDeltas[i].dz * mouthWeight;
                }
                UpdateMeshBuffer(model.meshes[0], 0, model.meshes[0].animVertices, model.meshes[0].vertexCount * 3 * sizeof(float), 0);
            }

            // Método B: UV Texture Atlas Fallback / Enriquecimento visual da boca
            if (model.meshCount > 2 && model.meshes[2].texcoords != nullptr && !baseMouthUVs.empty()) {
                // No Boy01_FacialAnimMap, as bocas abertas ficam deslocadas verticalmente no atlas V
                float vOffset = 0.0f;
                if (mouthWeight > 0.65f) {
                    vOffset = 0.30f; // Boca bem aberta
                } else if (mouthWeight > 0.35f) {
                    vOffset = 0.20f; // Boca intermediária
                } else if (mouthWeight > 0.10f) {
                    vOffset = 0.10f; // Boca semi-aberta
                }

                int vCount = model.meshes[2].vertexCount;
                for (int i = 0; i < vCount; i++) {
                    model.meshes[2].texcoords[i * 2 + 1] = baseMouthUVs[i * 2 + 1] + vOffset;
                }
                UpdateMeshBuffer(model.meshes[2], 1, model.meshes[2].texcoords, vCount * 2 * sizeof(float), 0);
            }
        }

        // Log periódico de status
        frameLoop++;
        if (frameLoop % 300 == 0) {
            Log("Frame %d | anim='%s' frame=%d | Audio=%.3f Mouth=%.2f | FPS=%.1f",
                frameLoop, nomeAnimAtual.c_str(), frameAtual, currentAudio, mouthWeight, GetFPS());
        }

        // 6. Renderização
        BeginDrawing();
            ClearBackground(coresFundo[modoFundo]);
            BeginMode3D(camera);
                if (modeloCarregado) {
                    DrawModel(model, { 0.0f, 0.0f, 0.0f }, 1.0f, WHITE);
                } else {
                    DrawCubeWires({ 0.0f, 1.0f, 0.0f }, 0.5f, 1.0f, 0.5f, MAROON);
                    DrawGrid(10, 1.0f);
                }
            EndMode3D();

            // HUD informativo (desabilitado em modo Chroma Key verde)
            if (modoFundo != 0) {
                DrawText("LiveAvatar - Audio Lipsync Ativo", 10, 10, 18, WHITE);
                std::string status = "Animacao: " + (nomeAnimAtual.empty() ? "Nenhuma" : nomeAnimAtual);
                DrawText(status.c_str(), 10, 32, 16, LIGHTGRAY);

                char audioBuf[64];
                snprintf(audioBuf, sizeof(audioBuf), "Audio: %.1f%% | Boca: %.1f%%", currentAudio * 100.0f, mouthWeight * 100.0f);
                DrawText(audioBuf, 10, 52, 16, isAudioTalking ? GREEN : DARKGRAY);

                // Barra visual de áudio
                DrawRectangle(10, 75, 200, 10, DARKGRAY);
                DrawRectangle(10, 75, (int)(200.0f * std::clamp(currentAudio, 0.0f, 1.0f)), 10, GREEN);

                std::string fps = "FPS: " + std::to_string((int)GetFPS());
                DrawText(fps.c_str(), 10, 92, 14, DARKGRAY);
            }
        EndDrawing();
    }

    Log("Encerrando LiveAvatar...");
    AudioDetector::Shutdown();

    for (auto& par : catalogoAnimacoes) {
        if (AnimValida(par.second.anim))
            UnloadModelAnimation(par.second.anim);
    }
    if (modeloCarregado) UnloadModel(model);
    closesocket(udpSocket);
    WSACleanup();

    if (gLog) fclose(gLog);
    CloseWindow();
    return 0;
}
