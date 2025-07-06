# Testes de comandos

Um guia dinâmico para verificar se todos os comandos implementados para o sistema de arquivos **_EXT2_** estão funcionando corretamente.

## Comandos Disponíveis

- `info` &mdash; Exibe informações do disco e do sistema de arquivos
- `ls` &mdash; Lista os arquivos e diretórios do diretório corrente
- `cd` &mdash; Altera o diretório corrente para o definido como `<path>`
- `pwd` &mdash; Exibe o diretório corrente (caminho absoluto)
- `cat` &mdash; Exibe o conteúdo de um arquivo `<file>` no formato texto
- `attr` &mdash; Exibe os atributos de um arquivo (`<file>`) ou diretório (`<dir>`)
- `touch` &mdash; Cria o arquivo `<file>` com conteúdo vazio
- `mkdir` &mdash; Cria o diretório `<dir>` vazio
- `rm` &mdash; Remove o arquivo `<file>` do sistema
- `rmdir` &mdash; Remove o diretório `<dir>`, se estiver vazio
- `rename` &mdash; Renomeia arquivo `<file>` para `<newfilename>`
- `cp` &mdash; Copia um arquivo de origem (`<source_path>`) para destino (`<target_path>`)
- `mv` &mdash; Move um arquivo da imagem EXT2 para o host (remove após copiar)
- `print` &mdash; Exibe informações do sistema EXT2

## Testes realizados

- ### attr

  - [x] `attr <file>` ✔️
  - [x] `attr <file_path_absolute>` ✔️
  - [x] `attr <dir>` ✔️
  - [x] `attr <dir_path_absolute>` ✔️
  - [x] `attr` - sintaxe inválida ❌
  - [x] `attr <x> <y>` - sintaxe inválida ❌
  - [x] `attr <not_exist>` - arquivo ou diretório não encontrado ❌

- ### cat

  - [x] `cat <file>` ✔️
  - [x] `cat <file_path_absolute>` ✔️
  - [x] `cat <dir>` ✔️
  - [x] `cat <dir_path_absolute>` ✔️
  - [x] `cat` - sintaxe inválida ❌
  - [x] `cat <x> <y>` - sintaxe inválida ❌
  - [x] `cat <not_exist>` - arquivo não encontrado ❌

- ### cd

  - [x] `cd <file>` - diretório não encontrado ❌
  - [x] `cd <file_path_absolute>` - diretório não encontrado ❌
  - [x] `cd <dir>` ✔️
  - [x] `cd <dir_path_absolute>` ✔️
  - [x] `cd` - sintaxe inválida ❌
  - [x] `cd <x> <y>` - sintaxe inválida ❌
  - [x] `cd <not_exist>` - diretório não encontrado ❌

- ### cp

  - [x] `cp <filename> <dir_path_host>` ✔️
  - [x] `cp <file_path_absolute> <dir_path_host>` ✔️
  - [x] `cp <dir> <x>` - arquivo não encontrado ❌
  - [x] `cp` - sintaxe inválida ❌
  - [x] `cp <x>` - sintaxe inválida ❌
  - [x] `cp <x> <y>` - sintaxe inválida ❌
  - [x] `cp <not_exist> <x>` - arquivo não encontrado ❌

- ### info

  - [x] `info` ✔️
  - [x] `info <x>` - sintaxe inválida ❌
  - [x] `info <x> <y>` - sintaxe inválida ❌

- ### ls

  - [x] `ls <file>` - diretório não encontrado ❌
  - [x] `ls <file_path_absolute>` - diretório não encontrado ❌
  - [x] `ls <dir>` ✔️
  - [x] `ls <dir_path_absolute>` ✔️
  - [x] `ls` ✔️
  - [x] `ls <x> <y>` - sintaxe inválida ❌
  - [x] `ls <not_exist>` - diretório não encontrado ❌

- ### mkdir

  - [x] `mkdir <file>` - Falso positivo ✔️
  - [x] `mkdir <file_path_absolute>` - Falso positivo ✔️
  - [x] `mkdir <dir>` ✔️
  - [x] `mkdir <dir_path_absolute>` ✔️
  - [x] `mkdir` - sintaxe inválida ❌
  - [x] `mkdir <x> <y>` - sintaxe inválida ❌
  - [x] `mkdir <file_or_dir_exist>` - arquivo ou diretório já existe ❌

- ### mv

  - [x] `mv <filename> <dir_path_host>` ✔️
  - [x] `mv <file_path_absolute> <dir_path_host>` ✔️
  - [x] `mv <dir> <x>` - arquivo não encontrado ❌
  - [x] `mv` - sintaxe inválida ❌
  - [x] `mv <x>` - sintaxe inválida ❌
  - [x] `mv <x> <y>` - sintaxe inválida ❌
  - [x] `mv <not_exist> <x>` - arquivo não encontrado ❌

- ### print

  - [x] `print superblock` ✔️
  - [x] `print groups` ✔️
  - [x] `print inode` ✔️
  - [x] `print inode <x>` ✔️
  - [x] `print` - sintaxe inválida ❌
  - [x] `print <not_exist>` - sintaxe inválida ❌
  - [x] `print <x> <y> <z>` - sintaxe inválida ❌

- ### pwd

  - [x] `pwd` ✔️
  - [x] `pwd <x>` - sintaxe inválida ❌

- ### rename

  - [x] `rename <filename> <new_filename>` ✔️
  - [x] `rename <filename_path_absolute> <new_filename>` ✔️
  - [x] `rename <dirname> <new_dirname>` ✔️
  - [x] `rename <dirname_path_absolute> <new_dirname>` ✔️
  - [x] `rename` - sintaxe inválida ❌
  - [x] `rename <not_exist> <y>` - arquivo não encontado ❌

- ### rm

  - [x] `rm <file>` ✔️
  - [ ] `rm <file_path_absolute>` - Não implementado ❌
  - [x] `rm <dir>` - arquivo não encontrado ❌
  - [x] `rm <dir_path_absolute>` - arquivo não encontrado ❌
  - [x] `rm` - sintaxe inválida ❌
  - [x] `rm <not_exist>` - arquivo não encontrado ❌

- ### rmdir

  - [x] `rmdir <file>` - diretório não encontrado ❌
  - [x] `rmdir <file_path_absolute>` - diretório não encontrado ❌
  - [x] `rmdir <dir>` ✔️
  - [x] `rmdir <dir_path_absolute>` ✔️
  - [x] `rmdir` - sintaxe inválida ❌
  - [x] `rmdir <x> <y>` - sintaxe inválida ❌
  - [x] `rmdir <not_exist>` - diretório não encontrado ❌

- ### touch

  - [x] `touch <file>` ✔️
  - [x] `touch <file_path_absolute>` ✔️
  - [x] `touch <dir>` - Falso positivo ✔️
  - [x] `touch <dir_path_absolute>` - Falso positivo ✔️
  - [x] `touch` - sintaxe inválida ❌
  - [x] `touch <x> <y>` - sintaxe inválida ❌
  - [x] `touch <file_or_dir_exist>` - arquivo ou diretório já existe ❌
