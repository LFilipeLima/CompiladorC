#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "TabelaSimbolos.h"

static TabelaSimbolos _AumentarTabela(TabelaSimbolos *tabela);
static void _CalcularDeslocamento(
    TabelaSimbolos *tabela,
    Simbolo *simbolo
);
static bool _OcupaPosicaoNoQuadro(const Simbolo *simbolo);

void inicializarTabela(TabelaSimbolos *tabela)
{
    tabela->tamanhoLogico = 0;
    tabela->tamanho = 100;
    tabela->ScopoAtual = 0;
    tabela->tabela = malloc((size_t)tabela->tamanho * sizeof(Simbolo));

    if(tabela->tabela == NULL)
        exit(EXIT_FAILURE);

    Simbolo read = GerarSimbolo("read", procedimento, tabela, NULL);
    Simbolo write = GerarSimbolo("write", procedimento, tabela, NULL);
    read.intrinseco = true;
    write.intrinseco = true;
    InserirSimbolo(tabela, read, false);
    InserirSimbolo(tabela, write, false);
}

Simbolo GerarSimbolo(
    char *nome,
    Token token,
    TabelaSimbolos *tabela,
    void *valor
)
{
    (void)tabela;

    if(nome == NULL || nome[0] == '\0' || strlen(nome) >= sizeof(((Simbolo *)0)->nome))
    {
        fprintf(stderr, "Erro: nome de símbolo inválido ou longo demais.\n");
        exit(EXIT_FAILURE);
    }

    Simbolo simbolo = {0};
    memcpy(simbolo.nome, nome, strlen(nome) + 1);
    simbolo.token = token;
    simbolo.tipo = tipoIndefinido;
    simbolo.escopo = -1;
    simbolo.deslocamento = -1;
    simbolo.modoPassagem = MODO_NAO_APLICAVEL;
    simbolo.escopoCorpo = -1;

    if(valor != NULL)
    {
        size_t tamanhoValor = strlen((const char *)valor) + 1;
        simbolo.valor = malloc(tamanhoValor);
        if(simbolo.valor == NULL)
            exit(EXIT_FAILURE);
        memcpy(simbolo.valor, valor, tamanhoValor);
    }

    return simbolo;
}

bool InserirSimbolo(
    TabelaSimbolos *tabela,
    Simbolo simboloASerInseriddo,
    bool aumentarEscopo
)
{
    if(tabela == NULL || tabela->tabela == NULL ||
       simboloASerInseriddo.nome[0] == '\0' ||
       memchr(simboloASerInseriddo.nome, '\0', sizeof(simboloASerInseriddo.nome)) == NULL)
        return false;

    int escopo = tabela->ScopoAtual;
    for(int i = tabela->tamanhoLogico - 1; i >= 0; i--)
    {
        if(tabela->tabela[i].escopo < escopo)
            break;
        if(tabela->tabela[i].escopo == escopo &&
           strcmp(tabela->tabela[i].nome, simboloASerInseriddo.nome) == 0)
            return false;
    }

    if(tabela->tamanhoLogico >= tabela->tamanho)
    {
        *tabela = _AumentarTabela(tabela);
        if(tabela->tabela == NULL)
            return false;
    }

    simboloASerInseriddo.escopo = escopo;
    _CalcularDeslocamento(tabela, &simboloASerInseriddo);
    tabela->tabela[tabela->tamanhoLogico++] = simboloASerInseriddo;
    if(aumentarEscopo)
        tabela->ScopoAtual++;

    return true;
}

static bool _OcupaPosicaoNoQuadro(const Simbolo *simbolo)
{
    /* Formais por valor viram slots locais; referências ficam na pilha de chamada. */
    return simbolo->token == variavel &&
           simbolo->modoPassagem != MODO_REFERENCIA;
}

static void _CalcularDeslocamento(TabelaSimbolos *tabela, Simbolo *simbolo)
{
    simbolo->deslocamento = -1;
    if(!_OcupaPosicaoNoQuadro(simbolo))
        return;

    int deslocamento = 0;
    for(int i = 0; i < tabela->tamanhoLogico; i++)
    {
        const Simbolo *anterior = &tabela->tabela[i];
        if(anterior->escopo == simbolo->escopo &&
           _OcupaPosicaoNoQuadro(anterior))
            deslocamento++;
    }

    simbolo->deslocamento = deslocamento;
}

int CompareSimbolo(Simbolo a, Simbolo b)
{
    return strcmp(a.nome, b.nome);
}

bool RegistrarParametroFormal(
    Simbolo *rotina,
    TipoSemantico tipo,
    ModoPassagem modoPassagem,
    int deslocamento
)
{
    /* -1 é provisório para referências; o parser define o offset após contar os formais. */
    if(rotina == NULL || deslocamento < -1)
        return false;

    ParametroFormal *novos = realloc(
        rotina->parametros,
        (size_t)(rotina->quantidadeParametros + 1) * sizeof(*novos)
    );
    if(novos == NULL)
        return false;

    rotina->parametros = novos;
    rotina->parametros[rotina->quantidadeParametros++] = (ParametroFormal){
        tipo, modoPassagem, deslocamento
    };
    return true;
}

