/*
 * Rede de Apoio Comunitário · Citizen Assist AI
 * Global Solution 2026.2 - Data Structures and Algorithms (DSA)
 *
 * Prototipo de terminal que organiza servicos de apoio ao cidadao
 * (saude, saude mental, alimentacao, acolhimento, animais, mulher e
 * contatos de emergencia) usando uma LISTA ENCADEADA implementada
 * , busca sequencial e ordenacao por INSERTION SORT.
 *
 * AVISO: este sistema NAO e um canal oficial e nao substitui os
 * orgaos publicos. Os dados vem de paginas publicas e podem estar
 * desatualizados (veja a coluna fonte/status no CSV).
 *
 * Compilar:  gcc -std=c11 -Wall -Wextra -o citizen_assist main.c
 * Executar:  ./citizen_assist [arquivo.csv]
 */

#define _POSIX_C_SOURCE 200809L   /* isatty/fileno com -std=c11 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define ISATTY _isatty
#define FILENO _fileno
#else
#include <unistd.h>
#define ISATTY isatty
#define FILENO fileno
#endif

/* ------------------------------------------------------------------ */
/* Constantes                                                          */
/* ------------------------------------------------------------------ */
#define ARQUIVO_PADRAO   "base_rede_apoio_sp.csv"
#define SENHA_SERVIDOR   "fiap2026"   /* senha do prototipo (documentada no README) */
#define MAX_TENTATIVAS   3

#define T_TITULO 160
#define T_CAT    24
#define T_REG    16
#define T_BAIRRO 64
#define T_DESC   320
#define T_REQ    128
#define T_FONTE  320
#define T_DATA   16
#define T_STATUS 100
#define T_LINHA  2048

#define N_CATS 7
#define N_REGS 6          /* Norte, Sul, Leste, Oeste, Centro, Todas */
#define N_REGS_BUSCA 5    /* regioes que o cidadao pode escolher      */

static const char *CATS[N_CATS] = {
    "saude", "saude_mental", "alimentacao", "acolhimento",
    "animais", "mulher", "contato"
};
static const char *CAT_ROTULO[N_CATS] = {
    "Saúde (hospitais)", "Saúde mental e dependência (CAPS)", "Alimentação",
    "Acolhimento", "Animais", "Mulher (violência)", "Contatos de emergência"
};
/* Opcoes de categoria para a pesquisa: as 7 categorias + "Todos" (indice N_CATS) */
static const char *CAT_ROTULO_TODOS[N_CATS + 1] = {
    "Saúde (hospitais)", "Saúde mental e dependência (CAPS)", "Alimentação",
    "Acolhimento", "Animais", "Mulher (violência)", "Contatos de emergência",
    "Todos os tipos de serviço"
};
static const char *CAT_DESC[N_CATS] = {
    "Hospitais municipais (SUS) com atendimento gratuito.",
    "CAPS Álcool e Drogas: atendimento sem agendamento e acolhimento noturno.",
    "Restaurantes Bom Prato: refeições subsidiadas.",
    "Centros de Acolhida e Centros Pop para pessoas em situação de rua.",
    "Hospitais veterinários públicos para cães e gatos.",
    "Centros de referência e Casa da Mulher Brasileira: apoio em casos de violência.",
    "Telefones de emergência: 190, 192, 193, 199, 180, 188 e 156."
};
static const char *REGS[N_REGS] = { "Norte", "Sul", "Leste", "Oeste", "Centro", "Todas" };

/* ------------------------------------------------------------------ */
/* Estruturas de dados                                                 */
/* ------------------------------------------------------------------ */

/* Registro de um servico/ponto de apoio = um NO da lista encadeada */
typedef struct Servico {
    int  id;                       /* identificador unico            */
    char titulo[T_TITULO];
    char categoria[T_CAT];
    char regiao[T_REG];
    char bairro[T_BAIRRO];
    char descricao[T_DESC];
    char requisitos[T_REQ];
    int  capacidade;               /* vagas informadas (0 = nao informado) */
    char fonte[T_FONTE];
    char data_consulta[T_DATA];
    char status[T_STATUS];
    struct Servico *prox;          /* ponteiro para o proximo no     */
} Servico;

/* Lista simplesmente encadeada com ponteiro para o inicio e para o fim */
typedef struct {
    Servico *inicio;
    Servico *fim;
    int total;
    int alterada;                  /* 1 se houve cadastro/edicao/exclusao */
} Lista;

static Lista g_lista;              /* lista global do programa           */
static char  g_arquivo[256] = ARQUIVO_PADRAO;
static long  g_passos = 0;         /* instrumentacao: comparacoes/nos visitados */

/* ------------------------------------------------------------------ */
/* Utilitarios de texto                                                */
/* ------------------------------------------------------------------ */

