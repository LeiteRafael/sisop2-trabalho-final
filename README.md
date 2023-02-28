# Parte I

A proposta de trabalho prático é implementar um serviço de “gerenciamento de sono” (sleep management) de estações de trabalho que pertencem a um mesmo segmento de rede física de uma grande organização. O objetivo
do serviço é promover a economia de energia ao incentivar que os colaboradores coloquem suas estações de trabalho para dormir após o termino do expediente na organização. Neste caso, o serviço deve garantir que as
estações poderão ser acordadas caso o colaborador(a) desejar acessar, remotamente, um serviço na sua estação de trabalho (por exemplo, um serviço de compartilhamento de arquivos, para acessar arquivos pessoais).

![image](https://user-images.githubusercontent.com/49589136/221374877-dca7ff74-4819-4698-9489-8e9029f81d56.png)

## Sobre
Sabe-se que uma das formas de acordar estações de trabalho de forma remota é via Wake-On-LAN (WoL). WoL é um padrão de rede que permite ligar ou acordar estações de trabalho a partir do envio de um “pacote mágico” (magic packet) para a placa de rede da estação que se deseja acordar. Para isso, a placa de rede deve suportar WoL
e a placa mãe da estação do trabalho deve estar configurada para aceitar WoL.

O projeto compreenderá UM ÚNICO PROGRAMA, que executará em múltiplas estações, e que deverá oferecer as
funcionalidades previstas nos subserviços abaixo.
<li>
Subserviço de Descoberta (Discovery Subservice), para identificar quais estações de trabalho no mesmo
segmento físico de rede passaram a executar o programa (denominadas participantes) e quais passaram a
não fazer mais parte do serviço.
</li>
<li>
Subserviço de Monitoração (Monitoring Subservice), para identificar quais participantes passaram para o
modo sono (asleep) e quais estão acordadas (awaken).
</li>
<li>
Subserviço de Gerenciamento (Management Subservice), para manter uma lista das estações
participantes e o status de cada participante (asleep ou awaken).
</li>
<li>
Subserviço de Interface (Interface Subservice), para exibir a relação atualizada de participantes na tela e
permitir a interação com usuários (para, por ex., enviar um comando para acordar uma estação).
</li>

## Compilação e Execução

Para compilar o programa, baixe os arquivos e coloque-os em uma pasta.
<br><br>
Em seguida, execute os seguintes comandos no console:
```
gcc main.c manager.c guest.c -lpthread -o sms
./sms [manager|guest]
```

# Parte II

TO-DO
