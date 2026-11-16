#include "raylib.h"
#include "raymath.h"

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

int main() {
    // Abre log
    gLog = fopen("liveavatar_log.txt", "w");
    Log("=== LiveAvatar Iniciando ===");

    SetTraceLogLevel(LOG_WARNING); // Reduz spam do raylib no log
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(800, 800, "LiveAvatar");
    SetTargetFPS(60);

    Log("Janela criada 800x800");

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

    if (FileExists("personagem.glb")) {
        Log("Carregando personagem.glb...");
        model = LoadModel("personagem.glb");
        modeloCarregado = true;
        Log("Modelo carregado. Meshes=%d, Materiais=%d", model.meshCount, model.materialCount);
    } else {
        Log("ERRO: personagem.glb nao encontrado!");
    }

    // === ANIMAÇÕES ===
    std::unordered_map<std::string, AnimacaoClip> catalogoAnimacoes;

    // Verificação defensiva de animação antes de usar
    auto AnimValida = [](const ModelAnimation& a) -> bool {
        return a.frameCount > 0 && a.framePoses != nullptr;
    };

    auto CarregarTodasAnimacoes = [&]() {
        // Limpa animações antigas
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
                for (int i = 0; i < qtd; i++) {
                    Log("  Anim[%d]: nome='%s' frames=%d bones=%d",
                        i, anims[i].name, anims[i].frameCount, anims[i].boneCount);
                }
                // Primeira animação = idle
                if (AnimValida(anims[0])) {
                    catalogoAnimacoes["idle"] = { "idle", anims[0], true };
                    Log("idle carregado: %d frames", anims[0].frameCount);
                }
                // Libera as extras (índice 1 em diante) se houver
                for (int i = 1; i < qtd; i++) {
                    UnloadModelAnimation(anims[i]);
                }
                RL_FREE(anims);
            } else {
                Log("AVISO: Nenhuma animacao embutida no personagem.glb");
            }
        }

        // Animações da pasta animacoes/
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
                    // Libera anterior se existir
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
            padrao << "F5=idle\nF6=dancar\nF7=tchau\nF8=fundo\nF9=reload\n";
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
    const float ANIM_FPS = 30.0f;
    Log("Animacao inicial: '%s'", nomeAnimAtual.c_str());

    // === DESCOBRE ONDE O PERSONAGEM REALMENTE ESTÁ NO MUNDO ===
    // Aplica o frame 0 da animação e mede o bounding box REAL (pós-skinning)
    // Isso resolve o problema do personagem aparecer deslocado/no canto
    int rootBoneIdx = -1;
    Vector3 rootBoneRefPos = { 0.0f, 0.0f, 0.0f };
    Vector3 charCenter = { 0.0f, 0.93f, 0.0f };
    float   charHeight = 1.86f;

    if (modeloCarregado && !catalogoAnimacoes.empty()) {
        ModelAnimation& a0 = catalogoAnimacoes.begin()->second.anim;

        // Encontra bone raiz (parent == -1) para travar root motion
        if (a0.bones != nullptr) {
            for (int b = 0; b < a0.boneCount; b++) {
                if (a0.bones[b].parent == -1) {
                    rootBoneIdx = b;
                    rootBoneRefPos = a0.framePoses[0][b].translation;
                    Log("Root bone idx=%d name='%s' refPos={%.3f, %.3f, %.3f}",
                        b, a0.bones[b].name,
                        rootBoneRefPos.x, rootBoneRefPos.y, rootBoneRefPos.z);
                    break;
                }
            }
        }

        // Aplica frame 0 para medir onde os vértices realmente ficam
        if (AnimValida(a0)) {
            UpdateModelAnimation(model, a0, 0);
            BoundingBox bb = GetMeshBoundingBox(model.meshes[0]);
            for (int mi = 1; mi < model.meshCount; mi++) {
                BoundingBox b2 = GetMeshBoundingBox(model.meshes[mi]);
                bb.min.x = fminf(bb.min.x, b2.min.x);
                bb.min.y = fminf(bb.min.y, b2.min.y);
                bb.min.z = fminf(bb.min.z, b2.min.z);
                bb.max.x = fmaxf(bb.max.x, b2.max.x);
                bb.max.y = fmaxf(bb.max.y, b2.max.y);
                bb.max.z = fmaxf(bb.max.z, b2.max.z);
            }
            charCenter = {
                (bb.min.x + bb.max.x) * 0.5f,
                (bb.min.y + bb.max.y) * 0.5f,
                (bb.min.z + bb.max.z) * 0.5f
            };
            charHeight = bb.max.y - bb.min.y;
            Log("BBox real (pos-anim): X[%.2f..%.2f] Y[%.2f..%.2f] Z[%.2f..%.2f]",
                bb.min.x, bb.max.x, bb.min.y, bb.max.y, bb.min.z, bb.max.z);
            Log("charCenter={%.2f, %.2f, %.2f} charHeight=%.2f",
                charCenter.x, charCenter.y, charCenter.z, charHeight);
        }
    }

    // === CÂMERA FRONTAL (PERSPECTIVA) ===
    // O modelo exportado do Blender com Transform Applied tem escala 1.0 real e centro em Y=0.9m
    Camera3D camera = { 0 };
    camera.position   = { 0.0f, 0.95f, 2.3f };
    camera.target     = { 0.0f, 0.90f, 0.0f };
    camera.up         = { 0.0f, 1.0f, 0.0f };
    camera.fovy       = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    Log("Camera PERSPECTIVE configurada: pos={0, 0.95, 2.3} target={0, 0.90, 0}");
    // === FUNDO ===
    // 0=Verde (Chroma Key OBS), 1=Cinza escuro, 2=Magenta
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

        // 3. Atualização de animação (30 fps, estável)
        bool temAnim = modeloCarregado
            && !nomeAnimAtual.empty()
            && catalogoAnimacoes.count(nomeAnimAtual)
            && AnimValida(catalogoAnimacoes[nomeAnimAtual].anim);

        if (temAnim) {
            ModelAnimation& animObj = catalogoAnimacoes[nomeAnimAtual].anim;
            animTimer += GetFrameTime();

            if (animTimer >= (1.0f / ANIM_FPS)) {
                animTimer -= (1.0f / ANIM_FPS);
                frameAtual++;

                if (frameAtual >= animObj.frameCount) {
                    frameAtual = 0;
                    // Volta para idle se não for idle
                    if (nomeAnimAtual != "idle" && catalogoAnimacoes.count("idle"))
                        nomeAnimAtual = "idle";
                }

                // Animação natural limpa - sem forçar transforms artificiais nos bones
                UpdateModelAnimation(model, animObj, frameAtual);
            }
        }

        // Log de saúde a cada 300 frames (~5s)
        frameLoop++;
        if (frameLoop % 300 == 0) {
            Log("Frame %d | anim='%s' frameAnim=%d | FPS=%.1f",
                frameLoop, nomeAnimAtual.c_str(), frameAtual, GetFPS());
        }

        // 4. Renderização
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

            // HUD leve (apenas no fundo não-verde para não sujar o chroma key)
            if (modoFundo != 0) {
                DrawText("LiveAvatar", 10, 10, 18, WHITE);
                std::string status = "Anim: " + (nomeAnimAtual.empty() ? "Nenhuma" : nomeAnimAtual);
                DrawText(status.c_str(), 10, 32, 16, LIGHTGRAY);
                std::string fps = "FPS: " + std::to_string((int)GetFPS());
                DrawText(fps.c_str(), 10, 52, 14, DARKGRAY);
            }
        EndDrawing();
    }

    Log("Encerrando...");

    // Cleanup
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