/* Converte uma letra acentuada UTF-8 (segundo byte apos 0xC3) na letra base */
static char base_latina(unsigned char d) {
    if ((d >= 0x80 && d <= 0x85) || (d >= 0xA0 && d <= 0xA5)) return 'a';
    if (d == 0x87 || d == 0xA7) return 'c';
    if ((d >= 0x88 && d <= 0x8B) || (d >= 0xA8 && d <= 0xAB)) return 'e';
    if ((d >= 0x8C && d <= 0x8F) || (d >= 0xAC && d <= 0xAF)) return 'i';
    if (d == 0x91 || d == 0xB1) return 'n';
    if ((d >= 0x92 && d <= 0x96) || (d >= 0xB2 && d <= 0xB6)) return 'o';
    if ((d >= 0x99 && d <= 0x9C) || (d >= 0xB9 && d <= 0xBC)) return 'u';
    return '?';
}

/* Minusculas e sem acento: permite buscar "saude" e achar "Saúde" */
static void normalizar(const char *src, char *dst, size_t n) {
    size_t j = 0;
    for (size_t i = 0; src[i] != '\0' && j + 1 < n; ) {
        unsigned char c = (unsigned char)src[i];
        if (c == 0xC3 && src[i + 1] != '\0') {
            dst[j++] = base_latina((unsigned char)src[i + 1]);
            i += 2;
        } else {
            dst[j++] = (char)tolower(c);
            i++;
        }
    }
    dst[j] = '\0';
}

static int contem(const char *texto, const char *trecho) {
    char a[T_LINHA], b[T_LINHA];
    normalizar(texto, a, sizeof a);
    normalizar(trecho, b, sizeof b);
    return strstr(a, b) != NULL;
}

static int iguais(const char *x, const char *y) {
    char a[T_LINHA], b[T_LINHA];
    normalizar(x, a, sizeof a);
    normalizar(y, b, sizeof b);
    return strcmp(a, b) == 0;
}

static int cmp_texto(const char *x, const char *y) {
    char a[T_LINHA], b[T_LINHA];
    normalizar(x, a, sizeof a);
    normalizar(y, b, sizeof b);
    return strcmp(a, b);
}

static void copiar(char *dst, size_t n, const char *src) {
    snprintf(dst, n, "%s", src);
}

static const char *rotulo_categoria(const char *cat) {
    for (int i = 0; i < N_CATS; i++)
        if (strcmp(cat, CATS[i]) == 0) return CAT_ROTULO[i];
    return cat;
}

/* ------------------------------------------------------------------ */
/* Lista encadeada: criar, inserir, buscar, remover, liberar           */
/* ------------------------------------------------------------------ */

static void lista_iniciar(Lista *l) {
    l->inicio = NULL;
    l->fim = NULL;
    l->total = 0;
    l->alterada = 0;
}

/* Libera todos os nos (free) - evita vazamento de memoria. O(n) */
static void lista_liberar(Lista *l) {
    Servico *atual = l->inicio;
    while (atual != NULL) {
        Servico *prox = atual->prox;
        free(atual);
        atual = prox;
    }
    lista_iniciar(l);
}

/* Busca sequencial por id. Conta os nos visitados em *visitados. O(n) */
static Servico *buscar_id(const Lista *l, int id, long *visitados) {
    long v = 0;
    for (Servico *p = l->inicio; p != NULL; p = p->prox) {
        v++;
        if (p->id == id) {
            if (visitados) *visitados = v;
            return p;
        }
    }
    if (visitados) *visitados = v;
    return NULL;
}

/* Insere no fim. Retorna 0 se ok, -1 se o id ja existe. O(n) por causa da
 * verificacao de duplicidade (a insercao em si e O(1) com ponteiro "fim"). */
static int lista_inserir_fim(Lista *l, Servico *novo) {
    if (buscar_id(l, novo->id, NULL) != NULL) return -1;
    novo->prox = NULL;
    if (l->fim == NULL) {           /* lista vazia */
        l->inicio = novo;
        l->fim = novo;
    } else {
        l->fim->prox = novo;
        l->fim = novo;
    }
    l->total++;
    return 0;
}

/* Remove o no com o id informado. Trata inicio, meio, fim e no unico.
 * Retorna 1 se removeu, 0 se nao encontrou. O(n) */
static int lista_remover(Lista *l, int id) {
    Servico *anterior = NULL;
    Servico *atual = l->inicio;
    while (atual != NULL && atual->id != id) {
        anterior = atual;
        atual = atual->prox;
    }
    if (atual == NULL) return 0;

    if (anterior == NULL) l->inicio = atual->prox;   /* removendo o primeiro */
    else anterior->prox = atual->prox;               /* meio ou ultimo       */
    if (atual == l->fim) l->fim = anterior;          /* atualiza o fim       */

    free(atual);
    l->total--;
    return 1;
}

static Servico *novo_servico(void) {
    Servico *s = (Servico *)calloc(1, sizeof(Servico));
    if (s == NULL) {
        fprintf(stderr, "Erro: memória insuficiente.\n");
        exit(1);
    }
    return s;
}

