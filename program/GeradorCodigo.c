#include "GeradorCodigo.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

FILE *arquivoCodigo = NULL;
TabelaSimbolos *tabelaCodigo = NULL;
unsigned long proximoRotulo = 0;

typedef struct ListaRotulos {
    char **itens;
    size_t quantidade;
    size_t capacidade;
} ListaRotulos;

static ListaRotulos rotulosDefinidos;
static ListaRotulos rotulosReferenciados;
static bool erroGeracao = false;

static void ErroGerador(const char *formato, ...)
{
    erroGeracao = true;
    va_list argumentos;
    va_start(argumentos, formato);
    fputs("Erro no gerador MEPA: ", stderr);
    vfprintf(stderr, formato, argumentos);
    fputc('\n', stderr);
    va_end(argumentos);
}

static void LiberarListaRotulos(ListaRotulos *lista)
{
    for(size_t i = 0; i < lista->quantidade; i++)
        free(lista->itens[i]);

    free(lista->itens);
    lista->itens = NULL;
    lista->quantidade = 0;
    lista->capacidade = 0;
}

static int IndiceRotulo(const ListaRotulos *lista, const char *rotulo)
{
    for(size_t i = 0; i < lista->quantidade; i++)
    {
        if(strcmp(lista->itens[i], rotulo) == 0)
            return (int)i;
    }
    return -1;
}

static bool AdicionarRotulo(ListaRotulos *lista, const char *rotulo)
{
    if(lista->quantidade == lista->capacidade)
    {
        size_t novaCapacidade = lista->capacidade == 0
            ? 8
            : lista->capacidade * 2;
        char **novosItens = realloc(
            lista->itens, novaCapacidade * sizeof(*novosItens)
        );
        if(novosItens == NULL)
            return false;

        lista->itens = novosItens;
        lista->capacidade = novaCapacidade;
    }

    size_t tamanho = strlen(rotulo) + 1;
    char *copia = malloc(tamanho);
    if(copia == NULL)
        return false;

    memcpy(copia, rotulo, tamanho);
    lista->itens[lista->quantidade++] = copia;
    return true;
}

static bool RegistrarReferencia(const char *rotulo)
{
    if(rotulo == NULL || rotulo[0] == '\0')
    {
        ErroGerador("destino de desvio inválido.");
        return false;
    }

    if(IndiceRotulo(&rotulosReferenciados, rotulo) >= 0)
        return true;

    if(!AdicionarRotulo(&rotulosReferenciados, rotulo))
    {
        ErroGerador("memória insuficiente ao registrar destino '%s'.", rotulo);
        return false;
    }
    return true;
}

static bool RegistrarDefinicao(const char *rotulo)
{
    if(rotulo == NULL || rotulo[0] == '\0')
    {
        ErroGerador("rótulo inválido.");
        return false;
    }

    if(IndiceRotulo(&rotulosDefinidos, rotulo) >= 0)
    {
        ErroGerador("rótulo '%s' definido mais de uma vez.", rotulo);
        return false;
    }

    if(!AdicionarRotulo(&rotulosDefinidos, rotulo))
    {
        ErroGerador("memória insuficiente ao definir '%s'.", rotulo);
        return false;
    }
    return true;
}

static void ReiniciarEstado(void)
{
    LiberarListaRotulos(&rotulosDefinidos);
    LiberarListaRotulos(&rotulosReferenciados);
    proximoRotulo = 0;
    erroGeracao = false;
}

static void EmitirPar(const char *instrucao, int primeiro, int segundo)
{
    char operandos[64];
    snprintf(operandos, sizeof(operandos), "%d, %d", primeiro, segundo);
    EmitirInstrucao(instrucao, operandos);
}

void EmitirInstrucao(const char *instrucao, const char *operandos)
{
    if(arquivoCodigo == NULL)
    {
        ErroGerador("arquivo de saída não foi configurado.");
        return;
    }

    if(instrucao == NULL || instrucao[0] == '\0')
    {
        ErroGerador("instrução vazia.");
        return;
    }

    if(operandos == NULL || operandos[0] == '\0')
        fprintf(arquivoCodigo, "%s\n", instrucao);
    else
        fprintf(arquivoCodigo, "%s %s\n", instrucao, operandos);

    if(ferror(arquivoCodigo))
        ErroGerador("falha ao escrever a instrução '%s'.", instrucao);
}

void INPP(void)
{
    ReiniciarEstado();
    EmitirInstrucao("INPP", NULL);
}

void PARA(void)
{
    for(size_t i = 0; i < rotulosReferenciados.quantidade; i++)
    {
        if(IndiceRotulo(&rotulosDefinidos, rotulosReferenciados.itens[i]) < 0)
            ErroGerador("destino '%s' não tem definição.",
                        rotulosReferenciados.itens[i]);
    }
    if(erroGeracao)
        exit(EXIT_FAILURE);
    EmitirInstrucao("PARA", NULL);
    if(erroGeracao)
        exit(EXIT_FAILURE);
}

void AMEM(int n)
{
    if(n < 0) { ErroGerador("AMEM recebeu quantidade negativa."); return; }
    char operando[32];
    snprintf(operando, sizeof(operando), "%d", n);
    EmitirInstrucao("AMEM", operando);
}

void DMEM(int n)
{
    if(n < 0) { ErroGerador("DMEM recebeu quantidade negativa."); return; }
    char operando[32];
    snprintf(operando, sizeof(operando), "%d", n);
    EmitirInstrucao("DMEM", operando);
}

