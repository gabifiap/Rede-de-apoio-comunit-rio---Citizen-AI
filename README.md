# Citizen Assist AI – Rede de Apoio Comunitário (São Paulo)

**Global Solution 2026.2 · Data Structures and Algorithms (DSA) · Linguagem C**

Turma 1CC-FEV · Integrantes: [preencher os nomes]

Protótipo de terminal que cadastra, consulta, pesquisa, atualiza, exclui e ordena pontos de apoio ao cidadão
(hospitais, CAPS, alimentação, acolhimento, animais, mulheres em situação de violência e contatos de emergência)
usando uma **lista encadeada implementada pela equipe**, **busca sequencial** e **Insertion Sort**.

> **Aviso:** protótipo acadêmico. **Não é canal oficial** e não substitui os órgãos públicos. Os dados vêm de páginas
> públicas e podem estar desatualizados; cada registro traz fonte, data de consulta e status de verificação.
> O sistema **não coleta nem armazena dados pessoais**.

---

## 1. Recorte, problema e público-alvo

- **Recorte:** rede de apoio comunitário na cidade de São Paulo (assistência social, saúde, direitos e emergências).
- **Problema:** em situações de dificuldade (emergência de chuva, violência doméstica, dependência química, fome,
  animal doente), a informação sobre *onde ir* está espalhada em vários sites e é difícil de achar.
- **Público-alvo:** moradores que precisam localizar um serviço público perto de casa (principalmente quem tem pouco
  acesso à internet ou pouco tempo) e **servidores/agentes comunitários** que mantêm o catálogo.
- **O que o protótipo prova:** que a informação pode ser organizada, pesquisada por região/categoria e mantida
  (CRUD) com estruturas de dados e algoritmos implementados do zero. Uma versão real usaria dados abertos,
  geolocalização e autenticação própria.

## 2. Como compilar e executar

Requisito: compilador C (gcc/clang). Na pasta do projeto:

```bash
gcc -std=c11 -Wall -Wextra -o citizen_assist main.c
./citizen_assist                      # usa base_rede_apoio_sp.csv
./citizen_assist outro_arquivo.csv    # opcional: outro arquivo (se não existir, inicia com lista vazia)
```

- **Windows (MinGW/MSYS2):** `gcc -std=c11 -o citizen_assist.exe main.c` e `citizen_assist.exe`.
  O programa ajusta o console para UTF-8 automaticamente.
- **Senha do perfil servidor (para testar cadastro, atualização e exclusão): `fiap2026`.**
- Compila sem avisos com `-Wall -Wextra -pedantic`.

### Arquivos

| Arquivo | Conteúdo |
|---|---|
| `main.c` | Código-fonte completo (lista encadeada, busca, ordenação, menu, CSV) |
| `base_rede_apoio_sp.csv` | Base inicial com 92 registros reais, com fonte e data de consulta |
| `rodar_testes.sh` | Script que compila com sanitizers e executa 11 sessões de teste |
| `evidencias_testes.txt` | Saída real dos testes (entradas e resultados observados) |
| `README.md` | Esta documentação |

## 3. Modelo do registro

Cada registro é um **nó** da lista (`struct Servico`).

| Campo | Tipo (C) | Descrição |
|---|---|---|
| `id` | `int` | Identificador único (inteiro positivo) |
| `titulo` | `char[160]` | Nome do serviço e endereço |
| `categoria` | `char[24]` | `saude`, `saude_mental`, `alimentacao`, `acolhimento`, `animais`, `mulher`, `contato` |
| `regiao` | `char[16]` | Norte, Sul, Leste, Oeste, Centro ou Todas (telefones) |
| `bairro` | `char[64]` | Bairro/distrito |
| `descricao` | `char[320]` | O que o serviço oferece |
| `requisitos` | `char[128]` | O que levar/exigências |
| `capacidade` | `int` | Vagas informadas (0 = não informado) |
| `fonte` | `char[320]` | Link ou órgão de origem |
| `data_consulta` | `char[16]` | Data em que a informação foi consultada |
| `status` | `char[100]` | Verificado em fonte oficial ou "confirmar" (ver seção 9) |
| `prox` | `struct Servico *` | Ponteiro para o próximo nó |

## 4. Lista encadeada

```c
typedef struct Servico { /* campos acima */ struct Servico *prox; } Servico;
typedef struct { Servico *inicio; Servico *fim; int total; int alterada; } Lista;
```