static void _LiberarDadosSimbolo(Simbolo *simbolo)
{
    free(simbolo->valor);
    free(simbolo->parametros);
    simbolo->valor = NULL;
    simbolo->parametros = NULL;
    simbolo->quantidadeParametros = 0;
}

Simbolo *BuscarSimbolo(TabelaSimbolos *tabela, char *nome)
{
    if(tabela == NULL || tabela->tabela == NULL || nome == NULL)
        return NULL;

    for(int i = tabela->tamanhoLogico - 1; i >= 0; i--)
    {
        Simbolo *simbolo = &tabela->tabela[i];
        if(simbolo->escopo <= tabela->ScopoAtual &&
           strcmp(simbolo->nome, nome) == 0)
            return simbolo;
    }

    return NULL;
}

void *AtualizarSimbolo(TabelaSimbolos *tabela, Simbolo sim)
{
    Simbolo *visivel = BuscarSimbolo(tabela, sim.nome);
    if(visivel == NULL)
        return NULL;

    char *novoValor = NULL;
    if(sim.valor != NULL)
    {
        size_t tamanhoValor = strlen((const char *)sim.valor) + 1;
        novoValor = malloc(tamanhoValor);
        if(novoValor == NULL)
            exit(EXIT_FAILURE);
        memcpy(novoValor, sim.valor, tamanhoValor);
    }

    free(visivel->valor);
    visivel->valor = novoValor;
    visivel->tipo = sim.tipo;
    visivel->token = sim.token;
    visivel->modoPassagem = sim.modoPassagem;
    visivel->mostrarValor = sim.mostrarValor;
    return novoValor;
}

bool RemoverUltimoSimbolo(TabelaSimbolos *tabela)
{
    if(tabela == NULL || tabela->tamanhoLogico <= 0)
        return false;

    _LiberarDadosSimbolo(&tabela->tabela[tabela->tamanhoLogico - 1]);
    tabela->tamanhoLogico--;
    return true;
}

void RemoverScopo(TabelaSimbolos *tabela, int escopo)
{
    if(tabela == NULL || tabela->tabela == NULL || escopo < 0)
        return;

    for(int i = 0; i < tabela->tamanhoLogico;)
    {
        if(tabela->tabela[i].escopo == escopo)
        {
            _LiberarDadosSimbolo(&tabela->tabela[i]);
            for(int j = i; j < tabela->tamanhoLogico - 1; j++)
                tabela->tabela[j] = tabela->tabela[j + 1];
            tabela->tamanhoLogico--;
        }
        else
        {
            i++;
        }
    }

    if(tabela->ScopoAtual == escopo && tabela->ScopoAtual > 0)
        tabela->ScopoAtual--;
}

bool RemoverSimbolo(TabelaSimbolos *tabela, char *nome)
{
    Simbolo *simbolo = BuscarSimbolo(tabela, nome);
    if(simbolo == NULL)
        return false;

    int indice = (int)(simbolo - tabela->tabela);
    _LiberarDadosSimbolo(simbolo);
    for(int j = indice; j < tabela->tamanhoLogico - 1; j++)
        tabela->tabela[j] = tabela->tabela[j + 1];
    tabela->tamanhoLogico--;
    return true;
}

static TabelaSimbolos _AumentarTabela(TabelaSimbolos *tabela)
{
    TabelaSimbolos novaTabela = *tabela;
    novaTabela.tamanho = tabela->tamanho > 0 ? tabela->tamanho * 2 : 100;
    novaTabela.tabela = malloc((size_t)novaTabela.tamanho * sizeof(Simbolo));
    if(novaTabela.tabela == NULL)
        exit(EXIT_FAILURE);

    memcpy(
        novaTabela.tabela,
        tabela->tabela,
        (size_t)tabela->tamanhoLogico * sizeof(Simbolo)
    );
    free(tabela->tabela);
    return novaTabela;
}

void ImprimirTabela(TabelaSimbolos *tabela)
{
    printf("Tabela de Simbolos:\n");
    printf("Nome\t\tTipo\t\tEscopo\n");
    for(int i = 0; i < tabela->tamanhoLogico; i++)
    {
        printf(
            "%s\t\t%s\t\t%d\n",
            tabela->tabela[i].nome,
            tokenString[tabela->tabela[i].token],
            tabela->tabela[i].escopo
        );
    }
}

void LiberarTabela(TabelaSimbolos *tabela)
{
    if(tabela == NULL)
        return;

    for(int i = 0; i < tabela->tamanhoLogico; i++)
        _LiberarDadosSimbolo(&tabela->tabela[i]);
    free(tabela->tabela);
    tabela->tabela = NULL;
    tabela->tamanhoLogico = 0;
    tabela->tamanho = 0;
}
