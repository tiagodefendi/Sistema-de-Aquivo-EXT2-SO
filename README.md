# 🚀 Sistema de Arquivo EXT2 em C

Bem-vindo(a) ao projeto de um **Sistema de arquivos EXT2** !

Aqui você encontra uma implementação do **Sistema de arquivos EXT2**. Explore, teste e aprenda como funciona um sistema de arquivos real, tudo via um shell interativo!

As estruturas e a lógica de manipulação foram implementadas a partir da especificação do EXT2: https://www.nongnu.org/ext2-doc/ext2.html

---

## ✨ Funcionalidades Principais

- 📄 Criação, leitura, escrita e remoção de arquivos
- 📁 Gerenciamento de diretórios
- 🗃️ Alocação e liberação de blocos e inodes
- 🧩 Estruturas compatíveis com o padrão EXT2

---

## 🛠️ Detalhes de Implementação

- **Linguagem utilizada:** C/C++.

- **Restrições:** Não utilizar chamadas para funções do sistema (ex: `system()`, `exec()`) nem estruturas EXT2 prontas de bibliotecas ou da internet.

- **Simplificações permitidas:**
    - Sintaxe dos comandos pode ser simplificada (ex: não tratar múltiplos diretórios como `rm dir1/dir2/file.txt`).
    - Não há arquivos maiores que 64 MiB.
    - Tamanho fixo de bloco: 1024 bytes.
    - Apenas diretórios que usam 1 bloco para armazenar entradas de diretório são tratados.
- **Limitações:**
    - Não é necessário processar arquivos com ponteiros triplamente indiretos.
    - Escrita de entradas em arquivos de diretório limitada ao tamanho do bloco.
    - Leitura de arquivos de diretório apenas com ponteiros diretos.

---

## 🖥️ Comandos do Shell

Experimente os comandos abaixo no shell interativo:

- [x] **info** — Exibe informações do disco e do sistema de arquivos
- [x] **cat &lt;file&gt;** — Mostra o conteúdo de um arquivo
- [x] **attr &lt;file \| dir&gt;** — Exibe atributos de arquivo/diretório
- [x] **cd &lt;path&gt;** — Muda o diretório atual
- [x] **ls** — Lista arquivos e diretórios
- [x] **pwd** — Mostra o caminho absoluto do diretório atual
- [x] **touch &lt;file&gt;** — Cria um arquivo vazio
- [x] **mkdir &lt;dir&gt;** — Cria um diretório vazio
- [x] **rm &lt;file&gt;** — Remove um arquivo
- [x] **rmdir &lt;dir&gt;** — Remove um diretório vazio
- [x] **rename &lt;file&gt; &lt;newfilename&gt;** — Renomeia um arquivo
- [x] **cp &lt;source_path&gt; &lt;target_path&gt;** — Copia arquivo da imagem para o sistema real
- [x] **mv &lt;source_path&gt; &lt;target_path&gt;** — Move arquivo (opcional)
- [x] **print [ superblock | groups | inode ]**: exibe informações do sistema EXT2.

> 💡 **Dicas rápidas:**
> - Comandos (1) a (6): apenas leitura da imagem.
> - Comandos (7) a (11): escrita na imagem.
> - Comandos (12) e (13): interagem entre a imagem EXT2 e o sistema real (use caminhos absolutos).
> - Comando (14): apenas print da estrutura

---

## ⚙️ Como Compilar

1. Certifique-se de ter **GCC** e **Make** instalados.
2. Compile o projeto com:

    ```bash
    make
    ```

---

## ▶️ Como Executar

1. Descompacte a pasta `myext2image.tar.gz`.

```bash
tar -xvzf <nome_da_imagem>.tar.gz
```

2. Execute o shell EXT2 com:

```bash
./ext2shell <nome_da_imagem.img>
```

---

## Para a criação e geração da imagem de volume EXT2

1. Gerando imagens EXT2 (64MiB com blocos de 1K):

```bash
dd if=/dev/zero of=./<nome_da_imagem.img> bs=1024 count=64K
mkfs.ext2 -b 1024 ./<nome_da_imagem.img>
```

2. Verificando a integridade de um sistema EXT2:

```bash
e2fsck <nome_da_imagem.img>
```

3. Montando a imagem do volume com EXT2:

```bash
sudo mount <nome_da_imagem.img> /mnt
```

4. Verificando estrutura original de arquivos do volume (comando `tree` via _bash_):

```bash
/
├── [1.0K]  documentos
│   ├── [1.0K]  emptydir
│   ├── [9.2K]  alfabeto.txt
│   └── [   0]  vazio.txt
├── [1.0K]  imagens
│   ├── [8.1M]  one_piece.jpg
│   ├── [391K]  saber.jpg
│   └── [ 11M]  toscana_puzzle.jpg
├── [1.0K]  livros
│   ├── [1.0K]  classicos
│   │   ├── [506K]  A Journey to the Centre of the Earth - Jules Verne.txt
│   │   ├── [409K]  Dom Casmurro - Machado de Assis.txt
│   │   ├── [861K]  Dracula-Bram_Stoker.txt
│   │   ├── [455K]  Frankenstein-Mary_Shelley.txt
│   │   └── [232K]  The Worderful Wizard of Oz - L. Frank Baum.txt
│   └── [1.0K]  religiosos
│       └── [3.9M]  Biblia.txt
├── [ 12K]  lost+found
└── [  29]  hello.txt
```

5. Verificando informações de espaço (comando `df` via _bash_):

```bash
Blocos de 1k: 62186
Usado: 26777 KiB
Disponível: 32133 KiB
Desmontando a imagem do volume com ext2:
```

6. Desmontando a imagem do volume com ext2:

```shell
sudo umount /mnt
```

---

## 📋 Requisitos

- GCC
- Make

---

## 👥 Contribuidores

- Artur Bento de Carvalho
- Eduardo Riki Matushita
- Rafaela Tieri Iwamoto Ferreira
- Tiago Defendi da Silva