Lista **simplesmente encadeada**, com ponteiros para o início e o fim. Memória alocada com `calloc` e liberada
com `free`. Operações:

| Operação | Função | Detalhes |
|---|---|---|
| Inserção | `lista_inserir_fim` | Recusa id duplicado; trata lista vazia (`inicio` = `fim` = novo nó) |
| Percurso | laços `for (p = inicio; p; p = p->prox)` | Usado em busca, listagem, indicadores e gravação |
| Remoção | `lista_remover` | Trata remoção do **primeiro**, do **meio**, do **último** e do **único** nó; atualiza `fim`; libera o nó |
| Liberação | `lista_liberar` | `free` de todos os nós ao encerrar (sem vazamentos) |
| Busca por id | `buscar_id` | Busca sequencial |

## 5. Fluxo de telas, menu e perfis

**1) Tela inicial:** título e a pergunta **"Quem é você?"** → 1) cidadão, 2) servidor público (exige senha), 0) encerrar.

**2) Tela "Como funciona"** (logo depois de escolher o perfil): explica o sistema, lista os **7 tipos de serviço**
com a quantidade de registros e uma descrição de cada um, e traz os avisos (não é canal oficial, sem dados pessoais,
telefones de emergência). O servidor vê uma versão do texto voltada à manutenção do catálogo.

**3) Perfil cidadão (fluxo guiado):**

```
Em qual região de São Paulo você está?      → Norte / Sul / Leste / Oeste / Centro
Que tipo de serviço você procura?           → 7 tipos  ou  8) Todos os tipos de serviço
Resultados - região: Sul | tipo: Saúde mental e dependência (CAPS)
   (lista dos locais encontrados)
O que deseja fazer agora?
 1) Ver detalhes de um registro (digite o id)
 2) Ordenar estes resultados
 3) Buscar por nome ou bairro (texto)
 4) Nova pesquisa (outra região ou tipo)
 5) Ver indicadores do sistema
 0) Voltar ao menu inicial
```

**4) Perfil servidor:** depois da senha, o menu completo:

| Opção | Cidadão (sem senha, sem nome) | Servidor (senha) |
|---|---|---|
| Cadastrar | | ✓ |
| Consultar por id | ✓ | ✓ |
| Pesquisar (título, categoria ou "todos", região, região+categoria, bairro) | ✓ (fluxo guiado e busca por texto) | ✓ |
| Atualizar | | ✓ |
| Excluir (com confirmação) | | ✓ |
| Ordenar (5 critérios, crescente/decrescente) | ✓ | ✓ |
| Indicadores (análise) | ✓ | ✓ |
| Salvar no CSV (extensão opcional) | | ✓ |
| Encerrar / voltar | ✓ | ✓ |

**Tratamentos de erro:** opção inválida de menu, texto onde se espera número, número fora do intervalo,
campo obrigatório vazio, id duplicado, id inexistente, lista vazia, senha incorreta (3 tentativas), entrada
encerrada (EOF) e linhas muito longas.

A pesquisa por **região** também devolve os registros de região "Todas" (telefones de emergência), pois valem para a
cidade inteira. A busca ignora maiúsculas e acentos (`sao miguel` encontra "São Miguel").

## 6. Algoritmos implementados

### 6.1 Busca sequencial (linear)
Percorre a lista nó a nó até achar (ou até o fim). Foi escolhida porque **uma lista encadeada não permite acesso por
índice**, então a busca binária não se aplica. Se não há resultado, o programa avisa e mostra os telefones de
emergência. O programa exibe quantos nós foram visitados.

### 6.2 Ordenação: Insertion Sort adaptado a lista encadeada (`ordenar_insertion`)
Constrói uma nova lista ordenada. Para cada nó da lista original: se ele deve vir antes do primeiro da lista
ordenada, entra no início; caso contrário, avança até achar a posição e é **reencadeado** ali (os dados não são
copiados, só os ponteiros mudam). É **estável** (iguais mantêm a ordem) e não usa memória extra.
Não usa `qsort` nem funções prontas. Critérios: título, id, capacidade, região+bairro e categoria.
O programa mostra o **número de comparações** realizadas.

## 7. Análise de complexidade (Big-O)

`n` = número de registros na lista. Os campos de texto têm tamanho máximo fixo, então comparar/buscar texto dentro de
um nó é tratado como custo constante.