/* ------------------------------------------------------------------ */
/* Ordenacao: INSERTION SORT adaptado para lista encadeada            */
/* ------------------------------------------------------------------ */

typedef int (*Comparador)(const Servico *, const Servico *);

static int cmp_id(const Servico *a, const Servico *b)        { return (a->id > b->id) - (a->id < b->id); }
static int cmp_titulo(const Servico *a, const Servico *b)    { return cmp_texto(a->titulo, b->titulo); }
static int cmp_capac(const Servico *a, const Servico *b)     { return (a->capacidade > b->capacidade) - (a->capacidade < b->capacidade); }
static int cmp_categoria(const Servico *a, const Servico *b) { return cmp_texto(a->categoria, b->categoria); }
static int cmp_regiao(const Servico *a, const Servico *b) {
    int c = cmp_texto(a->regiao, b->regiao);
    return c != 0 ? c : cmp_texto(a->bairro, b->bairro);
}

/* "a deve vir antes de b?"  (conta uma comparacao) */
static int precede(const Servico *a, const Servico *b, Comparador cmp, int decrescente) {
    g_passos++;
    int c = cmp(a, b);
    return decrescente ? (c > 0) : (c < 0);
}

/* Reencadeia os nos (nao copia dados) numa nova lista ja ordenada.
 * E estavel: elementos iguais mantem a ordem original.
 * Tempo: O(n^2) pior/medio caso; espaco extra: O(1). */
static void ordenar_insertion(Lista *l, Comparador cmp, int decrescente) {
    Servico *ordenada = NULL;
    Servico *atual = l->inicio;

    while (atual != NULL) {
        Servico *prox = atual->prox;                 /* guarda o proximo a processar */

        if (ordenada == NULL || precede(atual, ordenada, cmp, decrescente)) {
            atual->prox = ordenada;                  /* entra no inicio */
            ordenada = atual;
        } else {
            Servico *p = ordenada;                   /* procura a posicao */
            while (p->prox != NULL && !precede(atual, p->prox, cmp, decrescente))
                p = p->prox;
            atual->prox = p->prox;                   /* entra depois de p */
            p->prox = atual;
        }
        atual = prox;
    }

    l->inicio = ordenada;
    l->fim = ordenada;                               /* recalcula o fim */
    while (l->fim != NULL && l->fim->prox != NULL) l->fim = l->fim->prox;
}

/* ------------------------------------------------------------------ */
/* Entrada de dados com validacao                                      */
/* ------------------------------------------------------------------ */

static void encerrar_por_fim_de_entrada(void) {
    printf("\n[Entrada encerrada. Finalizando o programa.]\n");
    lista_liberar(&g_lista);
    exit(0);
}

/* Le uma linha, remove \n e espacos nas pontas. Descarta o excesso. */
static void ler_linha(const char *prompt, char *dst, size_t n) {
    printf("%s", prompt);
    fflush(stdout);
    if (fgets(dst, (int)n, stdin) == NULL) encerrar_por_fim_de_entrada();

    size_t len = strlen(dst);
    if (len > 0 && dst[len - 1] == '\n') {
        dst[--len] = '\0';
    } else {
        int c;                                       /* linha maior que o buffer */
        while ((c = getchar()) != '\n' && c != EOF) { }
    }
    while (len > 0 && isspace((unsigned char)dst[len - 1])) dst[--len] = '\0';
    size_t ini = 0;
    while (dst[ini] != '\0' && isspace((unsigned char)dst[ini])) ini++;
    if (ini > 0) memmove(dst, dst + ini, strlen(dst + ini) + 1);

    /* Se a entrada vem de arquivo/pipe (testes automatizados), repete o que
     * foi "digitado" para o registro de teste ficar legivel. */
    static int eco = -1;
    if (eco < 0) eco = !ISATTY(FILENO(stdin));
    if (eco) printf("%s\n", dst);
}

/* Le inteiro em [min,max]. Retorna 1 se leu, 0 se o usuario deu so Enter. */
static int ler_int(const char *prompt, long min, long max, long *out) {
    char buf[64];
    for (;;) {
        ler_linha(prompt, buf, sizeof buf);
        if (buf[0] == '\0') return 0;
        char *fim;
        long v = strtol(buf, &fim, 10);
        if (*fim != '\0') {
            printf("  Entrada inválida: digite apenas números.\n");
        } else if (v < min || v > max) {
            printf("  Valor fora do intervalo (%ld a %ld).\n", min, max);
        } else {
            *out = v;
            return 1;
        }
    }
}

/* Le texto obrigatorio (repete ate nao ser vazio) */
static void ler_obrigatorio(const char *prompt, char *dst, size_t n) {
    for (;;) {
        ler_linha(prompt, dst, n);
        if (dst[0] != '\0') return;
        printf("  Campo obrigatório.\n");
    }
}

/* Mostra opcoes numeradas e devolve o indice escolhido (0..n-1),
 * ou -1 se o usuario der so Enter. */
