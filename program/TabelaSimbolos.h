#ifndef TABELA_SIMBOLOS_H
#define TABELA_SIMBOLOS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "AnalisadorLexico.h"
#include "AnalisadorLexico.h"
#include "basics.h"

typedef enum TipoSemantico {
    tipoIndefinido, tipoInteger, tipoDouble, tipoChar, tipoBooleano
} TipoSemantico;

typedef struct Simbolo {
    char nome[100];
    Token token;
    TipoSemantico tipo;
    int escopo;
    int deslocamento;
    void *valor;
    void (*mostrarValor)(void *valor);
} Simbolo;

typedef struct TabelaSimbolos {
    Simbolo *tabela;
    int tamanhoLogico;
    int tamanho;
    int ScopoAtual;
} TabelaSimbolos;

Simbolo GerarSimbolo(char *nome, Token token, TabelaSimbolos *tabela, void *valor);
void inicializarTabela(TabelaSimbolos *tabela);
bool InserirSimbolo(
    TabelaSimbolos *tabela,
    Simbolo simboloASerInseriddo,
    bool aumentarEscopo
);
bool RemoverUltimoSimbolo(TabelaSimbolos *tabela);
Simbolo *BuscarSimbolo(TabelaSimbolos *tabela, char *nome);
bool RemoverSimbolo(TabelaSimbolos *tabela, char *nome);
void ImprimirTabela(TabelaSimbolos *tabela);
void mostrarValorInt(void *valor);
void LiberarTabela(TabelaSimbolos *tabela);
void *AtualizarSimbolo(TabelaSimbolos *tabela,Simbolo sim);
void RemoverScopo(TabelaSimbolos *tabela, int escopo);
int CompareSimbolo(Simbolo a, Simbolo b);
static void _CalcularDeslocamento(TabelaSimbolos *tabela, Simbolo *simbolo);

#endif