| Operação | Melhor caso | Pior/médio caso | Justificativa |
|---|---|---|---|
| **Inserção** (cadastro) | O(n) | O(n) | A verificação de id duplicado percorre a lista; ligar o nó no fim é O(1) graças ao ponteiro `fim` |
| **Busca por id** | O(1) | O(n) | Melhor: o id está no primeiro nó. Pior: está no último ou não existe |
| **Pesquisa por atributo** | O(n) | O(n) | Sempre percorre a lista inteira para listar todos os resultados |
| **Remoção por id** | O(1) | O(n) | Localizar o nó e o anterior é O(n); desligar e liberar é O(1) |
| **Atualização** | O(1) | O(n) | Custo da busca por id |
| **Ordenação** (insertion sort) | O(n) | O(n²) | Veja abaixo. Espaço extra O(1), estável |
| **Indicadores / listar / salvar** | O(n) | O(n) | Um percurso |
| Carga do CSV | O(n²) | O(n²) | Cada inserção confere duplicidade (n = 92, irrelevante na prática) |

**Ordenação – como foi medido:** como cada nó é procurado a partir do início da lista ordenada,
- **pior caso**: lista que já está na ordem pedida → cada nó percorre toda a parte ordenada → `n(n−1)/2`
  comparações. Observado com n = 92 (ordenar por id crescente uma lista já em ordem crescente): **4186** = 92·91/2;
- **melhor caso**: lista na ordem *inversa* da pedida → cada nó entra no início com 1 comparação → `n−1`.
  Observado: ordenar por id decrescente a lista crescente: **91** = n−1;
- caso médio (dados embaralhados): O(n²). Observado: 3238 comparações por capacidade e 2412 por título.

Para bases muito maiores, Merge Sort (O(n log n)) ou uma estrutura indexada (árvore/hash) seriam mais adequados;
aqui o Insertion Sort foi escolhido por ser simples de explicar e adequado a n < 100.

## 8. Testes e evidências

O script `rodar_testes.sh` compila o programa com **AddressSanitizer e UBSan** e executa as sessões abaixo. O resultado
completo (cada entrada digitada e cada saída) está em `evidencias_testes.txt`. **Nenhum erro de memória foi
detectado em nenhum teste.**

| Teste | Entrada | Resultado esperado | Resultado observado |
|---|---|---|---|
| T01 | Opção `9`, `abc`, Enter no menu inicial | Recusa e pede de novo | "Opção inválida" nos três casos |
| T02 | Cidadão: Norte + Todos os tipos; consultar id `101`, `99999`, `abc`, `-5`, Enter | Mostra o registro; avisa inexistente; recusa texto e fora do intervalo; Enter cancela | 26 resultados; registro 101 (1 nó visitado); "Nenhum registro com id 99999 (92 nós)"; "digite apenas números"; "fora do intervalo (1 a 999999)" |
| T03 | Cidadão: Norte + saúde mental; Oeste + animais; **Centro + animais**; texto `sao miguel`; texto `butanta` | 4; 1; nenhum resultado; busca sem acento; busca sem acento | 4 resultados; 1 resultado; "Nenhum resultado… 190/192/193/199"; 5 resultados com São Miguel; 3 resultados com Butantã |
| T04 | Cidadão: Leste + Todos; ordenar por id (já ordenado), id decrescente, capacidade decrescente, título | Contagem de comparações coerente com o Big-O; resultados reexibidos ordenados | 4186 (pior), 91 (melhor), 3238 e 2412 comparações |
| T05 | Cidadão: Norte + Todos, depois indicadores | Totais por categoria/região | 26 resultados; indicadores: 92 registros, 3 a confirmar |
| T06 | Servidor: senha errada 3× e depois correta | Bloqueia após 3 erros; depois libera | "Número máximo de tentativas…"; depois "Acesso liberado" |
| T07 | Cadastro com id duplicado `101`, id `abc`, capacidade `-5` e `abc`, depois dados válidos | Recusa os inválidos; cadastra o válido | "já existe um registro com id 101"; erros de entrada; "Registro 500 cadastrado (total: 93)" |
| T08 | Atualizar id `99999`; atualizar `101` (capacidade 12) | Avisa inexistente; atualiza | "Nenhum registro com id 99999"; capacidade passa de "não informada" a "12 vagas" |
| T09 | Excluir primeiro (101), último (193), meio (140), inexistente, cancelada; depois cadastrar após excluir o último | Lista íntegra, total correto, sem erro de memória | Restam 91, 90, 89; novo cadastro (total 90); listagem por id com 90 registros; sanitizers sem erros |
| T10 | Arquivo inexistente (**lista vazia**): entrar como cidadão; como servidor, atualizar, excluir e listar; cadastrar 1 e excluir o único nó; excluir de novo | Avisa lista vazia em todas; permite cadastrar e remover o único nó | "A lista está vazia…" em 5 operações; cadastro e exclusão do único nó funcionam |
| T11 | Servidor: cadastrar, salvar no CSV; reabrir como cidadão (Sul + acolhimento) e consultar o id 500 | Dados persistem | Reabre com 93 registros; registro 500 recuperado |