static int escolher(const char *titulo, const char *const *ops, int n) {
    printf("%s\n", titulo);
    for (int i = 0; i < n; i++) printf("  %d) %s\n", i + 1, ops[i]);
    long v;
    if (!ler_int("Opção (Enter cancela): ", 1, n, &v)) return -1;
    return (int)v - 1;
}

/* ------------------------------------------------------------------ */
/* Exibicao                                                            */
/* ------------------------------------------------------------------ */

static void imprimir_linha(const Servico *s) {
    printf("%4d | %-12s | %-6s | %s (%s)\n",
           s->id, s->categoria, s->regiao, s->titulo, s->bairro);
}

static void imprimir_detalhe(const Servico *s) {
    printf("----------------------------------------------------------\n");
    printf("Id:           %d\n", s->id);
    printf("Título:       %s\n", s->titulo);
    printf("Categoria:    %s\n", rotulo_categoria(s->categoria));
    printf("Região:       %s\n", s->regiao);
    printf("Bairro:       %s\n", s->bairro);
    printf("Descrição:    %s\n", s->descricao);
    printf("Requisitos:   %s\n", s->requisitos);
    if (s->capacidade > 0) printf("Capacidade:   %d vagas\n", s->capacidade);
    else                   printf("Capacidade:   não informada\n");
    printf("Fonte:        %s\n", s->fonte);
    printf("Consultado em:%s\n", s->data_consulta);
    printf("Verificação:  %s\n", s->status);
    printf("----------------------------------------------------------\n");
}

static void aviso_emergencia(void) {
    printf("Em emergência ligue: 190 (Polícia), 192 (SAMU), 193 (Bombeiros), 199 (Defesa Civil).\n");
}

