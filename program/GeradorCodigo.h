#ifndef GERADOR_CODIGO_H
#define GERADOR_CODIGO_H

#include <stdio.h>
#include "TabelaSimbolos.h"

/* Estado do gerador. O chamador define o arquivo e a tabela usados na compilação. */
extern FILE *arquivoCodigo;                 /* Destino onde as instruções serão escritas. */
extern TabelaSimbolos *tabelaCodigo;         /* Tabela usada para resolver escopo e deslocamento dos identificadores. */
extern unsigned long proximoRotulo;          /* Contador para criar rótulos únicos de desvio. */

/* Emite uma instrução e seus operandos no formato textual da máquina virtual. */
void EmitirInstrucao(const char *instrucao, const char *operandos);

/* Emite INPP para iniciar a execução do programa. */
void INPP(void);
/* Emite PARA para encerrar a execução do programa. */
void PARA(void);

/* Emite AMEM n para alocar n posições na pilha. */
void AMEM(int n);
/* Emite DMEM n para desalocar n posições da pilha. */
void DMEM(int n);

/* Emite CRCT para empilhar uma constante. */
void CRCT(long constante);
/* Emite CRVL para carregar o valor de uma variável no nível e deslocamento informados. */
void CRVL(int nivelLexico, int deslocamento);
/* Emite ARMZ para guardar o topo da pilha na variável indicada. */
void ARMZ(int nivelLexico, int deslocamento);
/* Emite CREN para empilhar o endereço da variável indicada (parâmetro por referência). */
void CREN(int nivelLexico, int deslocamento);
/* Emite CRVI para carregar o valor apontado por um parâmetro por referência. */
void CRVI(int nivelLexico, int deslocamento);
/* Emite ARMI para armazenar o topo no endereço indireto indicado pelo parâmetro. */
void ARMI(int nivelLexico, int deslocamento);

/* Emite SOMA para somar os dois valores do topo da pilha. */
void SOMA(void);
/* Emite SUBT para subtrair os dois valores do topo da pilha. */
void SUBT(void);
/* Emite MULT para multiplicar os dois valores do topo da pilha. */
void MULT(void);
/* Emite DIVI para dividir os dois valores do topo da pilha. */
void DIVI(void);
/* Emite INVR para inverter o sinal do valor no topo da pilha. */
void INVR(void);

/* Compara igualdade e empilha o resultado booleano. */
void CMIG(void);
/* Compara diferença e empilha o resultado booleano. */
void CMDG(void);
/* Compara menor ou igual e empilha o resultado booleano. */
void CMEG(void);
/* Compara maior ou igual e empilha o resultado booleano. */
void CMAG(void);
/* Compara menor e empilha o resultado booleano. */
void CMME(void);
/* Compara maior e empilha o resultado booleano. */
void CMMA(void);

/* Emite a negação lógica do valor no topo da pilha. */
void NEGA(void);
/* Emite a conjunção lógica dos dois valores do topo da pilha. */
void CONJ(void);
/* Emite a disjunção lógica dos dois valores do topo da pilha. */
void DISJ(void);

/* Emite uma declaração de rótulo para marcar a posição atual do código. */
void NADA(const char *rotulo);
/* Mantém DSVR na API C e emite DSVS no dialeto MEPA do projeto. */
void DSVR(const char *rotulo);
/* Emite DSVF para saltar ao rótulo informado quando o topo for falso. */
void DSVF(const char *rotulo);
/* Gera e retorna um identificador único para uso em rótulos de desvio. */
unsigned long GerarRotulo(void);

/* Emite CHPR para chamar uma rotina no nível léxico informado. */
void CHPR(const char *rotulo, int nivelLexico);
/* Emite ENPR para iniciar um quadro de sub-rotina no nível informado. */
void ENPR(int nivelLexico);
/* Emite IPVL para inicializar os parâmetros por valor do quadro. */
void IPVL(int nivelLexico, int quantidadeParametros);
/* Emite RTPR para retornar e remover os parâmetros indicados da pilha. */
void RTPR(int nivelLexico, int quantidadeParametros);

/* Emite a instrução de leitura e coloca o valor lido na pilha. */
void LEIT(void);
/* Emite a instrução de escrita para apresentar o valor no topo da pilha. */
void IMPR(void);

/* Localiza o identificador visível e emite CRVL ou CREN usando escopo e deslocamento. */
int EmitirReferenciaSimbolo(const char *nome, int carregarEndereco);

#endif
