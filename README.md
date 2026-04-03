# Guia do utilizador — BD simulado (cliente/servidor)

Este guia é para quem vai compilar e testar o projeto (professor, monitor ou colega).

Para o enunciado completo do trabalho, consulte também o ficheiro **`README.pdf`** na pasta pai do projeto (`../README.pdf` quando está na mesma árvore de pastas do zip).

---

## 1. O que precisa instalado

- **Linux** (laboratório, VM) ou **WSL** com Ubuntu.
- Ferramentas: **`gcc`**, **`make`**.

Ubuntu/WSL:

```bash
sudo apt install build-essential
```

**Importante:** rode o projeto numa pasta no **disco Linux** (ex.: `~/trabalho-SO/bd_simulado`).  
Em pastas do Windows montadas no WSL (`/mnt/c/...`, OneDrive), o comando **`mkfifo`** costuma falhar.

---

## 2. Compilar

No terminal, dentro da pasta `bd_simulado`:

```bash
make
```

Devem aparecer os executáveis **`servidor`** e **`cliente`** (sem erros).

Para limpar executáveis e FIFOs antigos:

```bash
make clean
```

---

## 3. Executar (sempre dois terminais)

### Terminal 1 — servidor

```bash
cd /caminho/para/bd_simulado
./servidor
```

**Mensagens esperadas no stderr (ordem aproximada):**

1. `Servidor à espera do cliente. Noutro terminal: ./cliente`
2. Depois de abrir o cliente: `Cliente ligado. Servidor a processar pedidos (Ctrl+D no cliente ou QUIT encerra).`

### Terminal 2 — cliente (na mesma pasta)

```bash
cd /caminho/para/bd_simulado
./cliente
```

Escreva **um comando por linha** e prima **Enter**. Para cada linha, o cliente mostra a resposta do servidor.

---

## 4. Comandos aceites (sintaxe)

| Comando | Exemplo | Notas |
|--------|---------|--------|
| **INSERT** | `INSERT id=5 nome=Maria` | `nome=` pode ter espaços até ao fim da linha. |
| **DELETE** | `DELETE id=5` | Remove o registo com esse `id`. |
| **SELECT** | `SELECT WHERE id=5` | Também pode usar `SELECT ...` com `WHERE id=N` noutra posição; o servidor procura `id=` após `WHERE` se existir (ex.: `SELECT nome WHERE id=5`). |
| **UPDATE** | `UPDATE id=5 nome=Maria Silva` | Altera o nome do registo. |
| **QUIT** | `QUIT` | Avisa o servidor para terminar o fluxo; feche o cliente depois se quiser. |

### Formato das respostas (uma linha)

- **Sucesso:** `OK|INSERT`, `OK|DELETE`, `OK|UPDATE`, `OK|SELECT|...`, `OK|QUIT`
- **Erro:** mensagens começadas por `ERR|...`
- **SELECT sem registo:** `OK|SELECT|vazio`

---

## 5. Ficheiro de dados

- **`banco.txt`** — tabela simulada em texto, uma linha por registo: `id|nome`
- É atualizado pelo servidor após **INSERT**, **DELETE** e **UPDATE** (na pasta onde o servidor corre).

---

## 6. Problemas frequentes

| Sintoma | O que fazer |
|--------|-------------|
| `mkfifo: Operation not supported` | Não use pasta em `/mnt/c/`. Copie o projeto para `~/...` no WSL. |
| Cliente diz erro ao abrir FIFO | Inicie primeiro o `./servidor` no **mesmo diretório**. |
| `make: command not found` | Instale `build-essential` (Linux) ou use um ambiente com `make` e `gcc`. |

---

## 7. Ordem correta

1. `make`
2. `./servidor` (terminal 1)
3. `./cliente` (terminal 2, mesma pasta)
4. Testar comandos; **QUIT** quando quiser encerrar o fluxo de pedidos.

---

*Projeto académico — Sistemas Operacionais (IPC, processos, threads, mutex).*