static int lista_vazia_aviso(void) {
    if (g_lista.inicio == NULL) {
        printf("A lista está vazia: não há registros cadastrados.\n");
        return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Persistencia em CSV (extensao opcional)                             */
/* ------------------------------------------------------------------ */

/* Separa uma linha CSV em campos, tratando aspas. Altera a linha. */
static int separar_csv(char *linha, char *campos[], int max) {
    char *r = linha, *w = linha;
    int n = 0, dentro = 0;
    campos[n++] = w;
    while (*r != '\0') {
        if (dentro) {
            if (*r == '"') {
                if (r[1] == '"') { *w++ = '"'; r += 2; }
                else { dentro = 0; r++; }
            } else {
                *w++ = *r++;
            }
        } else if (*r == '"') {
            dentro = 1; r++;
        } else if (*r == ',') {
            *w++ = '\0'; r++;
            if (n >= max) break;
            campos[n++] = w;
        } else {
            *w++ = *r++;
        }
    }
    *w = '\0';
    return n;
}

static int carregar_csv(const char *caminho) {
    FILE *f = fopen(caminho, "r");
    if (f == NULL) return -1;

    char linha[T_LINHA];
    int lidos = 0, ignorados = 0, numero = 0;

    while (fgets(linha, sizeof linha, f) != NULL) {
        numero++;
        char *l = linha;
        if (numero == 1) {                               /* pula BOM e cabecalho */
            continue;
        }
        size_t len = strlen(l);
        while (len > 0 && (l[len - 1] == '\n' || l[len - 1] == '\r')) l[--len] = '\0';
        if (len == 0) continue;

        char *c[11];
        if (separar_csv(l, c, 11) != 11) { ignorados++; continue; }

        char *fim1, *fim2;
        long id = strtol(c[0], &fim1, 10);
        long cap = strtol(c[7], &fim2, 10);
        if (*fim1 != '\0' || *fim2 != '\0' || id <= 0 || cap < 0) { ignorados++; continue; }

        Servico *s = novo_servico();
        s->id = (int)id;
        copiar(s->titulo, sizeof s->titulo, c[1]);
        copiar(s->categoria, sizeof s->categoria, c[2]);
        copiar(s->regiao, sizeof s->regiao, c[3]);
        copiar(s->bairro, sizeof s->bairro, c[4]);
        copiar(s->descricao, sizeof s->descricao, c[5]);
        copiar(s->requisitos, sizeof s->requisitos, c[6]);
        s->capacidade = (int)cap;
        copiar(s->fonte, sizeof s->fonte, c[8]);
        copiar(s->data_consulta, sizeof s->data_consulta, c[9]);
        copiar(s->status, sizeof s->status, c[10]);

        if (lista_inserir_fim(&g_lista, s) != 0) {      /* id duplicado no arquivo */
            free(s);
            ignorados++;
        } else {
            lidos++;
        }
    }
    fclose(f);
    printf("Base carregada de \"%s\": %d registros (%d linhas ignoradas).\n",
           caminho, lidos, ignorados);
    return lidos;
}

static void escrever_campo(FILE *f, const char *txt) {
    fputc('"', f);
    for (const char *p = txt; *p != '\0'; p++) {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

/* Grava num arquivo temporario e depois renomeia (evita perder dados) */
static int salvar_csv(const char *caminho) {
    char tmp[300];
    snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
    FILE *f = fopen(tmp, "w");
    if (f == NULL) return -1;

    fputs("\xEF\xBB\xBF" "id,titulo,categoria,regiao,bairro,descricao,requisitos,capacidade,fonte,data_consulta,status_verificacao\r\n", f);
    for (Servico *s = g_lista.inicio; s != NULL; s = s->prox) {
        fprintf(f, "%d,", s->id);
        escrever_campo(f, s->titulo);     fputc(',', f);
        escrever_campo(f, s->categoria);  fputc(',', f);
        escrever_campo(f, s->regiao);     fputc(',', f);
        escrever_campo(f, s->bairro);     fputc(',', f);
        escrever_campo(f, s->descricao);  fputc(',', f);
        escrever_campo(f, s->requisitos); fputc(',', f);
        fprintf(f, "%d,", s->capacidade);
        escrever_campo(f, s->fonte);      fputc(',', f);
        escrever_campo(f, s->data_consulta); fputc(',', f);
        escrever_campo(f, s->status);
        fputs("\r\n", f);
    }
    if (fclose(f) != 0) return -1;
    remove(caminho);
    if (rename(tmp, caminho) != 0) return -1;
    g_lista.alterada = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Operacoes do menu                                                   */
/* ------------------------------------------------------------------ */

static void op_consultar_id(void) {
    if (lista_vazia_aviso()) return;
    long id;
    if (!ler_int("Id do registro (Enter cancela): ", 1, 999999, &id)) return;
    long visitados;
    Servico *s = buscar_id(&g_lista, (int)id, &visitados);
    if (s == NULL) {
        printf("Nenhum registro com id %ld. (nós visitados: %ld)\n", id, visitados);
        printf("Dica: use a pesquisa por região ou categoria.\n");
    } else {
        imprimir_detalhe(s);
        printf("(busca sequencial: %ld nó(s) visitado(s) de %d)\n", visitados, g_lista.total);
    }
}

/* Mostra os resultados de uma pesquisa; filtro: 1=titulo 2=categoria 3=regiao
 * 4=regiao+categoria 5=bairro. Busca sequencial: percorre a lista toda. O(n) */
static int pesquisar(int modo, const char *texto, int idx_cat, int idx_reg) {
    long visitados = 0;
    int achados = 0;
    for (Servico *p = g_lista.inicio; p != NULL; p = p->prox) {
        visitados++;
        int ok = 0;
        int reg_ok = (idx_reg >= 0) &&
                     (iguais(p->regiao, REGS[idx_reg]) || iguais(p->regiao, "Todas"));
        int cat_ok = (idx_cat == N_CATS) ||                  /* "Todos os tipos" */
                     (idx_cat >= 0 && idx_cat < N_CATS && strcmp(p->categoria, CATS[idx_cat]) == 0);
        switch (modo) {
            case 1: ok = contem(p->titulo, texto); break;
            case 2: ok = cat_ok; break;
            case 3: ok = reg_ok; break;
            case 4: ok = reg_ok && cat_ok; break;
            case 5: ok = contem(p->bairro, texto); break;
            case 6: ok = contem(p->titulo, texto) || contem(p->bairro, texto); break;
        }
        if (ok) {
            if (achados == 0) printf("  id | categoria    | região | título (bairro)\n");
            imprimir_linha(p);
            achados++;
        }
    }
    if (achados == 0) {
        printf("Nenhum resultado encontrado. Tente outra região/categoria.\n");
        aviso_emergencia();
    } else {
        printf("%d resultado(s). (busca sequencial: %ld nós visitados)\n", achados, visitados);
    }
    return achados;
}

static void op_pesquisar(void) {
    if (lista_vazia_aviso()) return;
    const char *ops[] = {
        "Por título (texto)", "Por categoria", "Por região",
        "Por região + categoria (mais útil)", "Por bairro (texto)"
    };
    int modo = escolher("Pesquisar:", ops, 5);
    if (modo < 0) return;
    modo++;

    char texto[T_LINHA] = "";
    int ic = -1, ir = -1;
    if (modo == 1 || modo == 5) {
        ler_linha("Texto a procurar (Enter cancela): ", texto, sizeof texto);
        if (texto[0] == '\0') return;
    }
    if (modo == 2 || modo == 4) {
        ic = escolher("Categoria:", CAT_ROTULO_TODOS, N_CATS + 1);
        if (ic < 0) return;
    }
    if (modo == 3 || modo == 4) {
        ir = escolher("Região:", REGS, N_REGS_BUSCA);
        if (ir < 0) return;
    }
    pesquisar(modo, texto, ic, ir);
}

/* Pergunta criterio e ordem, ordena a lista (insertion sort) e informa as
 * comparacoes. Se imprimir_tudo, tambem lista todos os registros. */
static void op_ordenar_lista(int imprimir_tudo) {
    if (lista_vazia_aviso()) return;
    const char *crit[] = {
        "Título (A-Z)", "Id", "Capacidade (vagas)", "Região e bairro", "Categoria"
    };
    Comparador cmps[] = { cmp_titulo, cmp_id, cmp_capac, cmp_regiao, cmp_categoria };
    int c = escolher("Ordenar por:", crit, 5);
    if (c < 0) return;
    const char *ord[] = { "Crescente", "Decrescente" };
    int d = escolher("Ordem:", ord, 2);
    if (d < 0) return;

    g_passos = 0;
    ordenar_insertion(&g_lista, cmps[c], d == 1);
    if (imprimir_tudo) {
        printf("\n  id | categoria    | região | título (bairro)\n");
        for (Servico *p = g_lista.inicio; p != NULL; p = p->prox) imprimir_linha(p);
    }
    printf("%d registros ordenados por \"%s\" (insertion sort: %ld comparações).\n",
           g_lista.total, crit[c], g_passos);
}

static void op_listar_ordenado(void) { op_ordenar_lista(1); }

static void op_indicadores(void) {
    if (lista_vazia_aviso()) return;
    int por_cat[N_CATS + 1] = {0};
    long vagas[N_CATS + 1] = {0};
    int por_reg[N_REGS] = {0};
    int a_confirmar = 0;

    for (Servico *p = g_lista.inicio; p != NULL; p = p->prox) {     /* O(n) */
        int ic = N_CATS;                                            /* "outras" */
        for (int i = 0; i < N_CATS; i++)
            if (strcmp(p->categoria, CATS[i]) == 0) { ic = i; break; }
        por_cat[ic]++;
        vagas[ic] += p->capacidade;
        for (int i = 0; i < N_REGS; i++)
            if (iguais(p->regiao, REGS[i])) { por_reg[i]++; break; }
        if (contem(p->status, "confirmar")) a_confirmar++;
    }

    printf("\n=== INDICADORES ===\n");
    printf("Total de registros: %d\n\nPor categoria:\n", g_lista.total);
    for (int i = 0; i < N_CATS; i++)
        printf("  %-36s %3d registro(s)  | vagas informadas: %ld\n",
               CAT_ROTULO[i], por_cat[i], vagas[i]);
    if (por_cat[N_CATS] > 0) printf("  %-36s %3d registro(s)\n", "Outras", por_cat[N_CATS]);
    printf("\nPor região:\n");
    for (int i = 0; i < N_REGS; i++)
        printf("  %-8s %3d registro(s)\n", REGS[i], por_reg[i]);
    printf("\nRegistros que precisam de conferência (status \"confirmar\"): %d\n", a_confirmar);
}

/* ---- Operacoes exclusivas do perfil servidor ---- */

static void op_cadastrar(void) {
    long id;
    if (!ler_int("Id (inteiro positivo, Enter cancela): ", 1, 999999, &id)) return;
    if (buscar_id(&g_lista, (int)id, NULL) != NULL) {
        printf("Erro: já existe um registro com id %ld. Cadastro cancelado.\n", id);
        return;
    }
    Servico *s = novo_servico();
    s->id = (int)id;
    ler_obrigatorio("Título: ", s->titulo, sizeof s->titulo);

    int ic = -1;
    while (ic < 0) {
        ic = escolher("Categoria:", CAT_ROTULO, N_CATS);
        if (ic < 0) printf("  A categoria é obrigatória.\n");
    }
    copiar(s->categoria, sizeof s->categoria, CATS[ic]);

    int ir = -1;
    while (ir < 0) {
        ir = escolher("Região:", REGS, N_REGS);
        if (ir < 0) printf("  A região é obrigatória.\n");
    }
    copiar(s->regiao, sizeof s->regiao, REGS[ir]);

    ler_obrigatorio("Bairro: ", s->bairro, sizeof s->bairro);
    ler_obrigatorio("Descrição: ", s->descricao, sizeof s->descricao);
    ler_linha("Requisitos (Enter = Nenhum): ", s->requisitos, sizeof s->requisitos);
    if (s->requisitos[0] == '\0') copiar(s->requisitos, sizeof s->requisitos, "Nenhum");

    long cap = 0;
    if (!ler_int("Capacidade/vagas (0 a 100000, Enter = 0): ", 0, 100000, &cap)) cap = 0;
    s->capacidade = (int)cap;

    ler_obrigatorio("Fonte (link ou órgão): ", s->fonte, sizeof s->fonte);

    time_t agora = time(NULL);
    strftime(s->data_consulta, sizeof s->data_consulta, "%d/%m/%Y", localtime(&agora));
    copiar(s->status, sizeof s->status, "Cadastrado pelo servidor - confirmar");

    if (lista_inserir_fim(&g_lista, s) != 0) {          /* seguranca extra */
        free(s);
        printf("Erro: id duplicado.\n");
        return;
    }
    g_lista.alterada = 1;
    printf("Registro %d cadastrado com sucesso (total: %d).\n", s->id, g_lista.total);
}

static void atualizar_texto(const char *rotulo, char *campo, size_t n) {
    char buf[T_LINHA], prompt[T_LINHA];
    snprintf(prompt, sizeof prompt, "%s [%s]: ", rotulo, campo);
    ler_linha(prompt, buf, sizeof buf);
    if (buf[0] != '\0') copiar(campo, n, buf);
}

static void op_atualizar(void) {
    if (lista_vazia_aviso()) return;
    long id;
    if (!ler_int("Id do registro a atualizar (Enter cancela): ", 1, 999999, &id)) return;
    Servico *s = buscar_id(&g_lista, (int)id, NULL);
    if (s == NULL) { printf("Nenhum registro com id %ld.\n", id); return; }

    imprimir_detalhe(s);
    printf("Pressione Enter para manter o valor atual de cada campo.\n");
    atualizar_texto("Título", s->titulo, sizeof s->titulo);

    int ic = escolher("Nova categoria:", CAT_ROTULO, N_CATS);
    if (ic >= 0) copiar(s->categoria, sizeof s->categoria, CATS[ic]);
    int ir = escolher("Nova região:", REGS, N_REGS);
    if (ir >= 0) copiar(s->regiao, sizeof s->regiao, REGS[ir]);

    atualizar_texto("Bairro", s->bairro, sizeof s->bairro);
    atualizar_texto("Descrição", s->descricao, sizeof s->descricao);
    atualizar_texto("Requisitos", s->requisitos, sizeof s->requisitos);

    long cap;
    if (ler_int("Nova capacidade (0 a 100000, Enter mantém): ", 0, 100000, &cap))
        s->capacidade = (int)cap;
    atualizar_texto("Fonte", s->fonte, sizeof s->fonte);

    g_lista.alterada = 1;
    printf("Registro %d atualizado.\n", s->id);
}

static void op_excluir(void) {
    if (lista_vazia_aviso()) return;
    long id;
    if (!ler_int("Id do registro a excluir (Enter cancela): ", 1, 999999, &id)) return;
    Servico *s = buscar_id(&g_lista, (int)id, NULL);
    if (s == NULL) { printf("Nenhum registro com id %ld.\n", id); return; }

    imprimir_detalhe(s);
    char r[8];
    ler_linha("Confirma a exclusão? (s/n): ", r, sizeof r);
    if (r[0] == 's' || r[0] == 'S') {
        lista_remover(&g_lista, (int)id);
        g_lista.alterada = 1;
        printf("Registro %ld excluído (restam %d).\n", id, g_lista.total);
    } else {
        printf("Exclusão cancelada.\n");
    }
}

static void op_salvar(void) {
    if (salvar_csv(g_arquivo) == 0)
        printf("Base salva em \"%s\" (%d registros).\n", g_arquivo, g_lista.total);
    else
        printf("Erro ao salvar o arquivo \"%s\".\n", g_arquivo);
}

/* ------------------------------------------------------------------ */
/* Menus                                                               */
/* ------------------------------------------------------------------ */

static long ler_opcao_menu(int max) {
    long v;
    for (;;) {
        char buf[64];
        ler_linha("Opção: ", buf, sizeof buf);
        char *fim;
        v = strtol(buf, &fim, 10);
        if (buf[0] == '\0' || *fim != '\0') printf("  Opção inválida: digite um número do menu.\n");
        else if (v < 0 || v > max)           printf("  Opção inválida: escolha entre 0 e %d.\n", max);
        else return v;
    }
}

/* Tela de explicacao mostrada logo depois da escolha do perfil */
static void mostrar_explicacao(int servidor) {
    int por_cat[N_CATS] = {0};
    for (Servico *p = g_lista.inicio; p != NULL; p = p->prox)
        for (int i = 0; i < N_CATS; i++)
            if (strcmp(p->categoria, CATS[i]) == 0) { por_cat[i]++; break; }

    printf("\n==========================================================\n");
    printf(" COMO FUNCIONA\n");
    printf("==========================================================\n");
    if (servidor) {
        printf("Você é o responsável por manter o catálogo de pontos de apoio.\n");
        printf("Pode cadastrar, atualizar e excluir registros, além de pesquisar,\n");
        printf("ordenar e ver indicadores. Cada registro precisa ter fonte e data.\n");
    } else {
        printf("O Citizen Assist AI ajuda você a encontrar serviços de apoio em\n");
        printf("São Paulo. Você informa sua região e o tipo de serviço, e o sistema\n");
        printf("mostra os locais cadastrados, o que levar e a fonte da informação.\n");
    }
    printf("\nTipos de serviço cadastrados (%d registros no total):\n", g_lista.total);
    for (int i = 0; i < N_CATS; i++) {
        printf("  %d) %s - %d registro(s)\n", i + 1, CAT_ROTULO[i], por_cat[i]);
        printf("     %s\n", CAT_DESC[i]);
    }
    printf("\nAtenção: este sistema NÃO é canal oficial e os dados podem estar\n");
    printf("desatualizados; confira sempre a fonte de cada registro.\n");
    printf("Nenhum dado pessoal é coletado ou armazenado.\n");
    aviso_emergencia();
}

/* Fluxo guiado do cidadao: regiao -> tipo de servico -> resultados */
static void fluxo_cidadao(void) {
    mostrar_explicacao(0);
    if (lista_vazia_aviso()) return;

    for (;;) {
        printf("\n--- NOVA PESQUISA ---\n");
        int ir = escolher("Em qual região de São Paulo você está?", REGS, N_REGS_BUSCA);
        if (ir < 0) return;
        int ic = escolher("Que tipo de serviço você procura?", CAT_ROTULO_TODOS, N_CATS + 1);
        if (ic < 0) return;

        int mostrar = 1;
        for (;;) {
            if (mostrar) {
                printf("\nResultados - região: %s | tipo: %s\n", REGS[ir], CAT_ROTULO_TODOS[ic]);
                pesquisar(4, "", ic, ir);
            }
            mostrar = 0;
            printf("\nO que deseja fazer agora?\n");
            printf("1) Ver detalhes de um registro (digite o id)\n");
            printf("2) Ordenar estes resultados\n");
            printf("3) Buscar por nome ou bairro (texto)\n");
            printf("4) Nova pesquisa (outra região ou tipo)\n");
            printf("5) Ver indicadores do sistema\n");
            printf("0) Voltar ao menu inicial\n");
            long op = ler_opcao_menu(5);
            if (op == 0) return;
            if (op == 4) break;
            if (op == 1) {
                op_consultar_id();
            } else if (op == 2) {
                op_ordenar_lista(0);
                mostrar = 1;                /* reexibe os resultados ja ordenados */
            } else if (op == 3) {
                char texto[T_LINHA];
                ler_linha("Texto a procurar no nome ou bairro (Enter cancela): ", texto, sizeof texto);
                if (texto[0] != '\0') pesquisar(6, texto, -1, -1);
            } else {
                op_indicadores();
            }
        }
    }
}

static void menu_servidor(void) {
    for (;;) {
        printf("\n=== PERFIL SERVIDOR PÚBLICO ===\n");
        printf("1) Cadastrar registro\n");
        printf("2) Consultar registro por id\n");
        printf("3) Pesquisar\n");
        printf("4) Atualizar registro\n");
        printf("5) Excluir registro\n");
        printf("6) Listar ordenado\n");
        printf("7) Indicadores\n");
        printf("8) Salvar base no arquivo CSV\n");
        printf("0) Voltar\n");
        switch (ler_opcao_menu(8)) {
            case 1: op_cadastrar();        break;
            case 2: op_consultar_id();     break;
            case 3: op_pesquisar();        break;
            case 4: op_atualizar();        break;
            case 5: op_excluir();          break;
            case 6: op_listar_ordenado();  break;
            case 7: op_indicadores();      break;
            case 8: op_salvar();           break;
            case 0: return;
        }
    }
}

static int autenticar_servidor(void) {
    for (int t = 1; t <= MAX_TENTATIVAS; t++) {
        char senha[64];
        ler_linha("Senha do servidor: ", senha, sizeof senha);
        if (strcmp(senha, SENHA_SERVIDOR) == 0) {
            printf("Acesso liberado.\n");
            return 1;
        }
        printf("Senha incorreta (%d/%d).\n", t, MAX_TENTATIVAS);
    }
    printf("Número máximo de tentativas atingido. Voltando ao menu inicial.\n");
    return 0;
}

int main(int argc, char *argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    if (argc > 1) copiar(g_arquivo, sizeof g_arquivo, argv[1]);

    lista_iniciar(&g_lista);
    printf("==========================================================\n");
    printf(" CITIZEN ASSIST AI - Rede de Apoio Comunitário (São Paulo)\n");
    printf("==========================================================\n");

    if (carregar_csv(g_arquivo) < 0)
        printf("Aviso: arquivo \"%s\" não encontrado. Iniciando com a lista vazia.\n", g_arquivo);

    for (;;) {
        printf("\n=== QUEM É VOCÊ? ===\n");
        printf("1) Sou cidadão\n");
        printf("2) Sou servidor público (exige senha)\n");
        printf("0) Encerrar\n");
        long op = ler_opcao_menu(2);
        if (op == 1) {
            fluxo_cidadao();
        } else if (op == 2) {
            if (autenticar_servidor()) {
                mostrar_explicacao(1);
                menu_servidor();
            }
        } else {
            if (g_lista.alterada) {
                char r[8];
                ler_linha("Há alterações não salvas. Salvar agora? (s/n): ", r, sizeof r);
                if (r[0] == 's' || r[0] == 'S') op_salvar();
            }
            break;
        }
    }

    lista_liberar(&g_lista);
    printf("Programa encerrado. Até logo!\n");
    return 0;
}