## 9. Dados: origem, qualidade e limitações

- **92 registros** de São Paulo, coletados em páginas oficiais e conferidos em **11/11/2026**: hospitais municipais, CAPS por
  território, Centros de Acolhida e Centros Pop (SMADS), Casas da Mulher (Secretaria de Direitos Humanos), hospitais
  veterinários públicos (Secretaria da Saúde) e o Relatório de Pontos de Venda do Bom Prato (Secretaria de
  Desenvolvimento Social). Cada registro traz o link da fonte.
- **Verificação:** os endereços do Bom Prato, das Casas da Mulher, dos Centros Pop, do hospital veterinário Leste II, do CTA
  Brigadeiro Galvão e dos telefones de emergência foram conferidos em páginas oficiais. Isso corrigiu o cadastro inicial:
  3 endereços do Bom Prato (Tucuruvi, Guaianases e Grajaú/Cidade Dutra), 2 unidades trocadas (Limão e Capão Redondo
  por Vila Nova Cachoeirinha e Campo Limpo), 1 removida (Campos Elíseos, com dois endereços diferentes), o endereço da
  Casa da Mulher Maria de Lourdes Rodrigues, a Casa da Mulher 25 de Março (agora na Zona Oeste) e o Centro Pop Santa Cecília.
- **Ainda a conferir (3 registros):** os CTAs Vila Mariana e Santo Amaro aparecem na página oficial de CTAs da SMADS, mas são
  de 2017 e não foi possível confirmar o funcionamento atual; e a Casa da Mulher da Zona Leste (R. Ibiajara, 495), cuja
  página oficial traz nomes diferentes (Itaquera e São Miguel). O indicador do programa mostra essa contagem.
- **Cobertura:** até 4 serviços por região onde a rede existe em volume (CAPS, hospitais, Bom Prato, acolhimento, Casas da
  Mulher no Sul). Animais têm só 5 hospitais na cidade; a Zona Oeste tem 1 unidade de alimentação e 1 Casa da Mulher.
- **Abrigos de emergência de chuva:** não existe lista pública fixa; por isso a categoria é **acolhimento** (Centros de
  Acolhida e Centros Pop da Prefeitura, que atendem pessoas em situação de rua por encaminhamento).
- **Grupos de AA** não foram incluídos (não foi possível obter uma lista verificável); a categoria de dependência é
  coberta pelos CAPS AD.
- **Privacidade e segurança:** endereços sigilosos de casas-abrigo **não** constam. Não há dados de pessoas.
- **Capacidade:** só os CAPS AD têm valor (vagas de acolhimento noturno); nos demais, 0 = não informado.

### Limitações do protótipo
- Catálogo curado manualmente e restrito à cidade de São Paulo; o cidadão escolhe a região (não há GPS).
- A senha do servidor é fixa no código; um sistema real teria autenticação individual e registro de quem alterou.
- Terminal apenas; uma versão real seria web/mobile com dados abertos atualizados automaticamente.

## 10. Roteiro da demonstração

1. Compilar e abrir o programa; mostrar a pergunta "Quem é você?" e a carga de 92 registros.
2. **Cidadão:** ler a tela "Como funciona"; escolher região Norte + saúde mental (4 resultados); ver detalhes de um id;
   fazer nova pesquisa Centro + animais (sem resultado e telefones de emergência); testar a opção "Todos".
3. **Ordenação:** ordenar por id crescente (4186 comparações) e depois decrescente (91) para mostrar pior e melhor caso.
4. **Servidor:** errar a senha 3 vezes; entrar com a senha correta; cadastrar (testar id duplicado e capacidade inválida);
   atualizar; excluir; salvar no CSV.
5. Mostrar `evidencias_testes.txt` e o trecho do código de `lista_remover` e `ordenar_insertion`.
