# Editor de atributos para PES 6 (PAL) e Winning Eleven 10 - PS2

Programa de PS2 (`.elf`) para mudar atributos dos jogadores (ate 127) direto no console,
lendo o save do **memory card fisico**. Abre pela aba Apps do OPL ou pelo uLaunchELF.

> **Status:** o nucleo (cifra, checksums, edicao) foi testado no PC e reproduz byte a byte o save do WE10.
> A parte de tela/joystick/memory card **nao foi testada em PS2 nem no PCSX2** - foi escrita sem poder compilar.
> Pode dar erro de compilacao ou travar. Use SEMPRE uma copia do memory card.

## 1) Compilar (de graca, sem instalar nada)
1. Crie uma conta em github.com e um repositorio novo (botao "New").
2. Em "Add file > Upload files", envie TODOS os arquivos desta pasta (`main.c`, `core.c`, `core.h`, `Makefile`, `README.md`, a pasta `tests`).
3. Crie o arquivo do compilador: "Add file > Create new file", nome `.github/workflows/build.yml` (digite com as barras), e cole o conteudo do `build.yml` desta pasta. Clique "Commit".
4. Aba **Actions**: espere o circulo ficar verde (uns 2-3 min). Se ficar vermelho, abra e tire print do erro e me mande.
5. Clique na execucao > "Artifacts" > baixe `we_editor_elf` > descompacte: sai o `we_editor.elf`.

## 2) Testar no PCSX2 (recomendado antes do PS2)
- Em Configuracoes > Cartoes de memoria, crie/importe um cartao e importe o save (`.psu`) do WE10 ou PES 6 PAL.
- Sistema > Executar arquivo ELF > `we_editor.elf`.

## 3) Usar no PS2 via OPL
- Copie `we_editor.elf` para o pendrive/HDD (ex.: `APPS/we_editor.elf`).
- No memory card, em `OPL/conf_apps.cfg`, adicione a linha: `Editor PES=mass:/APPS/we_editor.elf`
- No OPL, aba Apps > Editor PES.

## Controles
- Clubes: CIMA/BAIXO, L1/R1 (+-10), X escolhe, SELECT digita ID do jogador, START salva, L1+R1+START sai sem salvar.
- Elenco: X edita o jogador, O volta.
- Atributos: CIMA/BAIXO escolhe, ESQ/DIR -+1, L1/R1 -+10, QUADRADO=99, TRIANGULO=127, O volta.

## Limites conhecidos
- So memory card fisico (mc0/mc1). O memory card virtual (.bin) do OPL nao e lido.
- O save nao guarda nomes dos jogadores: aparece o ID e os atributos. Os nomes dos atributos sao aproximados.
- Nao edita formacoes, taticas, times, uniformes.
- Atributo acima de 99 so vale se o jogo aceitar - teste.
- Para PES 6 PAL o programa usa as mesmas tabelas do PES 6 Editor; para o WE10, as que descobri no save enviado.

## Teste do nucleo no PC
```
gcc -O2 -I. tests/host_test.c core.c -o host_test
./host_test opcoes.bin saida.bin
```