void CRCT(long constante)
{
    char operando[48];
    snprintf(operando, sizeof(operando), "%ld", constante);
    EmitirInstrucao("CRCT", operando);
}

void CRVL(int nivelLexico, int deslocamento)
{
    EmitirPar("CRVL", nivelLexico, deslocamento);
}

void ARMZ(int nivelLexico, int deslocamento)
{
    EmitirPar("ARMZ", nivelLexico, deslocamento);
}

void CREN(int nivelLexico, int deslocamento)
{
    EmitirPar("CREN", nivelLexico, deslocamento);
}

void CRVI(int nivelLexico, int deslocamento)
{
    EmitirPar("CRVI", nivelLexico, deslocamento);
}

void ARMI(int nivelLexico, int deslocamento)
{
    EmitirPar("ARMI", nivelLexico, deslocamento);
}

void SOMA(void) { EmitirInstrucao("SOMA", NULL); }
void SUBT(void) { EmitirInstrucao("SUBT", NULL); }
void MULT(void) { EmitirInstrucao("MULT", NULL); }
void DIVI(void) { EmitirInstrucao("DIVI", NULL); }
void INVR(void) { EmitirInstrucao("INVR", NULL); }
void CMIG(void) { EmitirInstrucao("CMIG", NULL); }
void CMDG(void) { EmitirInstrucao("CMDG", NULL); }
void CMEG(void) { EmitirInstrucao("CMEG", NULL); }
void CMAG(void) { EmitirInstrucao("CMAG", NULL); }
void CMME(void) { EmitirInstrucao("CMME", NULL); }
void CMMA(void) { EmitirInstrucao("CMMA", NULL); }
void NEGA(void) { EmitirInstrucao("NEGA", NULL); }
void CONJ(void) { EmitirInstrucao("CONJ", NULL); }
void DISJ(void) { EmitirInstrucao("DISJ", NULL); }

void ENPR(int nivelLexico)
{
    char operando[32];
    snprintf(operando, sizeof(operando), "%d", nivelLexico);
    EmitirInstrucao("ENPR", operando);
}

void IPVL(int nivelLexico, int quantidadeParametros)
{
    EmitirPar("IPVL", nivelLexico, quantidadeParametros);
}

void NADA(const char *rotulo)
{
    if(!RegistrarDefinicao(rotulo) || arquivoCodigo == NULL)
        return;

    fprintf(arquivoCodigo, "%s: NADA\n", rotulo);
    if(ferror(arquivoCodigo))
        ErroGerador("falha ao escrever a definição de '%s'.", rotulo);
}

void DSVR(const char *rotulo)
{
    if(RegistrarReferencia(rotulo))
        EmitirInstrucao("DSVS", rotulo);
}

void DSVF(const char *rotulo)
{
    if(RegistrarReferencia(rotulo))
        EmitirInstrucao("DSVF", rotulo);
}

unsigned long GerarRotulo(void)
{
    return ++proximoRotulo;
}

void CHPR(const char *rotulo, int nivelLexico)
{
    if(rotulo == NULL || rotulo[0] == '\0')
    {
        ErroGerador("destino de CHPR inválido.");
        return;
    }

    if(RegistrarReferencia(rotulo))
    {
        char operandos[128];
        snprintf(operandos, sizeof(operandos), "%s, %d", rotulo, nivelLexico);
        EmitirInstrucao("CHPR", operandos);
    }
}

void RTPR(int nivelLexico, int quantidadeParametros)
{
    EmitirPar("RTPR", nivelLexico, quantidadeParametros);
}

void LEIT(void) { EmitirInstrucao("LEIT", NULL); }
void IMPR(void) { EmitirInstrucao("IMPR", NULL); }

int EmitirReferenciaSimbolo(const char *nome, int carregarEndereco)
{
    if(nome == NULL || nome[0] == '\0' || tabelaCodigo == NULL)
    {
        ErroGerador("identificador ou tabela de símbolos inválida.");
        return -1;
    }

    Simbolo *simbolo = BuscarSimbolo(tabelaCodigo, (char *)nome);
    if(simbolo == NULL)
    {
        ErroGerador("identificador '%s' não está visível.", nome);
        return -1;
    }

    bool retornoFuncao = simbolo->token == funcao && simbolo->escopoCorpo >= 0 &&
                         simbolo->deslocamento != -1;
    if(simbolo->token != variavel && !retornoFuncao)
    {
        ErroGerador("identificador '%s' não é uma variável.", nome);
        return -1;
    }

    int nivelLexico = retornoFuncao ? simbolo->escopoCorpo : simbolo->escopo;
    if(nivelLexico < 0 || nivelLexico > tabelaCodigo->ScopoAtual)
    {
        ErroGerador("nível léxico inválido para '%s'.", nome);
        return -1;
    }

    if(simbolo->modoPassagem == MODO_NOME)
    {
        ErroGerador("parâmetro por nome '%s' não tem execução suportada.", nome);
        return -1;
    }

    if(carregarEndereco)
    {
        if(simbolo->modoPassagem == MODO_REFERENCIA)
            CRVL(nivelLexico, simbolo->deslocamento);
        else
            CREN(nivelLexico, simbolo->deslocamento);
    }
    else if(simbolo->modoPassagem == MODO_REFERENCIA)
    {
        CRVI(nivelLexico, simbolo->deslocamento);
    }
    else
    {
        CRVL(nivelLexico, simbolo->deslocamento);
    }

    return 0;
}
