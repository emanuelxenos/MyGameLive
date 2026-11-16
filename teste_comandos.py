import socket
import time

UDP_IP = "127.0.0.1"
UDP_PORT = 9999

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def enviar_comando(cmd: str):
    print(f"Enviando comando: {cmd}")
    sock.sendto(cmd.encode('utf-8'), (UDP_IP, UDP_PORT))

if __name__ == "__main__":
    print("Script de Teste de Comandos da Live (Mixamo / Raylib)")
    print("Comandos disponíveis: dancar, dancar2, idle, falar:1, falar:0, sair")
    
    while True:
        try:
            comando = input("\nDigite o comando para enviar: ").strip()
            if comando.lower() == "sair":
                break
            if comando:
                enviar_comando(comando)
        except KeyboardInterrupt:
            break
