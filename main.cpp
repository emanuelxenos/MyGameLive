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

int main() {
    gLog = fopen("liveavatar_log.txt", "w");
    Log("=== LiveAvatar Iniciando (Audio Lipsync Ativo) ===");

    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(800, 800, "LiveAvatar");
    SetTargetFPS(60);

    Log("Janela criada 800x800");

    // Inicia captura de áudio WASAPI Loopback (captura voz/sons do Windows)
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

    // Buffer de UVs originais da boca
    std::vector<float> baseMouthUVs;
    int mouthMeshIndex = -1;

    // === TEXTURAS DINÂMICAS DE BOCA DO AVATURN ===
    Image imgBoca0 = { 0 }, imgBoca1 = { 0 }, imgBoca2 = { 0 };
    bool temBocasAvaturn = false;
    Texture2D* texRostoAvaturn = nullptr;
    // Coordenadas exatas dos lábios no mapa UV (rotacionado 90 graus)
    const Rectangle kRecBoca = { 61.0f, 611.0f, 128.0f, 128.0f };

    if (FileExists("boca_0.png") && FileExists("boca_1.png") && FileExists("boca_2.png")) {
        imgBoca0 = LoadImage("boca_0.png");
        imgBoca1 = LoadImage("boca_1.png");
        imgBoca2 = LoadImage("boca_2.png");
        ImageFormat(&imgBoca0, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        ImageFormat(&imgBoca1, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        ImageFormat(&imgBoca2, PIXELFORMAT_UNCOMPRESSED_R8G8B8);
        temBocasAvaturn = true;
        Log("Imagens de boca do Avaturn carregadas (64x64 RGBA)");
    }

    if (FileExists("personagem.glb")) {
        Log("Carregando personagem.glb...");
        model = LoadModel("personagem.glb");
        modeloCarregado = true;
        Log("Modelo carregado. Meshes=%d, Materiais=%d", model.meshCount, model.materialCount);

        // Procura em todos os materiais qual tem a textura de 1024x1024 do rosto
        for (int i = 0; i < model.materialCount; i++) {
            Texture2D& t = model.materials[i].maps[MATERIAL_MAP_DIFFUSE].texture;
            Log("Material %d: tex.id=%d, w=%d, h=%d, format=%d, mipmaps=%d", i, t.id, t.width, t.height, t.format, t.mipmaps);
            if (t.width >= 512 && t.height >= 512) {
                texRostoAvaturn = &t;
                Log("Encontrada textura real do rosto no Material %d (%dx%d)", i, t.width, t.height);
                break;
            }
        }

        // Se nenhuma textura com mais de 512px foi encontrada, carrega avaturn_rosto_diffuse.png
        if (texRostoAvaturn == nullptr && FileExists("avaturn_rosto_diffuse.png")) {
            Texture2D texCarregada = LoadTexture("avaturn_rosto_diffuse.png");
            if (texCarregada.id > 0) {
                // Atribui ao material do corpo (material 0)
                model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = texCarregada;
                texRostoAvaturn = &model.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture;
                Log("Textura avaturn_rosto_diffuse.png aplicada com sucesso ao Material 0 (%dx%d)",
                    texRostoAvaturn->width, texRostoAvaturn->height);
            }
        }
        for (int i = 0; i < model.meshCount; i++) {
            if (model.meshes[i].vertexCount == 49) {
                mouthMeshIndex = i;
                break;
            }
        }

        if (mouthMeshIndex >= 0 && model.meshes[mouthMeshIndex].texcoords != nullptr) {
            int vCount = model.meshes[mouthMeshIndex].vertexCount;
            baseMouthUVs.resize(vCount * 2);
            memcpy(baseMouthUVs.data(), model.meshes[mouthMeshIndex].texcoords, vCount * 2 * sizeof(float));
            Log("Base UVs salvas para a boca (Mesh %d): %d vertices", mouthMeshIndex, vCount);
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

    // Aplica imediatamente o frame 0 para não exibir T-pose no início
    if (modeloCarregado && !nomeAnimAtual.empty() && catalogoAnimacoes.count(nomeAnimAtual)) {
        UpdateModelAnimation(model, catalogoAnimacoes[nomeAnimAtual].anim, 0);
        Log("Frame 0 aplicado imediatamente na inicializacao.");
    }

    // Câmera frontal de corpo inteiro configurável por config_camera.ini
    Camera3D camera = { 0 };
    float camY = 0.95f, camZ = 2.3f, camFov = 45.0f;
    {
        std::ifstream cf("config_camera.ini");
        if (cf.is_open()) {
            std::string ln;
            while (std::getline(cf, ln)) {
                if (ln.rfind("camY=", 0) == 0)   camY   = std::stof(ln.substr(5));
                if (ln.rfind("camZ=", 0) == 0)   camZ   = std::stof(ln.substr(5));
                if (ln.rfind("camFov=", 0) == 0) camFov = std::stof(ln.substr(7));
            }
        }
    }
    camera.position   = { 0.0f, camY, camZ };
    camera.target     = { 0.0f, camY, 0.0f };
    camera.up         = { 0.0f, 1.0f, 0.0f };
    camera.fovy       = camFov;
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
    int currentMouthPhoneme = 0;

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
            isAudioTalking = true;
        } else {
            audioSilenceTimer += dt;
            if (audioSilenceTimer > 0.25f) {
                isAudioTalking = false;
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
                    // Se estiver falando e o áudio parou, volta para idle
                    if (nomeAnimAtual == "falar" && !isAudioTalking && catalogoAnimacoes.count("idle")) {
                        nomeAnimAtual = "idle";
                    }
                    // Dança e idle continuam em loop normal sem corte abrupto
                }

                // O Raylib calcula a animação do esqueleto limpa, sem artefatos nos vértices
                UpdateModelAnimation(model, animObj, frameAtual);
            }
        }

        // 5. Atualização da Boca (Texture Atlas Facial UV)
        // O atlas possui 4 estados de boca alinhados verticalmente:
        // [0] Fechada (repouso) : V offset = 0.0000f
        // [1] Semi-aberta       : V offset = 0.0963f
        // [2] Média (vogal)     : V offset = 0.2000f
        // [3] Aberta (fala alta): V offset = 0.3023f
        if (modeloCarregado) {  // Avaturn: sempre processa boca
            int targetPhoneme = 0;
            if (mouthWeight > 0.60f) {
                targetPhoneme = 3;
            } else if (mouthWeight > 0.35f) {
                targetPhoneme = 2;
            } else if (mouthWeight > 0.08f) {
                targetPhoneme = 1;
            } else {
                targetPhoneme = 0;
            }

            if (targetPhoneme != currentMouthPhoneme) {
                currentMouthPhoneme = targetPhoneme;
                Log("Boca nivel: %d (peso=%.2f)", currentMouthPhoneme, mouthWeight);

                // 1. Chaveamento UV (para modelos estilo Boy01)
                if (mouthMeshIndex >= 0 && !baseMouthUVs.empty()) {
                    float vOffsets[4] = { 0.0000f, 0.0963f, 0.2000f, 0.3023f };
                    float vOffset = vOffsets[currentMouthPhoneme];

                    int vCount = model.meshes[mouthMeshIndex].vertexCount;
                    for (int i = 0; i < vCount; i++) {
                        model.meshes[mouthMeshIndex].texcoords[i * 2 + 1] = baseMouthUVs[i * 2 + 1] + vOffset;
                    }
                    UpdateMeshBuffer(model.meshes[mouthMeshIndex], 1, model.meshes[mouthMeshIndex].texcoords, vCount * 2 * sizeof(float), 0);
                }

                // 2. Chaveamento Dinâmico de Textura (para modelos Avaturn)
                if (temBocasAvaturn && texRostoAvaturn != nullptr) {
                    void* pixelsData = nullptr;
                    if (currentMouthPhoneme == 0) pixelsData = imgBoca0.data;
                    else if (currentMouthPhoneme == 1) pixelsData = imgBoca1.data;
                    else pixelsData = imgBoca2.data;

                    if (pixelsData != nullptr) {
                        UpdateTextureRec(*texRostoAvaturn, kRecBoca, pixelsData);
                    }
                }
            }
        }

        // Log periódico de status
        frameLoop++;
        if (frameLoop % 300 == 0) {
            Log("Frame %d | anim='%s' frame=%d | Audio=%.3f Boca=%d (%.2f) | FPS=%.1f",
                frameLoop, nomeAnimAtual.c_str(), frameAtual, currentAudio, currentMouthPhoneme, mouthWeight, GetFPS());
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
                snprintf(audioBuf, sizeof(audioBuf), "Audio: %.1f%% | Boca nivel: %d", currentAudio * 100.0f, currentMouthPhoneme);
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

    if (temBocasAvaturn) {
        UnloadImage(imgBoca0);
        UnloadImage(imgBoca1);
        UnloadImage(imgBoca2);
    }

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







